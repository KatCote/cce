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

typedef struct CCE_GlyphEntry CCE_GlyphEntry;

struct TTF_Font {
    GLuint texture_id;
    int texture_width;
    int texture_height;
    float font_size;
    float scale;
    unsigned char* ttf_data;
    long ttf_data_size;

    // Cached font info to avoid stbtt_InitFont on every call.
    stbtt_fontinfo info;
    int info_initialized;

    // Scratch buffer for glyph rasterization (avoids per-glyph malloc/free).
    unsigned char* glyph_scratch;
    size_t glyph_scratch_size;

    float* vtx_scratch;
    size_t vtx_scratch_floats;

    // GPU glyph cache atlas (rasterized per requested `scale` like CPU path).
    int atlas_cursor_x;
    int atlas_cursor_y;
    int atlas_row_h;

    CCE_GlyphEntry* glyphs;
    int glyph_count;
    int glyph_cap;
};

#endif

struct CCE_GlyphEntry {
    int codepoint;
    // Quantized key for scale to keep cache stable across floating-point noise.
    // Key is scale * 1024 (rounded).
    unsigned int scale_key;

    int x;      // atlas x
    int y;      // atlas y
    int w;      // glyph bitmap width
    int h;      // glyph bitmap height

    int ix0;    // stbtt bitmap box x0 (offset from pen/baseline)
    int iy0;    // stbtt bitmap box y0 (offset from pen/baseline)

    float xadvance; // advance in screen pixels at this scale (includes base scale)
};

static unsigned int quantize_scale_key(float scale)
{
    if (scale <= 0.0f) return 0;
    // 1/1024 resolution is plenty for stable caching without exploding variants.
    float kf = scale * 1024.0f;
    if (kf < 0.0f) kf = 0.0f;
    return (unsigned int)(kf + 0.5f);
}

static float scale_from_key(unsigned int key)
{
    return (float)key / 1024.0f;
}

static CCE_GlyphEntry* find_glyph(TTF_Font* font, int codepoint, unsigned int scale_key)
{
    if (!font || !font->glyphs) return NULL;
    for (int i = 0; i < font->glyph_count; i++) {
        CCE_GlyphEntry* g = (CCE_GlyphEntry*)&font->glyphs[i];
        if (g->codepoint == codepoint && g->scale_key == scale_key) return g;
    }
    return NULL;
}

static CCE_GlyphEntry* push_glyph(TTF_Font* font)
{
    if (!font) return NULL;
    if (font->glyph_count + 1 > font->glyph_cap) {
        int nc = (font->glyph_cap == 0) ? 128 : font->glyph_cap * 2;
        CCE_GlyphEntry* nb = realloc(font->glyphs, (size_t)nc * sizeof(CCE_GlyphEntry));
        if (!nb) return NULL;
        font->glyphs = nb;
        font->glyph_cap = nc;
    }
    CCE_GlyphEntry* g = &font->glyphs[font->glyph_count++];
    memset(g, 0, sizeof(*g));
    return g;
}

static int ensure_glyph_atlas(TTF_Font* font)
{
    if (!font) return -1;
    if (font->texture_id != 0 && font->texture_width > 0 && font->texture_height > 0) return 0;

    // A single atlas is enough for demos; keep it large to support upscaled glyph rasterization.
    font->texture_width = 2048;
    font->texture_height = 2048;

    glGenTextures(1, &font->texture_id);
    glBindTexture(GL_TEXTURE_2D, font->texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Allocate empty RGBA atlas.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, font->texture_width, font->texture_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);

    font->atlas_cursor_x = 1;
    font->atlas_cursor_y = 1;
    font->atlas_row_h = 0;
    return 0;
}

static int upload_glyph_to_atlas(
    TTF_Font* font,
    int codepoint,
    unsigned int scale_key,
    float actual_scale)
{
    if (!font || !font->info_initialized) return -1;
    if (ensure_glyph_atlas(font) != 0) return -1;
    if (font->glyph_scratch == NULL) {
        font->glyph_scratch = NULL;
        font->glyph_scratch_size = 0;
    }

    int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
    stbtt_GetCodepointBitmapBox(&font->info, codepoint, actual_scale, actual_scale, &ix0, &iy0, &ix1, &iy1);

    const int w = ix1 - ix0;
    const int h = iy1 - iy0;
    if (w <= 0 || h <= 0) {
        // Still cache as empty so we don't re-query constantly (spaces etc).
        CCE_GlyphEntry* g = push_glyph(font);
        if (!g) return -1;
        int aw = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&font->info, codepoint, &aw, &lsb);
        g->codepoint = codepoint;
        g->scale_key = scale_key;
        g->x = 0; g->y = 0; g->w = 0; g->h = 0;
        g->ix0 = 0; g->iy0 = 0;
        g->xadvance = (float)aw * actual_scale;
        return 0;
    }

    const size_t needed = (size_t)w * (size_t)h;
    if (needed > font->glyph_scratch_size) {
        unsigned char* nb = realloc(font->glyph_scratch, needed);
        if (!nb) return -1;
        font->glyph_scratch = nb;
        font->glyph_scratch_size = needed;
    }

    unsigned char* bitmap = font->glyph_scratch;
    stbtt_MakeCodepointBitmap(&font->info, bitmap, w, h, w, actual_scale, actual_scale, codepoint);

    // Pack into atlas (simple shelf packer with 1px padding).
    const int pad = 1;
    if (font->atlas_cursor_x + w + pad >= font->texture_width) {
        font->atlas_cursor_x = 1;
        font->atlas_cursor_y += font->atlas_row_h + pad;
        font->atlas_row_h = 0;
    }
    if (font->atlas_cursor_y + h + pad >= font->texture_height) {
        // Atlas full.
        return -1;
    }

    const int gx = font->atlas_cursor_x;
    const int gy = font->atlas_cursor_y;
    font->atlas_cursor_x += w + pad;
    if (h > font->atlas_row_h) font->atlas_row_h = h;

    // Expand to RGBA for upload (white RGB, alpha = coverage), matching old behavior.
    // Use a temporary stack buffer for small glyphs, heap for larger ones.
    const size_t rgba_bytes = needed * 4;
    unsigned char* rgba = NULL;
    unsigned char small_rgba[4096];
    if (rgba_bytes <= sizeof(small_rgba)) {
        rgba = small_rgba;
    } else {
        rgba = malloc(rgba_bytes);
        if (!rgba) return -1;
    }
    for (size_t i = 0; i < needed; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }

    glBindTexture(GL_TEXTURE_2D, font->texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, gx, gy, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (rgba != small_rgba) free(rgba);

    // Store metrics for layout.
    int aw = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&font->info, codepoint, &aw, &lsb);

    CCE_GlyphEntry* g = push_glyph(font);
    if (!g) return -1;
    g->codepoint = codepoint;
    g->scale_key = scale_key;
    g->x = gx;
    g->y = gy;
    g->w = w;
    g->h = h;
    g->ix0 = ix0;
    g->iy0 = iy0;
    g->xadvance = (float)aw * actual_scale;
    return 0;
}

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
    if (!stbtt_InitFont(&font->info, ttf_data, 0)) {
        printf("Failed to init font\n");
        free(ttf_data);
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
    font->vtx_scratch = NULL;
    font->vtx_scratch_floats = 0;

    font->glyphs = NULL;
    font->glyph_count = 0;
    font->glyph_cap = 0;
    font->atlas_cursor_x = 0;
    font->atlas_cursor_y = 0;
    font->atlas_row_h = 0;

    // Initialize atlas lazily; this also makes CPU-only use avoid GL uploads beyond the GL context requirements.
    
    return font;
}

void cce_font_free(TTF_Font* font)
{
    if (font) {
        glDeleteTextures(1, &font->texture_id);
        if (font->glyphs) {
            free(font->glyphs);
        }
        if (font->glyph_scratch) {
            free(font->glyph_scratch);
        }
        if (font->vtx_scratch) {
            free(font->vtx_scratch);
        }
        if (font->ttf_data) {
            free(font->ttf_data);
        }
        free(font);
    }
}

void cce_font_set_smooth(TTF_Font* font, int smooth)
{
    if (!font) return;
    if (ensure_glyph_atlas(font) != 0) return;
    if (font->texture_id == 0) return;

    glBindTexture(GL_TEXTURE_2D, font->texture_id);
    if (smooth) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
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

    // GPU layer: draw directly with GPU text into the layer's render target.
    if (layer->backend == CCE_LAYER_GPU) {
        int began = cce_layer_begin(layer);
        if (began < 0) return;
        if (cce_draw_text_gpu(font, text, (float)x, (float)y, scale, color) < 0) {
            // ignore
        }
        if (began > 0) {
            cce_layer_end(layer);
        }
        return;
    }
    
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
                            
                            cce_set_pixel(layer, px, py, pixel_color);
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

int cce_draw_text_gpu(TTF_Font* font, const char* text, float x, float y, float scale, CCE_Color color)
{
    if (!font || !text || !font->info_initialized) return -1;
    if (scale <= 0.0f) return 0;

    const unsigned int scale_key = quantize_scale_key(scale);
    if (scale_key == 0) return 0;
    const float qscale = scale_from_key(scale_key);
    const float actual_scale = font->scale * qscale;

    // Conservative upper bound: each character => 2 triangles => 6 vertices => 24 floats.
    size_t len = 0;
    for (const char* p = text; *p; p++) len++;
    if (len == 0) return 0;

    const size_t needed_floats = len * 24;
    if (needed_floats > font->vtx_scratch_floats) {
        float* nb = realloc(font->vtx_scratch, needed_floats * sizeof(float));
        if (!nb) return -1;
        font->vtx_scratch = nb;
        font->vtx_scratch_floats = needed_floats;
    }

    // Pen position in screen pixels (baseline coords) like CPU path.
    float pen_x = 0.0f;
    float pen_y = 0.0f;
    float start_x = 0.0f;
    int v = 0;

    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
    const float line_advance = (ascent - descent + line_gap) * actual_scale;

    for (const char* p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;

        if (c == '\n') {
            pen_x = start_x;
            pen_y += line_advance;
            continue;
        }

        const int codepoint = (int)c;

        CCE_GlyphEntry* g = find_glyph(font, codepoint, scale_key);
        if (!g) {
            if (upload_glyph_to_atlas(font, codepoint, scale_key, actual_scale) != 0) {
                // Skip glyph on failure but still advance.
                int aw = 0, lsb = 0;
                stbtt_GetCodepointHMetrics(&font->info, codepoint, &aw, &lsb);
                int next = (unsigned char)*(p + 1);
                int kern = stbtt_GetCodepointKernAdvance(&font->info, codepoint, next);
                pen_x += (aw + kern) * actual_scale;
                continue;
            }
            g = find_glyph(font, codepoint, scale_key);
            if (!g) continue;
        }

        // Advance includes kerning to next character (like cce_text_width).
        int next = (unsigned char)*(p + 1);
        int kern = stbtt_GetCodepointKernAdvance(&font->info, codepoint, next);
        const float advance = g->xadvance + (float)kern * actual_scale;

        if (g->w > 0 && g->h > 0) {
            const float x0 = x + pen_x + (float)g->ix0;
            const float y0 = y + pen_y + (float)g->iy0;
            const float x1 = x0 + (float)g->w;
            const float y1 = y0 + (float)g->h;

            const float s0 = (float)g->x / (float)font->texture_width;
            const float t0 = (float)g->y / (float)font->texture_height;
            const float s1 = (float)(g->x + g->w) / (float)font->texture_width;
            const float t1 = (float)(g->y + g->h) / (float)font->texture_height;

            font->vtx_scratch[v++] = x0; font->vtx_scratch[v++] = y0; font->vtx_scratch[v++] = s0; font->vtx_scratch[v++] = t0;
            font->vtx_scratch[v++] = x1; font->vtx_scratch[v++] = y0; font->vtx_scratch[v++] = s1; font->vtx_scratch[v++] = t0;
            font->vtx_scratch[v++] = x1; font->vtx_scratch[v++] = y1; font->vtx_scratch[v++] = s1; font->vtx_scratch[v++] = t1;

            font->vtx_scratch[v++] = x1; font->vtx_scratch[v++] = y1; font->vtx_scratch[v++] = s1; font->vtx_scratch[v++] = t1;
            font->vtx_scratch[v++] = x0; font->vtx_scratch[v++] = y1; font->vtx_scratch[v++] = s0; font->vtx_scratch[v++] = t1;
            font->vtx_scratch[v++] = x0; font->vtx_scratch[v++] = y0; font->vtx_scratch[v++] = s0; font->vtx_scratch[v++] = t0;
        }

        pen_x += advance;
    }

    const int vertex_count = v / 4;
    if (vertex_count <= 0) return 0;
    return cce_draw_triangles_textured(font->texture_id, font->vtx_scratch, vertex_count, color);
}
