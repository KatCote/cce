#ifndef CCE_RENDER_GUARD_H
#define CCE_RENDER_GUARD_H

#include "../engine.h"

void draw_pixel(int x, int y, int size, pct r, pct g, pct b, pct a);
float procedural_noise(int x, int y, int seed);
void draw_filled_circle(float center_x, float center_y, float radius, int pixel_size);
Layer* create_layer(int screen_w, int screen_h, char * name);
void set_pixel(Layer* layer, int screen_x, int screen_y, cce_color color);

#endif