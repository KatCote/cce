#include "render.h"
#include "../engine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <GL/gl.h>
#include <stdarg.h>
#include <string.h>

typedef struct {
    float x, y, radius;
} CloudBlob;

pct frame[1920][1080];

void cce_setup_2d_projection(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void cce_draw_grid(int x0, int y0, int x1, int y1, int pixel_size, int offset_x, int offset_y, CCE_Palette palette)
{
    if ((x0 > x1) || (y0 > y1)) { cce_printf("Wrong grid size!\n"); ERRLOG; return; }
    if (pixel_size < 1) { cce_printf("Wrong pixel size!\n"); ERRLOG; return; }

    int cols = (x1 + pixel_size - 1) / pixel_size;
    int rows = (y1 + pixel_size - 1) / pixel_size;

    CCE_Color color;
    
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            int pixel_x = x0 + col * pixel_size;
            int pixel_y = y0 + row * pixel_size;
            
            color = cce_get_color(pixel_x, pixel_y, offset_x, offset_y, palette);
            
            draw_pixel(pixel_x, pixel_y, pixel_size, color.r, color.g, color.b, color.a);
        }
    }

    color = cce_get_color(x1 - pixel_size + 1, y1 - pixel_size + 1, offset_x, offset_y, palette);
    draw_pixel(x1, y1, pixel_size, color.r, color.g, color.b, color.a);
}

void cce_draw_cloud(int center_x, int center_y, int offset_x, int offset_y, float size, int seed)
{
    int num_blobs = 8 + 1 % 6;
    CloudBlob blobs[num_blobs];
    
    for (int i = 0; i < num_blobs; i++)
    {
        blobs[i].x = center_x + (seed*i % (int)(size * 1.5f) - size * 0.75f);
        blobs[i].y = center_y + (seed*i % (int)(size) - size * 0.5f);
        blobs[i].radius = size * (0.3f + (seed*i % 70) / 100.0f);
    }
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    CCE_Color color;
    color = cce_get_color(center_x, center_y, offset_x, offset_y, DefaultCloud);
    glColor4ub(color.r, color.g, color.b, color.a);
    
    for (int i = 0; i < num_blobs; i++)
    {
        color = cce_get_color(blobs[i].x, blobs[i].y, 0, 0, DefaultCloud);
        glColor4ub(color.r, color.g, color.b, color.a);
        draw_filled_circle(blobs[i].x + offset_x, blobs[i].y + offset_y, blobs[i].radius, 32);
    }
    
    glDisable(GL_BLEND);
}

void draw_filled_circle(float center_x, float center_y, float radius, int pixel_size)
{
    int r = (int)radius;
    int cx = (int)center_x;
    int cy = (int)center_y;
    int r2 = r * r;
    
    glBegin(GL_QUADS);
    
    for (int y = -r; y <= r; y += pixel_size) {
        for (int x = -r; x <= r; x += pixel_size) {

            int center_dist = x*x + y*y;
            if (center_dist <= r2) {
                float px = cx + x - pixel_size/2;
                float py = cy + y - pixel_size/2;
                
                glVertex2f(px, py);
                glVertex2f(px + pixel_size, py);
                glVertex2f(px + pixel_size, py + pixel_size);
                glVertex2f(px, py + pixel_size);
            }
        }
    }
    glEnd();
}

float procedural_noise(int x, int y, int seed)
{
    uint32_t n = (x * 1836311903) ^ (y * 2971215073) ^ (seed * 1073741827);
    n = (n >> 13) ^ n;
    n = (n * (n * n * 60493 + 19990303) + 1376312589);
    return (float)(n & 0x7FFFFFFF) / 2147483647.0f;
}

void draw_pixel(int x, int y, int size, pct r, pct g, pct b, pct a)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + size, y);
    glVertex2i(x + size, y + size);
    glVertex2i(x, y + size);
    glEnd();
    glDisable(GL_BLEND);
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
            chunk->data[index] = color;
            
            // Помечаем весь чанк как грязный
            chunk->dirty = true;
        }
    }
}

void update_dirty_chunks(CCE_Layer* layer)
{
    glBindTexture(GL_TEXTURE_2D, layer->texture);
    
    int updated = 0;
    for (int y = 0; y < layer->chunk_count_y; y++) {
        for (int x = 0; x < layer->chunk_count_x; x++) {
            CCE_Chunk* chunk = layer->chunks[y][x];
            
            if (chunk->dirty && chunk->visible) {
                // Вычисляем экранные координаты чанка
                int screen_x = x * layer->chunk_size;
                int screen_y = y * layer->chunk_size;
                
                // Копируем чанк в текстуру
                glTexSubImage2D(GL_TEXTURE_2D, 0,
                               screen_x, screen_y,
                               chunk->w, chunk->h,
                               GL_RGBA, GL_UNSIGNED_BYTE,
                               chunk->data);
                
                chunk->dirty = false;  // Сбросить флаг
                updated++;
            }
        }
    }

    if (updated > 0 && CCE_DEBUG == 1)
    { cce_printf("Dirty chunks updated: %d on %s\n", updated, layer->name); }
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
    
    // Включаем текстуры и блендинг ОДИН РАЗ для всех слоёв
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    for (int i = 0; i < count; i++) {
        if (!layers[i] || !layers[i]->enabled) continue;
        
        // Обновляем только грязные чанки этого слоя
        update_dirty_chunks(layers[i]);
        
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

