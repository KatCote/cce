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

#include "sprite.h"
#include "../engine.h"

#include <GL/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

int cce_sprite_load(CCE_Sprite* out)
{
    if (!out->path[0] || !out) {
        ERRLOG;
        return -1;
    }

    stbi_set_flip_vertically_on_load(0);

    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load(out->path, &w, &h, &channels, 4);
    if (!data) {
        cce_printf("❌ Failed to load PNG \"%s\": %s\n", out->path, stbi_failure_reason());
        return -1;
    }

    out->width = w;
    out->height = h;
    out->channels = 4;
    out->data = data;
    out->texture_id = 0;

    return 0;
}

int cce_texture_load(CCE_Texture* out, const char* filename)
{
    if (!out || !filename) {
        ERRLOG;
        return -1;
    }

    // Keep image data in file order (top-to-bottom). The renderer converts UVs to OpenGL convention.
    stbi_set_flip_vertically_on_load(0);

    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load(filename, &w, &h, &channels, 4);
    if (!data) {
        cce_printf("❌ Failed to load image \"%s\": %s\n", filename, stbi_failure_reason());
        return -1;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    out->id = (unsigned int)tex;
    out->width = w;
    out->height = h;
    return 0;
}

void cce_texture_free(CCE_Texture* tex)
{
    if (!tex) return;
    if (tex->id) {
        GLuint id = (GLuint)tex->id;
        glDeleteTextures(1, &id);
    }
    tex->id = 0;
    tex->width = 0;
    tex->height = 0;
}

void cce_sprite_free(CCE_Sprite* img)
{
    if (!img || !img->data) return;
    if (img->texture_id) {
        GLuint id = (GLuint)img->texture_id;
        glDeleteTextures(1, &id);
        img->texture_id = 0;
    }
    stbi_image_free(img->data);
    img->data = NULL;
    img->width = 0;
    img->height = 0;
    img->channels = 0;
}

int cce_draw_sprite(
    CCE_Layer* layer,
    const CCE_Sprite* sprite,
    int dst_x,
    int dst_y,
    int batch_size,
    CCE_Color modifier,
    int frame_step_px,
    int current_step)
{
    if (!layer || !sprite || !sprite->data || batch_size <= 0) {
        ERRLOG;
        return -1;
    }

    // GPU backend: upload sprite as a GL texture once and draw as a quad (baked into GPU layer FBO).
    if (layer->backend == CCE_LAYER_GPU)
    {
        // Lazily upload the sprite texture on first draw.
        // NOTE: `sprite` is const in public API, so we update the cache via a cast.
        CCE_Sprite* mut = (CCE_Sprite*)(void*)sprite;
        if (mut->texture_id == 0) {
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, sprite->width, sprite->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, sprite->data);
            glBindTexture(GL_TEXTURE_2D, 0);
            mut->texture_id = (unsigned int)tex;
        }

        const int img_w_total = sprite->width;
        const int img_h = sprite->height;
        if (img_w_total <= 0 || img_h <= 0) return -1;

        // Determine frame window (same logic as CPU path).
        int frame_width = img_w_total;
        int frame_offset_x = 0;
        if (frame_step_px > 0) {
            frame_width = frame_step_px;
            if (frame_width > img_w_total) frame_width = img_w_total;
            long long offset = (long long)frame_step_px * (long long)current_step;
            frame_offset_x = (int)(offset % img_w_total);
            if (frame_offset_x + frame_width > img_w_total) {
                frame_width = img_w_total - frame_offset_x;
            }
            if (frame_width <= 0) return -1;
        }

        // UVs: sprite data is loaded top-to-bottom (stbi flip=0). OpenGL expects bottom row first.
        // CPU path compensates by reading src_y = img_h - 1 - y; for GPU we flip V in UVs.
        float u0 = (float)frame_offset_x / (float)img_w_total;
        float u1 = (float)(frame_offset_x + frame_width) / (float)img_w_total;
        float v0 = 1.0f;
        float v1 = 0.0f;

        const float w = (float)frame_width * (float)batch_size;
        const float h = (float)img_h * (float)batch_size;
        CCE_Texture tmp = {0};
        tmp.id = mut->texture_id;
        tmp.width = img_w_total;
        tmp.height = img_h;
        return cce_draw_texture_region(&tmp, (float)dst_x, (float)dst_y, w, h, u0, v0, u1, v1, modifier);
    }

    const int img_w_total = sprite->width;
    const int img_h = sprite->height;

    if (img_w_total <= 0 || img_h <= 0) {
        ERRLOG;
        return -1;
    }

    // Determine frame window.
    int frame_width = img_w_total;
    int frame_offset_x = 0;

    if (frame_step_px > 0) {
        frame_width = frame_step_px;
        if (frame_width > img_w_total) {
            frame_width = img_w_total;
        }

        long long offset = (long long)frame_step_px * (long long)current_step;
        frame_offset_x = (int)(offset % img_w_total);

        if (frame_offset_x + frame_width > img_w_total) {
            frame_width = img_w_total - frame_offset_x;
        }

        if (frame_width <= 0) {
            return -1;
        }
    }

    for (int y = 0; y < img_h; y++) {
        int src_y = img_h - 1 - y; // bottom-left origin for png
        for (int x = 0; x < frame_width; x++) {
            int idx = (src_y * img_w_total + (frame_offset_x + x)) * 4;

            CCE_Color color = {
                .r = sprite->data[idx + 0],
                .g = sprite->data[idx + 1],
                .b = sprite->data[idx + 2],
                .a = sprite->data[idx + 3],
            };

            // Apply multiplicative modifier per channel.
            color.r = (pct)((color.r * modifier.r) / 255);
            color.g = (pct)((color.g * modifier.g) / 255);
            color.b = (pct)((color.b * modifier.b) / 255);
            color.a = (pct)((color.a * modifier.a) / 255);

            int screen_x0 = dst_x + x * batch_size;
            int screen_x1 = screen_x0 + batch_size - 1;

            // Convert bottom-left sprite coordinates to engine's top-left origin.
            int bottom_y = dst_y + y * batch_size;
            int screen_y0 = layer->scr_h - (bottom_y + batch_size);
            int screen_y1 = layer->scr_h - 1 - bottom_y;

            cce_set_pixel_rect(layer, screen_x0, screen_y0, screen_x1, screen_y1, color);
        }
    }

    return 0;
}

void cce_sprite_calc_frame_uv(const CCE_Texture* tex, int frame_width_px, int frame_index, float* u0, float* u1)
{
    if (!tex || tex->width <= 0) {
        if (u0) *u0 = 0.0f;
        if (u1) *u1 = 1.0f;
        return;
    }

    const int w = tex->width;
    if (frame_width_px <= 0) frame_width_px = w;

    int frame_px = frame_width_px > w ? w : frame_width_px;
    int xoff = (frame_width_px * frame_index) % w;
    if (xoff + frame_px > w) frame_px = w - xoff;
    if (frame_px <= 0) frame_px = w;

    const float inv_w = 1.0f / (float)w;
    if (u0) *u0 = (float)xoff * inv_w;
    if (u1) *u1 = (float)(xoff + frame_px) * inv_w;
}