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

#ifndef CCE_SHADER_GUARD_H
#define CCE_SHADER_GUARD_H

#include "../engine.h"
#include "../render/render.h"

int cce_shader_load_from_file(CCE_Shader* out, const char* path, CCE_ShaderType type, const char* name);
void cce_shader_unload(CCE_Shader* shader);
int cce_shader_apply(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, int arg_count, ...);
int cce_shader_apply_tint(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, CCE_Color tint);
int cce_shader_apply_grayscale(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_apply_glow(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_create_from_source(CCE_Shader* out, const char* vertex_src, const char* fragment_src, const char* name);
int cce_shader_create_from_files(CCE_Shader* out, const char* vertex_path, const char* fragment_path, const char* name);
int cce_shader_apply_bloom(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);

#endif