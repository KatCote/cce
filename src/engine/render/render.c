#include "render.h"
#include "../engine.h"

#include <stdio.h>
#include <unistd.h>
#include <GL/gl.h>
#include <stdarg.h>

void cce_setup_2d_projection(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void cce_draw_grid(int x0, int y0, int x1, int y1, int pixel_size, int offset_x, int offset_y, Palette palette)
{
    if ((x0 > x1) || (y0 > y1)) { cce_printf("Wrong grid size!\n"); ERRLOG; return; }
    if (pixel_size < 1) { cce_printf("Wrong pixel size!\n"); ERRLOG; return; }

    int cols = (x1 + pixel_size - 1) / pixel_size;
    int rows = (y1 + pixel_size - 1) / pixel_size;

    Color color;
    
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            int pixel_x = x0 + col * pixel_size;
            int pixel_y = y0 + row * pixel_size;
            
            color = cce_get_color(pixel_x, pixel_y, offset_x, offset_y, palette);
            
            draw_pixel(pixel_x, pixel_y, pixel_size, color.r, color.g, color.b);
        }
    }

    color = cce_get_color(x1 - pixel_size + 1, y1 - pixel_size + 1, offset_x, offset_y, palette);
    draw_pixel(x1, y1, pixel_size, color.r, color.g, color.b);
}

Color cce_get_color(int pos_x, int pos_y, int offset_x, int offset_y, Palette palette, ...)
{
    Color ret;
    float noise = procedural_noise(pos_x + offset_x, pos_y + offset_y, engine_seed);

    switch (palette)
    {
        case Red:
            ret.r = 1;
            ret.g = 0;
            ret.b = 0;
            break;

        case Green:
            ret.r = 0;
            ret.g = 1;
            ret.b = 0;
            break;

        case Blue:
            ret.r = 0;
            ret.g = 0;
            ret.b = 1;
            break;
            
        case Manual:
            va_list rgb;
            va_start(rgb, palette);
            ret.r = va_arg(rgb, double);
            ret.g = va_arg(rgb, double);
            ret.b = va_arg(rgb, double);
            va_end(rgb);
            break;

        case DefaultGrass:
            if (noise < 0.1f) { noise = 0.2; }
            if (noise < 0.5f) { noise = noise * 5.0f; }
            if (noise >= 1.0f) { noise = 0.6f; }
            ret.r = noise - 0.3f;
            ret.g = noise - 0.1f;
            ret.b = noise - 0.8f;
            break;

        case DefaultStone:
            if (noise < 0.6f) { noise = 0.6f; }
            if (noise > 0.8f) { noise = 0.8f; }
            ret.r = noise - 0.4f;
            ret.g = noise - 0.4f;
            ret.b = noise - 0.4f;
            break;

        case DefaultDark:
            ret.r = 0.08f;
            ret.g = 0.08f;
            ret.b = 0.08f;
            break;

        case DefaultLight:
            ret.r = 0.95f;
            ret.g = 0.95f;
            ret.b = 0.95f;
            break;

        default:
            ret.r = 0;
            ret.g = 0;
            ret.b = 0;
            break;
    }
    return ret;
}

void draw_pixel(int x, int y, int size, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + size, y);
    glVertex2i(x + size, y + size);
    glVertex2i(x, y + size);
    glEnd();
}

float procedural_noise(int x, int y, int seed)
{
    uint32_t n = (x * 1836311903) ^ (y * 2971215073) ^ (seed * 1073741827);
    n = (n >> 13) ^ n;
    n = (n * (n * n * 60493 + 19990303) + 1376312589);
    return (float)(n & 0x7FFFFFFF) / 2147483647.0f;
}
