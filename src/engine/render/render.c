/*
===========================================================================
MIT License

Copyright (c) 2026 Stepan Pukhovskiy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
===========================================================================
*/

#include "render.h"
#include "../engine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <GL/gl.h>
#include <stdarg.h>
#include <string.h>

// Определения для PBO констант
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif

#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif

// Прямые объявления функций PBO (OpenGL 1.5+)
// Эти функции должны быть доступны через динамическую загрузку или напрямую
extern void glGenBuffers(GLsizei n, GLuint *buffers);
extern void glDeleteBuffers(GLsizei n, const GLuint *buffers);
extern void glBindBuffer(GLenum target, GLuint buffer);
extern void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
extern void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);

pct frame[1920][1080];

void cce_setup_2d_projection(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


float procedural_noise(int x, int y, int seed)
{
    uint32_t n = (x * 1836311903) ^ (y * 2971215073) ^ (seed * 1073741827);
    n = (n >> 13) ^ n;
    n = (n * (n * n * 60493 + 19990303) + 1376312589);
    return (float)(n & 0x7FFFFFFF) / 2147483647.0f;
}

CCE_Layer* create_layer(int screen_w, int screen_h, char * name)
{
    CCE_Layer* layer = malloc(sizeof(CCE_Layer));
    layer->scr_w = screen_w;
    layer->scr_h = screen_h;
    layer->chunk_size = CHUNK_SIZE;
    layer->enabled = true;

    // Округление вверх для количества чанков
    layer->chunk_count_x = (screen_w + CHUNK_SIZE - 1) / CHUNK_SIZE;
    layer->chunk_count_y = (screen_h + CHUNK_SIZE - 1) / CHUNK_SIZE;

    if (name) {
        layer->name = malloc(strlen(name) + 1);
        strcpy(layer->name, name);
    } else {
        layer->name = malloc(1);
        layer->name[0] = '\0';
    }

    // Выделяем память под массив указателей на строки чанков
    layer->chunks = malloc(layer->chunk_count_y * sizeof(CCE_Chunk**));
    //layer->name = malloc(strlen(name) * sizeof(char));

    cce_printf("New Layer: screen %dx%d, chunks %dx%d, name \"%s\"\n", 
        screen_w, screen_h, layer->chunk_count_x, layer->chunk_count_y, layer->name);

    for (int y = 0; y < layer->chunk_count_y; y++) {
        // Выделяем строку чанков
        layer->chunks[y] = malloc(layer->chunk_count_x * sizeof(CCE_Chunk*));
      
        for (int x = 0; x < layer->chunk_count_x; x++) {
            CCE_Chunk* chunk = (CCE_Chunk*)malloc(sizeof(CCE_Chunk));
            chunk->x = x;
            chunk->y = y;
            
            // Размер чанка (последний в строке/столбце может быть меньше)
            chunk->w = (x == layer->chunk_count_x - 1) ? 
                screen_w - x * CHUNK_SIZE : CHUNK_SIZE;
            chunk->h = (y == layer->chunk_count_y - 1) ? 
                screen_h - y * CHUNK_SIZE : CHUNK_SIZE;
            
            // Выделяем и очищаем память под пиксели
            chunk->data = malloc(chunk->w * chunk->h * sizeof(CCE_Color));
            memset(chunk->data, 0, chunk->w * chunk->h * sizeof(CCE_Color));
            
            chunk->dirty = true;  // Помечаем как грязный изначально
            chunk->visible = true;
            
            layer->chunks[y][x] = chunk;
            
            // Отладка
            cce_printf("New Chunk [%d][%d]: %dx%d pixels\n", 
                   y, x, chunk->w, chunk->h);
        }
    }

    // Создаём OpenGL текстуру
    glGenTextures(1, &layer->texture);
    glBindTexture(GL_TEXTURE_2D, layer->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_w, screen_h, 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Инициализируем PBO для асинхронной загрузки
    layer->pbo_size = CHUNK_SIZE * CHUNK_SIZE * 4;  // RGBA = 4 байта на пиксель
    layer->current_pbo_index = 0;
    
    glGenBuffers(2, layer->pbo_ids);
    
    // Инициализируем оба PBO
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, layer->pbo_ids[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, layer->pbo_size, NULL, GL_STREAM_DRAW);
    }
    
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);  // Отвязываем

    return layer;
}

void set_pixel(CCE_Layer* layer, int screen_x, int screen_y, CCE_Color color)
{
    // Проверяем границы экрана
    if (screen_x < 0 || screen_x >= layer->scr_w || 
        screen_y < 0 || screen_y >= layer->scr_h) {
        return;
    }
    
    // Определяем, какой чанк
    int chunk_x = screen_x / layer->chunk_size;
    int chunk_y = screen_y / layer->chunk_size;
    
    // Проверяем границы чанков
    if (chunk_x >= 0 && chunk_x < layer->chunk_count_x && 
        chunk_y >= 0 && chunk_y < layer->chunk_count_y) {
        
        CCE_Chunk* chunk = layer->chunks[chunk_y][chunk_x];
        
        // Локальные координаты внутри чанка
        int local_x = screen_x % layer->chunk_size;
        int local_y = screen_y % layer->chunk_size;
        
        // Проверяем границы внутри чанка (для последнего чанка в строке/столбце)
        if (local_x >= 0 && local_x < chunk->w && 
            local_y >= 0 && local_y < chunk->h) {
            
            // Записываем пиксель
            int index = local_y * chunk->w + local_x;
            
            // Проверяем, действительно ли изменился пиксель
            CCE_Color old_color = chunk->data[index];
            if (old_color.r == color.r && old_color.g == color.g && 
                old_color.b == color.b && old_color.a == color.a) {
                return;  // Пиксель не изменился, пропускаем
            }
            
            chunk->data[index] = color;
            
            // Помечаем весь чанк как грязный
            chunk->dirty = true;
        }
    }
}

void set_pixel_rect(CCE_Layer* layer, int x0, int y0, int x1, int y1, CCE_Color color)
{
    // Нормализуем координаты
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    
    // Ограничиваем границы
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= layer->scr_w) x1 = layer->scr_w - 1;
    if (y1 >= layer->scr_h) y1 = layer->scr_h - 1;
    
    if (x0 > x1 || y0 > y1) return;
    
    // Определяем затронутые чанки
    int chunk_x0 = x0 / layer->chunk_size;
    int chunk_y0 = y0 / layer->chunk_size;
    int chunk_x1 = x1 / layer->chunk_size;
    int chunk_y1 = y1 / layer->chunk_size;
    
    // Заполняем область батчем
    for (int cy = chunk_y0; cy <= chunk_y1; cy++) {
        for (int cx = chunk_x0; cx <= chunk_x1; cx++) {
            if (cx < 0 || cx >= layer->chunk_count_x || 
                cy < 0 || cy >= layer->chunk_count_y) continue;
            
            CCE_Chunk* chunk = layer->chunks[cy][cx];
            
            // Вычисляем область пересечения
            int chunk_screen_x = cx * layer->chunk_size;
            int chunk_screen_y = cy * layer->chunk_size;
            
            int local_x0 = (x0 > chunk_screen_x) ? (x0 - chunk_screen_x) : 0;
            int local_y0 = (y0 > chunk_screen_y) ? (y0 - chunk_screen_y) : 0;
            int local_x1 = (x1 < chunk_screen_x + chunk->w) ? 
                           (x1 - chunk_screen_x) : (chunk->w - 1);
            int local_y1 = (y1 < chunk_screen_y + chunk->h) ? 
                           (y1 - chunk_screen_y) : (chunk->h - 1);
            
            // Заполняем область батчем
            for (int ly = local_y0; ly <= local_y1; ly++) {
                for (int lx = local_x0; lx <= local_x1; lx++) {
                    int index = ly * chunk->w + lx;
                    chunk->data[index] = color;
                }
            }
            
            chunk->dirty = true;
        }
    }
}


void update_dirty_chunks(CCE_Layer* layer)
{
    if (!layer) return;
    
    glBindTexture(GL_TEXTURE_2D, layer->texture);
    
    int updated = 0;
    
    // Собираем список грязных чанков
    CCE_Chunk* dirty_chunks[256];  // Максимум 256 чанков за кадр
    int dirty_count = 0;
    
    for (int y = 0; y < layer->chunk_count_y && dirty_count < 256; y++) {
        for (int x = 0; x < layer->chunk_count_x && dirty_count < 256; x++) {
            CCE_Chunk* chunk = layer->chunks[y][x];
            
            if (chunk->dirty && chunk->visible) {
                dirty_chunks[dirty_count++] = chunk;
            }
        }
    }
    
    // Обновляем чанки через PBO (асинхронно)
    for (int i = 0; i < dirty_count; i++) {
        CCE_Chunk* chunk = dirty_chunks[i];
        
        int screen_x = chunk->x * layer->chunk_size;
        int screen_y = chunk->y * layer->chunk_size;
        
        int chunk_data_size = chunk->w * chunk->h * 4;  // RGBA
        
        // Если размер чанка больше PBO, используем прямой метод
        if (chunk_data_size > layer->pbo_size) {
            // Fallback на синхронный метод для больших чанков
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                           screen_x, screen_y,
                           chunk->w, chunk->h,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           chunk->data);
        } else {
            // Переключаемся на следующий PBO (двойная буферизация)
            layer->current_pbo_index = (layer->current_pbo_index + 1) % 2;
            int pbo_index = layer->pbo_ids[layer->current_pbo_index];
            
            // Привязываем PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_index);
            
            // Записываем данные в PBO
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, chunk_data_size, chunk->data);
            
            // Загружаем из PBO в текстуру (асинхронная операция!)
            // offset = 0, потому что мы записали данные в начало PBO
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                           screen_x, screen_y,
                           chunk->w, chunk->h,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           0);  // offset = 0, данные берутся из привязанного PBO
            
            // Отвязываем PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        
        chunk->dirty = false;
        updated++;
    }

    if (updated > 0 && CCE_DEBUG == 1) {
        cce_printf("Dirty chunks updated: %d/%d on %s\n", 
                   updated, dirty_count, layer->name);
    }
}

void render_layer(CCE_Layer* layer)
{
    // Обновляем только грязные чанки
    update_dirty_chunks(layer);
    
    // Включаем текстуры
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, layer->texture);
    
    // Рисуем всю текстуру одним квадом В ПИКСЕЛЬНЫХ КООРДИНАТАХ
    // (0,0) - верхний левый, (width,height) - нижний правый
    glBegin(GL_QUADS);
    // Верхний левый
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    // Верхний правый
    glTexCoord2f(1.0f, 0.0f); glVertex2f((float)layer->scr_w, 0.0f);
    // Нижний правый
    glTexCoord2f(1.0f, 1.0f); glVertex2f((float)layer->scr_w, (float)layer->scr_h);
    // Нижний левый
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, (float)layer->scr_h);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

void render_pie(CCE_Layer** layers, int count)
{
    if (!layers || count <= 0) return;
    
    // Обновляем грязные чанки для всех слоев ПЕРЕД рендерингом
    for (int i = 0; i < count; i++) {
        if (!layers[i] || !layers[i]->enabled) continue;
        update_dirty_chunks(layers[i]);
    }
    
    // Включаем текстуры и блендинг ОДИН РАЗ для всех слоёв
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Теперь рендерим все слои
    for (int i = 0; i < count; i++) {
        if (!layers[i] || !layers[i]->enabled) continue;
        
        // Привязываем текстуру слоя
        glBindTexture(GL_TEXTURE_2D, layers[i]->texture);
        
        // Рисуем слой
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f((float)layers[i]->scr_w, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f((float)layers[i]->scr_w, (float)layers[i]->scr_h);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, (float)layers[i]->scr_h);
        glEnd();
    }
    
    // Выключаем после отрисовки всех слоёв
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void destroy_layer(CCE_Layer* layer)
{
    if (!layer) return;
    
    // Удаляем PBO
    if (layer->pbo_ids[0] != 0 || layer->pbo_ids[1] != 0) {
        glDeleteBuffers(2, layer->pbo_ids);
    }
    
    for (int y = 0; y < layer->chunk_count_y; y++) {
        for (int x = 0; x < layer->chunk_count_x; x++) {
            free(layer->chunks[y][x]->data);
            free(layer->chunks[y][x]);
        }
        free(layer->chunks[y]);
    }
    free(layer->chunks);

    cce_printf("Destroying Layer: name \"%s\"\n", layer->name);

    if (layer->name) { free(layer->name); }
    
    glDeleteTextures(1, &layer->texture);
    free(layer);
}

void test_simple()
{
    // Тест 1: Простая текстура без чанков
    int w = 550, h = 550;
    CCE_Color* pixels = malloc(w * h * sizeof(CCE_Color));
    
    // Заливаем белым
    for (int i = 0; i < w * h; i++) {
        pixels[i] = cce_get_color(255, 255, 255, 255, DefaultLight);
    }
    
    // Красный квадрат 4x4 в центре
    int center_x = w/2, center_y = h/2;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int idx = (center_y + dy) * w + (center_x + dx);
            pixels[idx] = cce_get_color(255, 0, 0, 255, Red);
        }
    }
    
    // Создаём текстуру
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // Включаем текстуру
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Рисуем в ПИКСЕЛЬНЫХ координатах!
    // Размещаем в центре экрана 1920x1080
    float x0 = (1920 - w) / 2.0f;
    float y0 = (1080 - h) / 2.0f;
    float x1 = x0 + w;
    float y1 = y0 + h;
    
    glBegin(GL_QUADS);
    // Текстурные координаты: (0,0) - левый нижний, (1,1) - правый верхний
    // Вершинные координаты: в пикселях от (0,0) до (1920,1080)
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y0);  // левый верхний
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y0);  // правый верхний
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y1);  // правый нижний
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y1);  // левый нижний
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &texture);
    free(pixels);
}

CCE_Color cce_get_color(int pos_x, int pos_y, int offset_x, int offset_y, CCE_Palette palette, ...)
{
    CCE_Color ret;
    pct noise = (pct) ((procedural_noise(pos_x + offset_x, pos_y + offset_y, engine_seed + palette)) * 255);

    switch (palette)
    {
        case Red:
            ret.r = 255;
            ret.g = 0;
            ret.b = 0;
            ret.a = 255;
            break;

        case Green:
            ret.r = 0;
            ret.g = 255;
            ret.b = 0;
            ret.a = 255;
            break;

        case Blue:
            ret.r = 0;
            ret.g = 0;
            ret.b = 255;
            ret.a = 255;
            break;
            
        case Manual:
            va_list rgb;
            va_start(rgb, palette);
            ret.r = (pct) va_arg(rgb, int);
            ret.g = (pct) va_arg(rgb, int);
            ret.b = (pct) va_arg(rgb, int);
            ret.a = (pct) va_arg(rgb, int);
            va_end(rgb);
            break;

        case DefaultGrass:
            noise = noise < 80 ? 80: noise;
            noise = noise > 255 ? 255: noise;
            noise = COMPRESS(noise, 80, 150);
            ret.r = noise * 0.7f;
            ret.g = noise;
            ret.b = noise * 0.1f;
            ret.a = 255;
            break;

        case DefaultStone:
            noise = noise < 120 ? 120: noise;
            noise = noise > 255 ? 255: noise;
            ret.r = noise * 0.4f;
            ret.g = noise * 0.4f;
            ret.b = noise * 0.4f;
            ret.a = 255;
            break;

        case DefaultDark:
            ret.r = 20u;
            ret.g = 20u;
            ret.b = 20u;
            ret.a = 255;
            break;

        case DefaultLight:
            ret.r = 225u;
            ret.g = 225u;
            ret.b = 225u;
            ret.a = 255;
            break;

        case DefaultLeaves:
            // if (noise < 0.1f) { noise = 0.2f; }
            // if (noise < 0.5f) { noise = noise * 5.0f; }
            // if (noise >= 1.0f) { noise = 0.6f; }
            // ret.r = noise - 0.4f;
            // ret.g = noise - 0.2f;
            // ret.b = noise - 0.9f;
            // ret.a = 0.85f;

            ret.r = 255;
            ret.g = 255;
            ret.b = 255;
            ret.a = 255;

            break;

        case DefaultCloud:
            ret.r = noise;
            ret.g = noise;
            ret.b = noise;
            ret.a = 80u;
            break;

        case Empty:
            ret.r = 0;
            ret.g = 0;
            ret.b = 0;
            ret.a = 0;
            break;

        default:
            ret.r = 0;
            ret.g = 0;
            ret.b = 0;
            ret.a = 255;
            break;
    }
    return ret;
}

