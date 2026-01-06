#ifndef CCE_RENDER_GUARD_H
#define CCE_RENDER_GUARD_H

#include "../engine.h"

float procedural_noise(int x, int y, int seed);
CCE_Layer* create_layer(int screen_w, int screen_h, char * name);
void set_pixel(CCE_Layer* layer, int screen_x, int screen_y, CCE_Color color);
void set_pixel_rect(CCE_Layer* layer, int x0, int y0, int x1, int y1, CCE_Color color);

#endif