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

#define GL_GLEXT_PROTOTYPES 1
#include "render.h"
#include "../engine.h"
#include "../shader/shader.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

_Static_assert(sizeof(CCE_Color) == 4, "CCE_Color must be 4 bytes (RGBA u8) for packed fast paths");

// FBO constants (glext should provide them, but keep fallbacks).
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

// Определения для PBO констант
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif

#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif

static GLuint g_quad_vao = 0;
static GLuint g_quad_vbo = 0;
static GLuint g_quad_ebo = 0;
static CCE_Shader g_quad_shader;
static GLint g_u_projection = -1;
static GLint g_u_texture = -1;
static GLint g_u_tint = -1;
static int g_renderer_ready = 0;
static float g_projection[16];
static int g_proj_w = 0;
static int g_proj_h = 0;
static void update_dirty_chunks(CCE_Layer* layer);

static GLuint g_batch_vao = 0;
static GLuint g_batch_vbo = 0;
static int g_batch_ready = 0;

static GLuint g_white_tex = 0;

static float srgb_to_linear_u8(pct c)
{
    const float v = (float)c / 255.0f;
    if (v <= 0.04045f) return v / 12.92f;
    return powf((v + 0.055f) / 1.055f, 2.4f);
}

typedef struct {
    GLint viewport[4];
    int proj_w;
    int proj_h;
    float projection[16];
    GLuint prev_fbo;
} CCE_TargetSnapshot;

static CCE_TargetSnapshot g_target_stack[8];
static int g_target_stack_top = 0;

static CCE_Layer* g_active_layer = NULL; // used for auto begin/end

static void ensure_white_texture(void)
{
    if (g_white_tex != 0) return;
    const unsigned char px[4] = {255, 255, 255, 255};
    glGenTextures(1, &g_white_tex);
    glBindTexture(GL_TEXTURE_2D, g_white_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static int push_target(GLuint fbo, int w, int h)
{
    if (g_target_stack_top >= (int)(sizeof(g_target_stack) / sizeof(g_target_stack[0]))) {
        return -1;
    }

    CCE_TargetSnapshot* s = &g_target_stack[g_target_stack_top++];
    glGetIntegerv(GL_VIEWPORT, s->viewport);
    GLint bound = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
    s->prev_fbo = (GLuint)bound;
    s->proj_w = g_proj_w;
    s->proj_h = g_proj_h;
    memcpy(s->projection, g_projection, sizeof(g_projection));

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    cce_setup_2d_projection(w, h);
    return 0;
}

static void pop_target(void)
{
    if (g_target_stack_top <= 0) return;
    CCE_TargetSnapshot* s = &g_target_stack[--g_target_stack_top];

    glBindFramebuffer(GL_FRAMEBUFFER, s->prev_fbo);
    memcpy(g_projection, s->projection, sizeof(g_projection));
    g_proj_w = s->proj_w;
    g_proj_h = s->proj_h;
    glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
}

static int ensure_layer_shader_target(CCE_Layer* layer)
{
    if (!layer) return -1;
    if (layer->shader_texture != 0 && layer->shader_fbo != 0) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, layer->scr_w, layer->scr_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        return -1;
    }

    layer->shader_texture = (unsigned int)tex;
    layer->shader_fbo = (unsigned int)fbo;
    layer->shader_dirty = 1;
    return 0;
}

static int maybe_apply_layer_shader(CCE_Layer* layer, int each_frame)
{
    if (!layer || !layer->shader || !layer->shader->loaded) return 0;

    if (ensure_layer_shader_target(layer) != 0) return -1;

    if (!each_frame && layer->shader_mode == CCE_LAYER_SHADER_BAKE_ON_DIRTY && !layer->shader_dirty) {
        return 0;
    }

    // Render shader output into layer->shader_texture.
    const int arg_count = layer->shader_has_tint ? 4 : 0;
    int res = 0;
    if (arg_count == 0) {
        res = cce_shader_apply_to_texture(layer->shader, layer->texture, layer->shader_fbo, layer->scr_w, layer->scr_h, layer->shader_coefficient, 0);
    } else {
        res = cce_shader_apply_to_texture(
            layer->shader,
            layer->texture,
            layer->shader_fbo,
            layer->scr_w,
            layer->scr_h,
            layer->shader_coefficient,
            4,
            (double)layer->shader_tint.r / 255.0,
            (double)layer->shader_tint.g / 255.0,
            (double)layer->shader_tint.b / 255.0,
            (double)layer->shader_tint.a / 255.0
        );
    }

    if (res == 0 && layer->shader_mode == CCE_LAYER_SHADER_BAKE_ON_DIRTY) {
        layer->shader_dirty = 0;
    }
    return res;
}

static void make_ortho(float left, float right, float bottom, float top, float near_val, float far_val, float out[16])
{
    // Column-major layout
    memset(out, 0, sizeof(float) * 16);
    out[0]  = 2.0f / (right - left);
    out[5]  = 2.0f / (top - bottom);
    out[10] = -2.0f / (far_val - near_val);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(far_val + near_val) / (far_val - near_val);
    out[15] = 1.0f;
}

static int ensure_quad_pipeline(void)
{
    if (g_renderer_ready) return 0;

    const char* vs =
        "#version 330 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aUV;\n"
        "uniform mat4 uProjection;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vUV = aUV;\n"
        "    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* fs =
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTexture;\n"
        "uniform vec4 uTint;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = texture(uTexture, vUV) * uTint;\n"
        "}\n";

    if (cce_shader_create_from_source(&g_quad_shader, vs, fs, "cce-quad") != 0) {
        return -1;
    }

    g_u_projection = glGetUniformLocation(g_quad_shader.program, "uProjection");
    g_u_texture = glGetUniformLocation(g_quad_shader.program, "uTexture");
    g_u_tint = glGetUniformLocation(g_quad_shader.program, "uTint");

    glGenVertexArrays(1, &g_quad_vao);
    glGenBuffers(1, &g_quad_vbo);
    glGenBuffers(1, &g_quad_ebo);

    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_quad_ebo);
    const unsigned int indices[6] = {0, 1, 2, 2, 3, 0};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    g_renderer_ready = 1;

    // Enable sRGB framebuffer writes so colors/alpha match source textures.
    glEnable(GL_FRAMEBUFFER_SRGB);
    return 0;
}

static int ensure_batch_pipeline(void)
{
    if (g_batch_ready) return 0;
    if (ensure_quad_pipeline() != 0) return -1;

    glGenVertexArrays(1, &g_batch_vao);
    glGenBuffers(1, &g_batch_vbo);

    glBindVertexArray(g_batch_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_batch_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STREAM_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    g_batch_ready = 1;
    return 0;
}

void cce_render_prepare_layer(CCE_Layer* layer)
{
    if (!layer) return;
    if (ensure_quad_pipeline() != 0) return;
    if (layer->backend == CCE_LAYER_CPU) {
        update_dirty_chunks(layer);
    }
}

void cce_setup_2d_projection(int width, int height)
{
    g_proj_w = width;
    g_proj_h = height;
    // Top-left origin: left=0, right=width, top=0, bottom=height
    make_ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, g_projection);
    glViewport(0, 0, width, height);
    ensure_quad_pipeline();
}

int cce_draw_texture_region(
    const CCE_Texture* tex,
    float x, float y,
    float w, float h,
    float u0, float v0,
    float u1, float v1,
    CCE_Color tint)
{
    if (!tex || tex->id == 0 || w <= 0.0f || h <= 0.0f) return -1;
    if (ensure_quad_pipeline() != 0) return -1;

    // Match engine sprite convention: (x,y) are in bottom-left screen coordinates.
    // Convert to top-left screen coordinates used by our projection.
    const float y_top = (float)g_proj_h - y - h;

    float verts[16] = {
        // UV convention: v=0 at TOP (matches stbtt atlas and stb_image default row order).
        x,     y_top,     u0, v0,
        x + w, y_top,     u1, v0,
        x + w, y_top + h, u1, v1,
        x,     y_top + h, u0, v1
    };

    glUseProgram(g_quad_shader.program);
    glUniformMatrix4fv(g_u_projection, 1, GL_FALSE, g_projection);
    glUniform1i(g_u_texture, 0);
    glUniform4f(
        g_u_tint,
        (float)tint.r / 255.0f,
        (float)tint.g / 255.0f,
        (float)tint.b / 255.0f,
        (float)tint.a / 255.0f
    );

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex->id);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glUseProgram(0);
    return 0;
}

int cce_draw_triangles_textured(
    unsigned int texture_id,
    const float* verts_xyuv,
    int vertex_count,
    CCE_Color tint)
{
    if (texture_id == 0 || !verts_xyuv || vertex_count <= 0) return -1;
    if ((vertex_count % 3) != 0) return -1; // triangles
    if (ensure_batch_pipeline() != 0) return -1;

    // UV convention for public GPU API: v=0 at TOP (matches stbtt atlas and stb_image default row order).
    // We keep it as-is for the GL upload path we use (no vertical flip on load).

    glUseProgram(g_quad_shader.program);
    glUniformMatrix4fv(g_u_projection, 1, GL_FALSE, g_projection);
    glUniform1i(g_u_texture, 0);
    glUniform4f(
        g_u_tint,
        (float)tint.r / 255.0f,
        (float)tint.g / 255.0f,
        (float)tint.b / 255.0f,
        (float)tint.a / 255.0f
    );

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)texture_id);

    glBindVertexArray(g_batch_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_batch_vbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)vertex_count * sizeof(float) * 4, verts_xyuv, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glUseProgram(0);
    return 0;
}


float procedural_noise(int x, int y, int seed)
{
    uint32_t n = (x * 1836311903) ^ (y * 2971215073) ^ (seed * 1073741827);
    n = (n >> 13) ^ n;
    n = (n * (n * n * 60493 + 19990303) + 1376312589);
    return (float)(n & 0x7FFFFFFF) / 2147483647.0f;
}

CCE_Layer* cce_layer_create(int screen_w, int screen_h, char * name, CCE_LayerBackend backend)
{
    if (backend == CCE_LAYER_GPU) {
        return cce_layer_gpu_create(screen_w, screen_h, name);
    } else {
        return cce_layer_cpu_create(screen_w, screen_h, name);
    }
}

CCE_Layer* cce_layer_cpu_create(int screen_w, int screen_h, char * name)
{
    CCE_Layer* layer = malloc(sizeof(CCE_Layer));
    if (!layer) return NULL;
    memset(layer, 0, sizeof(*layer));

    layer->backend = CCE_LAYER_CPU;
    layer->scr_w = screen_w;
    layer->scr_h = screen_h;
    layer->chunk_size = CHUNK_SIZE;
    layer->enabled = true;
    layer->has_dirty = true; // newly created layer uploads initial texture data
    layer->shader = NULL;
    layer->shader_mode = CCE_LAYER_SHADER_NONE;
    layer->shader_coefficient = 1.0f;
    layer->shader_dirty = 0;

    // Округление вверх для количества чанков
    layer->chunk_count_x = (screen_w + CHUNK_SIZE - 1) / CHUNK_SIZE;
    layer->chunk_count_y = (screen_h + CHUNK_SIZE - 1) / CHUNK_SIZE;

    if (name) {
        layer->name = malloc(strlen(name) + 1);
        strcpy(layer->name, name);
    } else {
        layer->name = malloc(1);
        layer->name[0] = '\0';
    }

    // Выделяем память под массив указателей на строки чанков
    layer->chunks = malloc(layer->chunk_count_y * sizeof(CCE_Chunk**));

    cce_printf("New CPU Layer: screen %dx%d, chunks %dx%d, name \"%s\"\n",
        screen_w, screen_h, layer->chunk_count_x, layer->chunk_count_y, layer->name);

    for (int y = 0; y < layer->chunk_count_y; y++) {
        layer->chunks[y] = malloc(layer->chunk_count_x * sizeof(CCE_Chunk*));
        for (int x = 0; x < layer->chunk_count_x; x++) {
            CCE_Chunk* chunk = (CCE_Chunk*)malloc(sizeof(CCE_Chunk));
            chunk->x = x;
            chunk->y = y;
            chunk->w = (x == layer->chunk_count_x - 1) ? screen_w - x * CHUNK_SIZE : CHUNK_SIZE;
            chunk->h = (y == layer->chunk_count_y - 1) ? screen_h - y * CHUNK_SIZE : CHUNK_SIZE;
            chunk->data = malloc(chunk->w * chunk->h * sizeof(CCE_Color));
            memset(chunk->data, 0, chunk->w * chunk->h * sizeof(CCE_Color));
            chunk->dirty = true;
            chunk->visible = true;
            layer->chunks[y][x] = chunk;
        }
    }

    // Создаём OpenGL текстуру
    glGenTextures(1, &layer->texture);
    glBindTexture(GL_TEXTURE_2D, layer->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, screen_w, screen_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // PBO for async uploads
    layer->pbo_size = CHUNK_SIZE * CHUNK_SIZE * 4;
    layer->current_pbo_index = 0;
    glGenBuffers(2, layer->pbo_ids);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, layer->pbo_ids[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, layer->pbo_size, NULL, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return layer;
}

CCE_Layer* cce_layer_gpu_create(int screen_w, int screen_h, char * name)
{
    if (ensure_quad_pipeline() != 0) return NULL;

    CCE_Layer* layer = malloc(sizeof(CCE_Layer));
    if (!layer) return NULL;
    memset(layer, 0, sizeof(*layer));

    layer->backend = CCE_LAYER_GPU;
    layer->scr_w = screen_w;
    layer->scr_h = screen_h;
    layer->enabled = true;
    layer->has_dirty = false;
    layer->shader = NULL;
    layer->shader_mode = CCE_LAYER_SHADER_NONE;
    layer->shader_coefficient = 1.0f;
    layer->shader_dirty = 0;

    if (name) {
        layer->name = malloc(strlen(name) + 1);
        strcpy(layer->name, name);
    } else {
        layer->name = malloc(1);
        layer->name[0] = '\0';
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_w, screen_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        if (layer->name) free(layer->name);
        free(layer);
        cce_printf("❌ GPU layer FBO incomplete for \"%s\"\n", name ? name : "");
        return NULL;
    }

    layer->texture = (unsigned int)tex;
    layer->fbo = (unsigned int)fbo;

    // Default clear to transparent.
    cce_layer_clear(layer, cce_get_color(0, 0, 0, 0, Empty));

    cce_printf("New GPU Layer: %dx%d, name \"%s\"\n", screen_w, screen_h, layer->name);
    return layer;
}

int cce_layer_begin(CCE_Layer* layer)
{
    if (!layer) return -1;
    if (layer->backend != CCE_LAYER_GPU) {
        g_active_layer = layer;
        return 0;
    }

    if (g_active_layer == layer) return 0;

    if (push_target((GLuint)layer->fbo, layer->scr_w, layer->scr_h) != 0) return -1;
    g_active_layer = layer;
    return 1;
}

int cce_layer_end(CCE_Layer* layer)
{
    if (!layer) return -1;
    if (g_active_layer != layer) return 0;

    if (layer->backend == CCE_LAYER_GPU) {
        pop_target();
    }
    g_active_layer = NULL;

    // If we bake shader on dirty, update processed texture at end of recording.
    if (layer->shader && layer->shader_mode == CCE_LAYER_SHADER_BAKE_ON_DIRTY) {
        (void)maybe_apply_layer_shader(layer, 0);
    }
    return 0;
}

int cce_layer_clear(CCE_Layer* layer, CCE_Color color)
{
    if (!layer) return -1;

    layer->shader_dirty = 1;

    if (layer->backend == CCE_LAYER_GPU) {
        int auto_wrapped = 0;
        if (g_active_layer != layer) {
            if (cce_layer_begin(layer) < 0) return -1;
            auto_wrapped = 1;
        }

        glDisable(GL_BLEND);
        const float lr = srgb_to_linear_u8(color.r);
        const float lg = srgb_to_linear_u8(color.g);
        const float lb = srgb_to_linear_u8(color.b);
        const float la = (float)color.a / 255.0f; // alpha is linear already
        glClearColor(lr, lg, lb, la);
        glClear(GL_COLOR_BUFFER_BIT);

        if (auto_wrapped) {
            cce_layer_end(layer);
        }
        return 0;
    }

    // CPU layer: clear all chunks in-place (fast path).
    const uint32_t packed =
        ((uint32_t)color.r) |
        ((uint32_t)color.g << 8) |
        ((uint32_t)color.b << 16) |
        ((uint32_t)color.a << 24);
    for (int y = 0; y < layer->chunk_count_y; y++) {
        for (int x = 0; x < layer->chunk_count_x; x++) {
            CCE_Chunk* chunk = layer->chunks[y][x];
            uint32_t* p = (uint32_t*)(void*)chunk->data;
            size_t count = (size_t)chunk->w * (size_t)chunk->h;
            for (size_t i = 0; i < count; i++) p[i] = packed;
            chunk->dirty = true;
        }
    }
    layer->has_dirty = true;
    return 0;
}

int cce_layer_set_shader(CCE_Layer* layer, const CCE_Shader* shader, CCE_LayerShaderMode mode, float coefficient)
{
    if (!layer) return -1;
    layer->shader = shader;
    layer->shader_mode = mode;
    layer->shader_coefficient = coefficient;
    layer->shader_dirty = 1;
    if (!shader) {
        layer->shader_mode = CCE_LAYER_SHADER_NONE;
        layer->shader_has_tint = 0;
    }
    if (shader && (mode == CCE_LAYER_SHADER_EACH_FRAME || mode == CCE_LAYER_SHADER_BAKE_ON_DIRTY)) {
        return ensure_layer_shader_target(layer);
    }
    return 0;
}

int cce_layer_set_shader_tint(CCE_Layer* layer, CCE_Color tint)
{
    if (!layer) return -1;
    layer->shader_has_tint = 1;
    layer->shader_tint = tint;
    layer->shader_dirty = 1;
    return 0;
}

void cce_set_pixel(CCE_Layer* layer, int screen_x, int screen_y, CCE_Color color)
{
    if (!layer) return;
    if (layer->backend == CCE_LAYER_GPU) {
        cce_set_pixel_rect(layer, screen_x, screen_y, screen_x, screen_y, color);
        return;
    }

    // Проверяем границы экрана
    if (screen_x < 0 || screen_x >= layer->scr_w || 
        screen_y < 0 || screen_y >= layer->scr_h) {
        return;
    }
    
    // Определяем, какой чанк
    int chunk_x = screen_x / layer->chunk_size;
    int chunk_y = screen_y / layer->chunk_size;
    
    // Проверяем границы чанков
    if (chunk_x >= 0 && chunk_x < layer->chunk_count_x && 
        chunk_y >= 0 && chunk_y < layer->chunk_count_y) {
        
        CCE_Chunk* chunk = layer->chunks[chunk_y][chunk_x];
        
        // Локальные координаты внутри чанка
        int local_x = screen_x % layer->chunk_size;
        int local_y = screen_y % layer->chunk_size;
        
        // Проверяем границы внутри чанка (для последнего чанка в строке/столбце)
        if (local_x >= 0 && local_x < chunk->w && 
            local_y >= 0 && local_y < chunk->h) {
            
            // Записываем пиксель
            int index = local_y * chunk->w + local_x;
            
            // Проверяем, действительно ли изменился пиксель
            CCE_Color old_color = chunk->data[index];
            if (old_color.r == color.r && old_color.g == color.g && 
                old_color.b == color.b && old_color.a == color.a) {
                return;  // Пиксель не изменился, пропускаем
            }
            
            chunk->data[index] = color;
            
            // Помечаем весь чанк как грязный
            chunk->dirty = true;
            layer->has_dirty = true;
            layer->shader_dirty = 1;
        }
    }
}

void cce_set_pixel_rect(CCE_Layer* layer, int x0, int y0, int x1, int y1, CCE_Color color)
{
    if (!layer) return;
    if (layer->backend == CCE_LAYER_GPU)
    {
        // For GPU layers, treat rect coordinates in top-left pixel space (same as CPU layer storage).
        // We draw a tinted quad using a 1x1 white texture.
        ensure_white_texture();

        // Normalize / clamp.
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= layer->scr_w) x1 = layer->scr_w - 1;
        if (y1 >= layer->scr_h) y1 = layer->scr_h - 1;
        if (x0 > x1 || y0 > y1) return;

        int auto_wrapped = 0;
        if (g_active_layer != layer) {
            if (cce_layer_begin(layer) < 0) return;
            auto_wrapped = 1;
        }

        const float fx0 = (float)x0;
        const float fy0 = (float)y0;
        const float fx1 = (float)(x1 + 1);
        const float fy1 = (float)(y1 + 1);
        const float verts[24] = {
            fx0, fy0, 0.0f, 0.0f,
            fx1, fy0, 1.0f, 0.0f,
            fx1, fy1, 1.0f, 1.0f,

            fx1, fy1, 1.0f, 1.0f,
            fx0, fy1, 0.0f, 1.0f,
            fx0, fy0, 0.0f, 0.0f,
        };
        (void)cce_draw_triangles_textured(g_white_tex, verts, 6, color);

        layer->shader_dirty = 1;

        if (auto_wrapped) {
            cce_layer_end(layer);
        }
        return;
    }

    // Нормализуем координаты
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    
    // Ограничиваем границы
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= layer->scr_w) x1 = layer->scr_w - 1;
    if (y1 >= layer->scr_h) y1 = layer->scr_h - 1;
    
    if (x0 > x1 || y0 > y1) return;

    // Fast path for solid fills: write packed 32-bit pixels and mark chunk dirty once.
    // This is especially useful for clears/rect fills (e.g. UI animated regions).
    const uint32_t packed =
        ((uint32_t)color.r) |
        ((uint32_t)color.g << 8) |
        ((uint32_t)color.b << 16) |
        ((uint32_t)color.a << 24);
    
    // Определяем затронутые чанки
    int chunk_x0 = x0 / layer->chunk_size;
    int chunk_y0 = y0 / layer->chunk_size;
    int chunk_x1 = x1 / layer->chunk_size;
    int chunk_y1 = y1 / layer->chunk_size;
    
    // Заполняем область батчем
    for (int cy = chunk_y0; cy <= chunk_y1; cy++) {
        for (int cx = chunk_x0; cx <= chunk_x1; cx++) {
            if (cx < 0 || cx >= layer->chunk_count_x || 
                cy < 0 || cy >= layer->chunk_count_y) continue;
            
            CCE_Chunk* chunk = layer->chunks[cy][cx];
            
            // Вычисляем область пересечения
            int chunk_screen_x = cx * layer->chunk_size;
            int chunk_screen_y = cy * layer->chunk_size;
            
            int local_x0 = (x0 > chunk_screen_x) ? (x0 - chunk_screen_x) : 0;
            int local_y0 = (y0 > chunk_screen_y) ? (y0 - chunk_screen_y) : 0;
            int local_x1 = (x1 < chunk_screen_x + chunk->w) ? 
                           (x1 - chunk_screen_x) : (chunk->w - 1);
            int local_y1 = (y1 < chunk_screen_y + chunk->h) ? 
                           (y1 - chunk_screen_y) : (chunk->h - 1);
            
            // Fill rows with packed pixels.
            uint32_t* row0 = (uint32_t*)(void*)chunk->data;
            const int span = local_x1 - local_x0 + 1;
            int any = 0;
            for (int ly = local_y0; ly <= local_y1; ly++) {
                uint32_t* p = row0 + (size_t)ly * (size_t)chunk->w + (size_t)local_x0;
                for (int i = 0; i < span; i++) {
                    p[i] = packed;
                }
                any = 1;
            }
            if (any) {
                chunk->dirty = true;
                layer->has_dirty = true;
                layer->shader_dirty = 1;
            }
        }
    }
}


void update_dirty_chunks(CCE_Layer* layer)
{
    if (!layer) return;
    if (!layer->has_dirty) return;
    
    glBindTexture(GL_TEXTURE_2D, layer->texture);
    
    int updated = 0;
    
    // Собираем список грязных чанков
    enum { DIRTY_CAP = 1024 };
    CCE_Chunk* dirty_chunks[DIRTY_CAP];  // Максимум DIRTY_CAP чанков за кадр
    int dirty_count = 0;
    int has_more = 0;
    
    for (int y = 0; y < layer->chunk_count_y; y++) {
        for (int x = 0; x < layer->chunk_count_x; x++) {
            CCE_Chunk* chunk = layer->chunks[y][x];
            
            if (chunk->dirty && chunk->visible) {
                if (dirty_count < DIRTY_CAP) {
                    dirty_chunks[dirty_count++] = chunk;
                } else {
                    has_more = 1;
                }
            }
        }
    }
    
    // Обновляем чанки через PBO (асинхронно)
    for (int i = 0; i < dirty_count; i++) {
        CCE_Chunk* chunk = dirty_chunks[i];
        
        int screen_x = chunk->x * layer->chunk_size;
        int screen_y = chunk->y * layer->chunk_size;
        
        int chunk_data_size = chunk->w * chunk->h * 4;  // RGBA
        
        // Если размер чанка больше PBO, используем прямой метод
        if (chunk_data_size > layer->pbo_size) {
            // Fallback на синхронный метод для больших чанков
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                           screen_x, screen_y,
                           chunk->w, chunk->h,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           chunk->data);
        } else {
            // Переключаемся на следующий PBO (двойная буферизация)
            layer->current_pbo_index = (layer->current_pbo_index + 1) % 2;
            int pbo_index = layer->pbo_ids[layer->current_pbo_index];
            
            // Привязываем PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_index);
            
            // Записываем данные в PBO
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, chunk_data_size, chunk->data);
            
            // Загружаем из PBO в текстуру (асинхронная операция!)
            // offset = 0, потому что мы записали данные в начало PBO
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                           screen_x, screen_y,
                           chunk->w, chunk->h,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           0);  // offset = 0, данные берутся из привязанного PBO
            
            // Отвязываем PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        
        chunk->dirty = false;
        updated++;
    }

    if (updated > 0 && CCE_DEBUG == 1) {
        cce_printf("Dirty chunks updated: %d/%d on %s\n", 
                   updated, layer->chunk_count_x * layer->chunk_count_y, layer->name);
    }

    // If we found no remaining dirty chunks (or we updated all of them), we can skip scanning next frame.
    if (!has_more && dirty_count == 0) {
        layer->has_dirty = false;
    } else if (!has_more && dirty_count > 0) {
        // We scanned full layer and updated all dirty chunks we saw; no more remain.
        layer->has_dirty = false;
    } else {
        // Some dirty chunks remain beyond our cap.
        layer->has_dirty = true;
    }
}

void render_layer(CCE_Layer* layer)
{
    if (!layer) return;
    if (ensure_quad_pipeline() != 0) return;

    if (layer->backend == CCE_LAYER_CPU) {
        update_dirty_chunks(layer);
    }

    // For GPU layers the texture is rendered with top-left origin during baking,
    // but GL sampling treats v=0 as bottom. Flip V when compositing GPU layers.
    const float v0 = (layer->backend == CCE_LAYER_GPU) ? 1.0f : 0.0f;
    const float v1 = (layer->backend == CCE_LAYER_GPU) ? 0.0f : 1.0f;
    float verts[16] = {
        0.0f,                0.0f,                 0.0f, v0,
        (float)layer->scr_w, 0.0f,                 1.0f, v0,
        (float)layer->scr_w, (float)layer->scr_h,  1.0f, v1,
        0.0f,                (float)layer->scr_h,  0.0f, v1
    };

    glUseProgram(g_quad_shader.program);
    glUniformMatrix4fv(g_u_projection, 1, GL_FALSE, g_projection);
    glUniform1i(g_u_texture, 0);
    glUniform4f(g_u_tint, 1.0f, 1.0f, 1.0f, 1.0f);

    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer->texture);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

void render_pie(CCE_Layer** layers, int count)
{
    if (!layers || count <= 0) return;
    if (ensure_quad_pipeline() != 0) return;

    for (int i = 0; i < count; i++) {
        if (!layers[i] || !layers[i]->enabled) continue;
        if (layers[i]->backend == CCE_LAYER_CPU) {
            update_dirty_chunks(layers[i]);
        }
    }

    glUseProgram(g_quad_shader.program);
    glUniformMatrix4fv(g_u_projection, 1, GL_FALSE, g_projection);
    glUniform1i(g_u_texture, 0);
    glUniform4f(g_u_tint, 1.0f, 1.0f, 1.0f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(g_quad_vao);

    for (int i = 0; i < count; i++) {
        if (!layers[i] || !layers[i]->enabled) continue;

        // Optional per-layer shader pass.
        // - each-frame mode: run shader every render.
        // - bake-on-dirty: recompute only when content changed / cleared.
        if (layers[i]->shader && layers[i]->shader_mode != CCE_LAYER_SHADER_NONE) {
            const int each_frame = (layers[i]->shader_mode == CCE_LAYER_SHADER_EACH_FRAME) ? 1 : 0;
            (void)maybe_apply_layer_shader(layers[i], each_frame);
        }

        const GLuint tex_to_draw = (layers[i]->shader && layers[i]->shader_mode != CCE_LAYER_SHADER_NONE && layers[i]->shader_texture != 0)
            ? (GLuint)layers[i]->shader_texture
            : (GLuint)layers[i]->texture;

        const float v0 = (layers[i]->backend == CCE_LAYER_GPU) ? 1.0f : 0.0f;
        const float v1 = (layers[i]->backend == CCE_LAYER_GPU) ? 0.0f : 1.0f;
        float verts[16] = {
            0.0f,                    0.0f,                     0.0f, v0,
            (float)layers[i]->scr_w, 0.0f,                     1.0f, v0,
            (float)layers[i]->scr_w, (float)layers[i]->scr_h,  1.0f, v1,
            0.0f,                    (float)layers[i]->scr_h,  0.0f, v1
        };

        glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_to_draw);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void cce_layer_destroy(CCE_Layer* layer)
{
    if (!layer) return;

    if (g_active_layer == layer) {
        (void)cce_layer_end(layer);
    }

    if (layer->backend == CCE_LAYER_CPU) {
        if (layer->pbo_ids[0] != 0 || layer->pbo_ids[1] != 0) {
            glDeleteBuffers(2, layer->pbo_ids);
        }
        for (int y = 0; y < layer->chunk_count_y; y++) {
            for (int x = 0; x < layer->chunk_count_x; x++) {
                free(layer->chunks[y][x]->data);
                free(layer->chunks[y][x]);
            }
            free(layer->chunks[y]);
        }
        free(layer->chunks);
    } else {
        if (layer->fbo) {
            GLuint f = (GLuint)layer->fbo;
            glDeleteFramebuffers(1, &f);
        }
    }

    if (layer->shader_fbo) {
        GLuint f = (GLuint)layer->shader_fbo;
        glDeleteFramebuffers(1, &f);
    }
    if (layer->shader_texture) {
        GLuint t = (GLuint)layer->shader_texture;
        glDeleteTextures(1, &t);
    }

    cce_printf("Destroying Layer: name \"%s\"\n", layer->name ? layer->name : "");

    if (layer->name) { free(layer->name); }
    if (layer->texture) {
        GLuint t = (GLuint)layer->texture;
        glDeleteTextures(1, &t);
    }
    free(layer);
}

void test_simple()
{
    // Тест 1: Простая текстура без чанков
    int w = 550, h = 550;
    CCE_Color* pixels = malloc(w * h * sizeof(CCE_Color));
    
    // Заливаем белым
    for (int i = 0; i < w * h; i++) {
        pixels[i] = cce_get_color(255, 255, 255, 255, DefaultLight);
    }
    
    // Красный квадрат 4x4 в центре
    int center_x = w/2, center_y = h/2;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int idx = (center_y + dy) * w + (center_x + dx);
            pixels[idx] = cce_get_color(255, 0, 0, 255, Red);
        }
    }
    
    // Создаём текстуру
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (ensure_quad_pipeline() == 0)
    {
        float verts[16] = {
            0.0f, 0.0f, 0.0f, 0.0f,
            (float)w, 0.0f, 1.0f, 0.0f,
            (float)w, (float)h, 1.0f, 1.0f,
            0.0f, (float)h, 0.0f, 1.0f
        };

        glUseProgram(g_quad_shader.program);
        glUniformMatrix4fv(g_u_projection, 1, GL_FALSE, g_projection);
        glUniform1i(g_u_texture, 0);

        glBindVertexArray(g_quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        glUseProgram(0);
    }

    glDeleteTextures(1, &texture);
    free(pixels);
}

CCE_Color cce_get_color(int pos_x, int pos_y, int offset_x, int offset_y, CCE_Palette palette, ...)
{
    CCE_Color ret;
    pct noise = (pct) ((procedural_noise(pos_x + offset_x, pos_y + offset_y, engine_seed + palette)) * 255);

    switch (palette)
    {
        case Shadow:
            va_list shadow;
            va_start(shadow, palette);
            ret.a = (pct) va_arg(shadow, int);
            va_end(shadow);
            ret.r = 0;
            ret.g = 0;
            ret.b = 0;
            break;

        case Alpha:
            va_list alpha;
            va_start(alpha, palette);
            ret.a = (pct) va_arg(alpha, int);
            va_end(alpha);
            ret.r = 255;
            ret.g = 255;
            ret.b = 255;
            break;

        case Empty:
            ret.r = 0;
            ret.g = 0;
            ret.b = 0;
            ret.a = 0;
            break;

        case Full:
            ret.r = 255;
            ret.g = 255;
            ret.b = 255;
            ret.a = 255;
            break;
            
        case Red:
            ret.r = 255;
            ret.g = 0;
            ret.b = 0;
            ret.a = 255;
            break;

        case Green:
            ret.r = 0;
            ret.g = 255;
            ret.b = 0;
            ret.a = 255;
            break;

        case Blue:
            ret.r = 0;
            ret.g = 0;
            ret.b = 255;
            ret.a = 255;
            break;
            
        case Manual:
            va_list rgb;
            va_start(rgb, palette);
            ret.r = (pct) va_arg(rgb, int);
            ret.g = (pct) va_arg(rgb, int);
            ret.b = (pct) va_arg(rgb, int);
            ret.a = (pct) va_arg(rgb, int);
            va_end(rgb);
            break;

        case DefaultGrass:
            noise = noise < 80 ? 80: noise;
            noise = noise > 255 ? 255: noise;
            noise = COMPRESS(noise, 80, 150);
            ret.r = noise * 0.7f;
            ret.g = noise;
            ret.b = noise * 0.1f;
            ret.a = 255;
            break;

        case DefaultStone:
            noise = noise < 120 ? 120: noise;
            noise = noise > 255 ? 255: noise;
            ret.r = noise * 0.4f;
            ret.g = noise * 0.4f;
            ret.b = noise * 0.4f;
            ret.a = 255;
            break;

        case DefaultDark:
            ret.r = 20u;
            ret.g = 20u;
            ret.b = 20u;
            ret.a = 255;
            break;

        case DefaultLight:
            ret.r = 225u;
            ret.g = 225u;
            ret.b = 225u;
            ret.a = 255;
            break;

        case DefaultLeaves:
            // if (noise < 0.1f) { noise = 0.2f; }
            // if (noise < 0.5f) { noise = noise * 5.0f; }
            // if (noise >= 1.0f) { noise = 0.6f; }
            // ret.r = noise - 0.4f;
            // ret.g = noise - 0.2f;
            // ret.b = noise - 0.9f;
            // ret.a = 0.85f;

            ret.r = 255;
            ret.g = 255;
            ret.b = 255;
            ret.a = 255;

            break;

        case DefaultCloud:
            ret.r = noise;
            ret.g = noise;
            ret.b = noise;
            ret.a = 80u;
            break;

        default:
            ret.r = 0;
            ret.g = 0;
            ret.b = 0;
            ret.a = 255;
            break;
    }
    return ret;
}

