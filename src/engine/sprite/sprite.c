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

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

int cce_sprite_image_load(const char* filepath, CCE_SpriteImage* out)
{
    if (!filepath || !out) {
        ERRLOG;
        return -1;
    }

    stbi_set_flip_vertically_on_load(0);

    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load(filepath, &w, &h, &channels, 4);
    if (!data) {
        cce_printf("❌ Failed to load PNG \"%s\": %s\n", filepath, stbi_failure_reason());
        return -1;
    }

    out->width = w;
    out->height = h;
    out->channels = 4;
    out->data = data;

    return 0;
}

void cce_sprite_image_free(CCE_SpriteImage* img)
{
    if (!img || !img->data) return;
    stbi_image_free(img->data);
    img->data = NULL;
    img->width = 0;
    img->height = 0;
    img->channels = 0;
}

int cce_draw_png_to_layer(
    CCE_Layer* layer,
    const CCE_SpriteImage* sprite,
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

            set_pixel_rect(layer, screen_x0, screen_y0, screen_x1, screen_y1, color);
        }
    }

    return 0;
}

int cce_get_png_height(const char* filepath)
{
    if (!filepath) {
        ERRLOG;
        return -1;
    }

    int w = 0, h = 0, comp = 0;
    if (stbi_info(filepath, &w, &h, &comp) == 0) {
        cce_printf("❌ Failed to read PNG info \"%s\": %s\n", filepath, stbi_failure_reason());
        return -1;
    }

    return h;
}