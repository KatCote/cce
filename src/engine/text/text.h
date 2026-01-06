#ifndef CCE_TEXT_GUARD_H
#define CCE_TEXT_GUARD_H

#include "../engine.h"

void ttf_render_text_to_layer(CCE_Layer* layer, TTF_Font* font, const char* text, 
                               int x, int y, float scale, CCE_Color color);
void ttf_render_text_to_layer_fmt(CCE_Layer* layer, TTF_Font* font, 
                                   int x, int y, float scale, CCE_Color color,
                                   const char* format, ...);

#endif