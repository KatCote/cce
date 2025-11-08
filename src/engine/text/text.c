#include "text.h"
#include "../engine.h"

#include <cce.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../external/stb_truetype.h"

#ifndef FONT_TTF_H
#define FONT_TTF_H

struct TTF_Font {
    GLuint texture_id;
    int texture_width;
    int texture_height;
    stbtt_bakedchar* char_data;
    float font_size;
    float scale;
    int first_char;
    int num_chars;
};

#endif

TTF_Font* ttf_font_load(const char* filename, float font_size)
{
    FILE* font_file = fopen(filename, "rb");
    if (!font_file) {
        cce_printf("Failed to open font file: %s\n", filename);
        return NULL;
    }
    
    fseek(font_file, 0, SEEK_END);
    long file_size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);
    
    unsigned char* ttf_data = malloc(file_size);
    fread(ttf_data, 1, file_size, font_file);
    fclose(font_file);
    
    TTF_Font* font = malloc(sizeof(TTF_Font));
    font->font_size = font_size;
    font->first_char = 32;
    font->num_chars = 96;
    
    font->char_data = malloc(sizeof(stbtt_bakedchar) * font->num_chars);
    
    font->texture_width = 512;
    font->texture_height = 512;
    
    unsigned char* temp_bitmap = malloc(font->texture_width * font->texture_height);
    
    int result = stbtt_BakeFontBitmap(ttf_data, 0, font_size, 
                                     temp_bitmap, font->texture_width, font->texture_height,
                                     font->first_char, font->num_chars, 
                                     font->char_data);
    
    if (result <= 0) {
        cce_printf("Failed to bake font bitmap. Texture too small?\n");
        free(ttf_data);
        free(temp_bitmap);
        free(font->char_data);
        free(font);
        return NULL;
    }
    
    unsigned char* rgba_bitmap = malloc(font->texture_width * font->texture_height * 4);
    
    for (int i = 0; i < font->texture_width * font->texture_height; i++) {
        rgba_bitmap[i * 4 + 0] = 255;
        rgba_bitmap[i * 4 + 1] = 255;
        rgba_bitmap[i * 4 + 2] = 255;
        rgba_bitmap[i * 4 + 3] = temp_bitmap[i];
    }
    
    glGenTextures(1, &font->texture_id);
    glBindTexture(GL_TEXTURE_2D, font->texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, font->texture_width, font->texture_height, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_bitmap);
    
    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf_data, 0);
    font->scale = stbtt_ScaleForPixelHeight(&info, font_size);
    
    // Очистка временных данных
    free(ttf_data);
    free(temp_bitmap);
    free(rgba_bitmap);
    
    return font;
}

void ttf_font_free(TTF_Font* font)
{
    if (font) {
        glDeleteTextures(1, &font->texture_id);
        free(font->char_data);
        free(font);
    }
}

void ttf_render_text(TTF_Font* font, const char* text, float x, float y, Palette palette, ...)
{
    if (!font || !text) return;

    Color color;

    if (palette != Manual)
    {
        color = cce_get_color(0, 0, 0, 0, palette);
    }
    else
    {
        va_list rgb;
        va_start(rgb, palette);
        color.r = va_arg(rgb, double);
        color.g = va_arg(rgb, double);
        color.b = va_arg(rgb, double);
        va_end(rgb);
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, font->texture_id);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glColor3f(color.r, color.g, color.b);
    
    float start_x = x;
    
    glBegin(GL_QUADS);
    
    while (*text) {
        if (*text == '\n') {
            y += font->font_size;
            x = start_x;
            text++;
            continue;
        }
        
        int char_index = *text - font->first_char;
        if (char_index < 0 || char_index >= font->num_chars) {
            text++;
            continue;
        }
        
        stbtt_bakedchar* b = &font->char_data[char_index];
        
        float x0 = x + b->xoff;
        float y0 = y + b->yoff;
        float x1 = x0 + b->x1 - b->x0;
        float y1 = y0 + b->y1 - b->y0;
        
        float s0 = b->x0 / (float)font->texture_width;
        float t0 = b->y0 / (float)font->texture_height;
        float s1 = b->x1 / (float)font->texture_width;
        float t1 = b->y1 / (float)font->texture_height;
        
        glTexCoord2f(s0, t0); glVertex2f(x0, y0);
        glTexCoord2f(s1, t0); glVertex2f(x1, y0);
        glTexCoord2f(s1, t1); glVertex2f(x1, y1);
        glTexCoord2f(s0, t1); glVertex2f(x0, y1);
        
        x += b->xadvance;
        text++;
    }
    
    glEnd();
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

float ttf_text_width(TTF_Font* font, const char* text)
{
    if (!font || !text) return 0;
    
    float width = 0;
    
    while (*text) {
        if (*text == '\n') break;
        
        int char_index = *text - font->first_char;
        if (char_index >= 0 && char_index < font->num_chars) {
            stbtt_bakedchar* b = &font->char_data[char_index];
            width += b->xadvance;
        }
        text++;
    }
    
    return width;
}
