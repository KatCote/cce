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

#include "text.h"
#include "../engine.h"

#include <cce.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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
    unsigned char* ttf_data;
    long ttf_data_size;

    // Cached font info to avoid stbtt_InitFont on every call.
    stbtt_fontinfo info;
    int info_initialized;

    // Scratch buffer for glyph rasterization (avoids per-glyph malloc/free).
    unsigned char* glyph_scratch;
    size_t glyph_scratch_size;
};

#endif

TTF_Font* cce_font_load(const char* filename, float font_size)
{
    FILE* font_file = fopen(filename, "rb");
    if (!font_file) {
        printf("Failed to open font file: %s\n", filename);
        return NULL;
    }
    
    fseek(font_file, 0, SEEK_END);
    long file_size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);
    
    unsigned char* ttf_data = malloc(file_size);
    fread(ttf_data, 1, file_size, font_file);
    fclose(font_file);
    
    TTF_Font* font = malloc(sizeof(TTF_Font));
    memset(font, 0, sizeof(TTF_Font));
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
        printf("Failed to bake font bitmap. Texture too small?\n");
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
    
    if (!stbtt_InitFont(&font->info, ttf_data, 0)) {
        printf("Failed to init font\n");
        free(ttf_data);
        free(temp_bitmap);
        free(rgba_bitmap);
        free(font->char_data);
        free(font);
        return NULL;
    }
    font->info_initialized = 1;
    font->scale = stbtt_ScaleForPixelHeight(&font->info, font_size);
    
    // Store TTF data for rasterization
    font->ttf_data = ttf_data;
    font->ttf_data_size = file_size;

    font->glyph_scratch = NULL;
    font->glyph_scratch_size = 0;
    
    free(temp_bitmap);
    free(rgba_bitmap);
    
    return font;
}

void cce_font_free(TTF_Font* font)
{
    if (font) {
        glDeleteTextures(1, &font->texture_id);
        free(font->char_data);
        if (font->glyph_scratch) {
            free(font->glyph_scratch);
        }
        if (font->ttf_data) {
            free(font->ttf_data);
        }
        free(font);
    }
}

float cce_text_width(TTF_Font* font, const char* text, float scale)
{
    if (!font || !text || !font->ttf_data) return 0;
    if (*text == '\0') return 0;
    if (!font->info_initialized) return 0;

    float actual_scale = font->scale * scale;
    float width = 0.0f;

    while (*text) {
        if (*text == '\n') break;

        int codepoint = (unsigned char)*text;
        int advance_width = 0;
        int left_side_bearing = 0;
        stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance_width, &left_side_bearing);

        int next_codepoint = (unsigned char)*(text + 1);
        int kern = stbtt_GetCodepointKernAdvance(&font->info, codepoint, next_codepoint);

        width += (advance_width + kern) * actual_scale;
        text++;
    }

    return width;
}

float cce_text_ascent(TTF_Font* font, float scale)
{
    if (!font || !font->ttf_data) return 0;
    if (!font->info_initialized) return 0;

    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);

    float actual_scale = font->scale * scale;
    return ascent * actual_scale;
}

float cce_text_height(TTF_Font* font, const char* text, float scale)
{
    if (!font || !text || !font->ttf_data) return 0;
    if (*text == '\0') return 0;

    if (!font->info_initialized) return 0;

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);

    float actual_scale = font->scale * scale;
    float line_height = (ascent - descent + line_gap) * actual_scale;

    int lines = 1;
    const char* ptr = text;
    while (*ptr) {
        if (*ptr == '\n') {
            lines++;
        }
        ptr++;
    }

    return line_height * lines;
}

void cce_draw_text(CCE_Layer* layer, TTF_Font* font, const char* text, 
                               int x, int y, float scale, CCE_Color color)
{
    if (!layer || !font || !text || !font->ttf_data) return;
    if (!font->info_initialized) return;
    
    // Calculate the actual scale (font scale * user scale)
    float actual_scale = font->scale * scale;
    
    // Get font metrics for baseline
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
    
    float current_x = (float)x;
    float current_y = (float)y;
    float start_x = current_x;
    
    while (*text) {
        if (*text == '\n') {
            current_y += (ascent - descent + line_gap) * actual_scale;
            current_x = start_x;
            text++;
            continue;
        }
        
        int codepoint = (unsigned char)*text;
        
        // Get character metrics
        int advance_width, left_side_bearing;
        stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance_width, &left_side_bearing);
        
        // Get bitmap box to determine size
        int ix0, iy0, ix1, iy1;
        stbtt_GetCodepointBitmapBox(&font->info, codepoint, actual_scale, actual_scale, &ix0, &iy0, &ix1, &iy1);
        
        int bitmap_width = ix1 - ix0;
        int bitmap_height = iy1 - iy0;
        
        if (bitmap_width > 0 && bitmap_height > 0) {
            const size_t needed = (size_t)bitmap_width * (size_t)bitmap_height;
            if (needed > font->glyph_scratch_size) {
                unsigned char* new_buf = realloc(font->glyph_scratch, needed);
                if (!new_buf) {
                    // Skip glyph if allocation fails.
                    current_x += advance_width * actual_scale;
                    text++;
                    continue;
                }
                font->glyph_scratch = new_buf;
                font->glyph_scratch_size = needed;
            }

            unsigned char* bitmap = font->glyph_scratch;
            // Render glyph to bitmap
            stbtt_MakeCodepointBitmap(&font->info, bitmap, bitmap_width, bitmap_height,
                                      bitmap_width, actual_scale, actual_scale, codepoint);
                
                // Calculate position
                // stb_truetype uses y-up coordinate system (y=0 at baseline, y increases upward)
                // layer uses y-down coordinate system (y=0 at top, y increases downward)
                // ix0, iy0 is top-left of bitmap in font coords (iy0 is negative, above baseline)
                // To convert: layer_y = baseline_y + font_y (since layer y increases downward)
                int base_x = (int)(current_x + left_side_bearing * actual_scale + ix0);
                int base_y = (int)(current_y + iy0); // iy0 is negative, so this gives top of bitmap
                
                // Write pixels to layer
                for (int by = 0; by < bitmap_height; by++) {
                    for (int bx = 0; bx < bitmap_width; bx++) {
                        unsigned char alpha = bitmap[by * bitmap_width + bx];
                        if (alpha > 0) {
                            int px = base_x + bx;
                            int py = base_y + by;
                            
                            // Blend color with alpha
                            CCE_Color pixel_color;
                            pixel_color.r = (unsigned char)((color.r * alpha) / 255);
                            pixel_color.g = (unsigned char)((color.g * alpha) / 255);
                            pixel_color.b = (unsigned char)((color.b * alpha) / 255);
                            pixel_color.a = (unsigned char)((color.a * alpha) / 255);
                            
                            set_pixel(layer, px, py, pixel_color);
                        }
                    }
                }
        }
        
        // Advance to next character
        current_x += advance_width * actual_scale;
        text++;
    }
}

void cce_draw_text_fmt(CCE_Layer* layer, TTF_Font* font, 
                                   int x, int y, float scale, CCE_Color color,
                                   const char* format, ...)
{
    if (!layer || !font || !format) return;
    
    // Try with a fixed-size buffer first (sufficient for most cases like FPS)
    char static_buffer[256];
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(static_buffer, sizeof(static_buffer), format, args);
    va_end(args);
    
    char* buffer = static_buffer;
    char* dynamic_buffer = NULL;
    
    // If the formatted string doesn't fit, allocate a larger buffer
    if (needed >= (int)sizeof(static_buffer)) {
        dynamic_buffer = malloc(needed + 1);
        if (dynamic_buffer) {
            va_start(args, format);
            vsnprintf(dynamic_buffer, needed + 1, format, args);
            va_end(args);
            buffer = dynamic_buffer;
        } else {
            // If allocation fails, use truncated version from static buffer
            buffer = static_buffer;
        }
    }
    
    // Call the actual rendering function
    cce_draw_text(layer, font, buffer, x, y, scale, color);
    
    // Free dynamically allocated buffer if used
    if (dynamic_buffer) {
        free(dynamic_buffer);
    }
}
