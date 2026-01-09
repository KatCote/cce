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
#include "shader.h"
#include "../engine.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* DEFAULT_VS =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aUV;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char* DEFAULT_FS_TINT =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec4 uTint;\n"
    "uniform float uCoefficient;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec4 c = texture(uTexture, vUV);\n"
    "    vec3 tinted = c.rgb * uTint.rgb;\n"
    "    vec3 mixed = mix(c.rgb, tinted, clamp(uCoefficient, 0.0, 1.0));\n"
    "    float alpha = c.a * mix(1.0, uTint.a, clamp(uCoefficient, 0.0, 1.0));\n"
    "    FragColor = vec4(mixed, alpha);\n"
    "}\n";

static const char* DEFAULT_FS_GRAYSCALE =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "uniform sampler2D uTexture;\n"
    "uniform float uCoefficient;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec4 c = texture(uTexture, vUV);\n"
    "    float g = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
    "    vec3 mixed = mix(c.rgb, vec3(g), clamp(uCoefficient, 0.0, 1.0));\n"
    "    FragColor = vec4(mixed, c.a);\n"
    "}\n";

static const char* DEFAULT_FS_GLOW =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec2 uResolution;\n"
    "uniform float uCoefficient;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec2 texel = 1.0 / uResolution;\n"
    "    vec4 sum = vec4(0.0);\n"
    "    float weights[5] = float[](0.204164, 0.304005, 0.093913, 0.01856, 0.004429);\n"
    "    for (int x = -2; x <= 2; x++) {\n"
    "        for (int y = -2; y <= 2; y++) {\n"
    "            float w = weights[abs(x)] * weights[abs(y)];\n"
    "            sum += texture(uTexture, vUV + vec2(x, y) * texel) * w;\n"
    "        }\n"
    "    }\n"
    "    vec4 base = texture(uTexture, vUV);\n"
    "    vec3 glow = sum.rgb * clamp(uCoefficient, 0.0, 2.5);\n"
    "    vec3 finalColor = base.rgb + glow;\n"
    "    float alpha = max(base.a, sum.a * clamp(uCoefficient, 0.0, 1.0));\n"
    "    FragColor = vec4(finalColor, alpha);\n"
    "}\n";

static const char* DEFAULT_FS_BLOOM =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec2 uResolution;\n"
    "uniform float uCoefficient;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec2 texel = 1.0 / uResolution;\n"
    "    vec3 base = texture(uTexture, vUV).rgb;\n"
    "    float luma = dot(base, vec3(0.2126, 0.7152, 0.0722));\n"
    "    vec3 thresholded = luma > 0.75 ? base : vec3(0.0);\n"
    "    float w0 = 0.227027;\n"
    "    float w1 = 0.1945946;\n"
    "    float w2 = 0.1216216;\n"
    "    vec3 blur = thresholded * w0;\n"
    "    blur += texture(uTexture, vUV + texel * vec2(1, 0)).rgb * w1;\n"
    "    blur += texture(uTexture, vUV - texel * vec2(1, 0)).rgb * w1;\n"
    "    blur += texture(uTexture, vUV + texel * vec2(0, 1)).rgb * w1;\n"
    "    blur += texture(uTexture, vUV - texel * vec2(0, 1)).rgb * w1;\n"
    "    blur += texture(uTexture, vUV + texel * vec2(2, 0)).rgb * w2;\n"
    "    blur += texture(uTexture, vUV - texel * vec2(2, 0)).rgb * w2;\n"
    "    blur += texture(uTexture, vUV + texel * vec2(0, 2)).rgb * w2;\n"
    "    blur += texture(uTexture, vUV - texel * vec2(0, 2)).rgb * w2;\n"
    "    float coeff = clamp(uCoefficient, 0.0, 2.5);\n"
    "    vec3 color = base + blur * coeff;\n"
    "    FragColor = vec4(color, 1.0);\n"
    "}\n";

static GLuint g_pp_vao = 0;
static GLuint g_pp_vbo = 0;
static GLuint g_pp_ebo = 0;

// Forward declaration for vararg uniform helper used before its definition.
static void apply_varargs(const CCE_Shader* shader, int arg_count, va_list args);

static char* read_file_to_buffer(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read_bytes] = '\0';
    return buf;
}

static int compile_shader(GLenum type, const char* src, GLuint* out_id, const char* name)
{
    GLuint shader = glCreateShader(type);
    if (!shader) return -1;

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        cce_printf("❌ Shader compile failed (%s): %s\n", name ? name : "unnamed", log);
        glDeleteShader(shader);
        return -1;
    }

    *out_id = shader;
    return 0;
}

static int link_program(GLuint vs, GLuint fs, GLuint* out_program, const char* name)
{
    GLuint program = glCreateProgram();
    if (!program) return -1;

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        cce_printf("❌ Program link failed (%s): %s\n", name ? name : "unnamed", log);
        glDeleteProgram(program);
        return -1;
    }

    *out_program = program;
    return 0;
}

static void cache_uniforms(CCE_Shader* shader)
{
    shader->uniform_texture = glGetUniformLocation(shader->program, "uTexture");
    shader->uniform_coeff = glGetUniformLocation(shader->program, "uCoefficient");
    shader->uniform_tint = glGetUniformLocation(shader->program, "uTint");
    shader->uniform_resolution = glGetUniformLocation(shader->program, "uResolution");
}

static int ensure_postprocess_quad(void)
{
    if (g_pp_vao != 0) return 0;

    const float quad[] = {
        -1.0f,  1.0f, 0.0f, 0.0f, // top-left
         1.0f,  1.0f, 1.0f, 0.0f, // top-right
         1.0f, -1.0f, 1.0f, 1.0f, // bottom-right
        -1.0f, -1.0f, 0.0f, 1.0f  // bottom-left
    };

    const unsigned int indices[6] = {0, 1, 2, 2, 3, 0};

    glGenVertexArrays(1, &g_pp_vao);
    glGenBuffers(1, &g_pp_vbo);
    glGenBuffers(1, &g_pp_ebo);

    glBindVertexArray(g_pp_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_pp_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_pp_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return 0;
}

static int cce_shader_render_to_fbo(const CCE_Shader* shader, unsigned int src_texture, unsigned int dst_fbo, int dst_w, int dst_h, float coefficient, int arg_count, va_list args)
{
    if (!shader || !shader->loaded) return -1;
    if (dst_w <= 0 || dst_h <= 0) return -1;
    if (ensure_postprocess_quad() != 0) return -1;

    // Save viewport + FBO.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint prev_fbo = 0;
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)dst_fbo);
    glViewport(0, 0, dst_w, dst_h);

    // Clear destination to avoid additive accumulation between frames.
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader->program);

    if (shader->uniform_texture >= 0) glUniform1i(shader->uniform_texture, 0);
    if (shader->uniform_coeff >= 0) glUniform1f(shader->uniform_coeff, coefficient);
    if (shader->uniform_resolution >= 0) glUniform2f(shader->uniform_resolution, (float)dst_w, (float)dst_h);

    if (shader->uniform_tint >= 0 && shader->type != CCE_SHADER_TINT && arg_count == 0)
    {
        glUniform4f(shader->uniform_tint, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (arg_count > 0) {
        apply_varargs(shader, arg_count, args);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)src_texture);

    glBindVertexArray(g_pp_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    // Restore.
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    return 0;
}

static int build_shader(CCE_Shader* out, const char* vs_src, const char* fs_src, CCE_ShaderType type, const char* name, const char* path)
{
    GLuint vs = 0, fs = 0, program = 0;
    if (compile_shader(GL_VERTEX_SHADER, vs_src, &vs, name) != 0) return -1;
    if (compile_shader(GL_FRAGMENT_SHADER, fs_src, &fs, name) != 0)
    {
        glDeleteShader(vs);
        return -1;
    }

    if (link_program(vs, fs, &program, name) != 0)
    {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return -1;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    memset(out, 0, sizeof(*out));
    out->program = program;
    out->type = type;
    out->vertex_id = 0;
    out->fragment_id = 0;
    out->loaded = 1;
    if (name) { strncpy(out->name, name, sizeof(out->name) - 1); }
    if (path) { strncpy(out->path, path, sizeof(out->path) - 1); }

    cache_uniforms(out);
    return 0;
}

int cce_shader_load_from_file(CCE_Shader* out, const char* path, CCE_ShaderType type, const char* name)
{
    if (!out) return -1;

    const char* fragment_src = NULL;
    char* owned_fragment = NULL;

    if (path)
    {
        owned_fragment = read_file_to_buffer(path);
        if (!owned_fragment)
        {
            cce_printf("❌ Failed to read shader file: %s\n", path);
            return -1;
        }
        fragment_src = owned_fragment;
    }
    else
    {
        switch (type)
        {
            case CCE_SHADER_TINT: fragment_src = DEFAULT_FS_TINT; break;
            case CCE_SHADER_GRAYSCALE: fragment_src = DEFAULT_FS_GRAYSCALE; break;
            case CCE_SHADER_GLOW: fragment_src = DEFAULT_FS_GLOW; break;
            case CCE_SHADER_BLOOM: fragment_src = DEFAULT_FS_BLOOM; break;
            default: fragment_src = DEFAULT_FS_TINT; break;
        }
    }

    int result = build_shader(out, DEFAULT_VS, fragment_src, type, name, path);
    if (owned_fragment) free(owned_fragment);
    return result;
}

void cce_shader_unload(CCE_Shader* shader)
{
    if (shader && shader->loaded)
    {
        glDeleteProgram(shader->program);
        shader->program = 0;
        shader->loaded = 0;
    }
}

static void apply_varargs(const CCE_Shader* shader, int arg_count, va_list args)
{
    if (!shader || shader->uniform_tint < 0 || arg_count <= 0) return;
    float vals[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    int count = arg_count > 4 ? 4 : arg_count;
    for (int i = 0; i < count; i++)
    {
        vals[i] = (float)va_arg(args, double);
    }
    glUniform4fv(shader->uniform_tint, 1, vals);
}

static int cce_shader_render(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, int arg_count, va_list args)
{
    if (!shader || !layer || !shader->loaded) return -1;
    if (ensure_postprocess_quad() != 0) return -1;

    cce_render_prepare_layer(layer);

    glUseProgram(shader->program);

    if (shader->uniform_texture >= 0) glUniform1i(shader->uniform_texture, 0);
    if (shader->uniform_coeff >= 0) glUniform1f(shader->uniform_coeff, coefficient);
    if (shader->uniform_resolution >= 0) glUniform2f(shader->uniform_resolution, (float)layer->scr_w, (float)layer->scr_h);

    if (shader->uniform_tint >= 0 && shader->type != CCE_SHADER_TINT && arg_count == 0)
    {
        glUniform4f(shader->uniform_tint, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (arg_count > 0) { apply_varargs(shader, arg_count, args); }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer->texture);

    glBindVertexArray(g_pp_vao);
    glEnable(GL_BLEND);
    if (shader->type == CCE_SHADER_GLOW || shader->type == CCE_SHADER_BLOOM)
    {
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE); // additive for light effects
    }
    else
    {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
    glBindVertexArray(0);

    glUseProgram(0);
    return 0;
}

static int cce_shader_render_with_radius(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, float radius)
{
    if (!shader || !layer || !shader->loaded) return -1;
    if (ensure_postprocess_quad() != 0) return -1;

    cce_render_prepare_layer(layer);

    glUseProgram(shader->program);

    if (shader->uniform_texture >= 0) glUniform1i(shader->uniform_texture, 0);
    if (shader->uniform_coeff >= 0) glUniform1f(shader->uniform_coeff, coefficient);
    if (shader->uniform_resolution >= 0) glUniform2f(shader->uniform_resolution, (float)layer->scr_w, (float)layer->scr_h);
    if (shader->uniform_tint >= 0 && shader->type != CCE_SHADER_TINT)
    {
        glUniform4f(shader->uniform_tint, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    GLint radius_loc = glGetUniformLocation(shader->program, "uRadius");
    if (radius_loc >= 0) glUniform1f(radius_loc, radius);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer->texture);

    glBindVertexArray(g_pp_vao);
    glEnable(GL_BLEND);
    if (shader->type == CCE_SHADER_GLOW || shader->type == CCE_SHADER_BLOOM)
    {
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE); // additive for light effects
    }
    else
    {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
    glBindVertexArray(0);

    glUseProgram(0);
    return 0;
}

int cce_shader_apply(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, int arg_count, ...)
{
    va_list args;
    va_start(args, arg_count);
    int res = cce_shader_render(shader, layer, coefficient, arg_count, args);
    va_end(args);
    return res;
}

int cce_shader_apply_tint(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, CCE_Color tint)
{
    if (!shader || !shader->loaded || shader->uniform_tint < 0) return -1;
    glUseProgram(shader->program);
    glUniform4f(shader->uniform_tint,
                tint.r / 255.0f,
                tint.g / 255.0f,
                tint.b / 255.0f,
                tint.a / 255.0f);
    glUseProgram(0);
    return cce_shader_apply(shader, layer, coefficient, 0);
}

int cce_shader_apply_grayscale(const CCE_Shader* shader, CCE_Layer* layer, float coefficient)
{
    return cce_shader_apply(shader, layer, coefficient, 0);
}

int cce_shader_apply_glow(const CCE_Shader* shader, CCE_Layer* layer, float coefficient)
{
    return cce_shader_apply(shader, layer, coefficient, 0);
}

int cce_shader_apply_bloom(const CCE_Shader* shader, CCE_Layer* layer, float coefficient)
{
    return cce_shader_apply(shader, layer, coefficient, 0);
}

int cce_shader_apply_bloom_radius(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, float radius)
{
    return cce_shader_render_with_radius(shader, layer, coefficient, radius);
}

int cce_shader_apply_to_texture(
    const CCE_Shader* shader,
    unsigned int src_texture,
    unsigned int dst_fbo,
    int dst_w,
    int dst_h,
    float coefficient,
    int arg_count,
    ...)
{
    va_list args;
    va_start(args, arg_count);
    int res = cce_shader_render_to_fbo(shader, src_texture, dst_fbo, dst_w, dst_h, coefficient, arg_count, args);
    va_end(args);
    return res;
}

int cce_shader_create_from_source(CCE_Shader* out, const char* vertex_src, const char* fragment_src, const char* name)
{
    if (!out || !vertex_src || !fragment_src) return -1;
    return build_shader(out, vertex_src, fragment_src, CCE_SHADER_CUSTOM, name, NULL);
}

int cce_shader_create_from_files(CCE_Shader* out, const char* vertex_path, const char* fragment_path, const char* name)
{
    if (!out || !vertex_path || !fragment_path) return -1;
    char* vsrc = read_file_to_buffer(vertex_path);
    char* fsrc = read_file_to_buffer(fragment_path);
    if (!vsrc || !fsrc)
    {
        if (vsrc) free(vsrc);
        if (fsrc) free(fsrc);
        cce_printf("❌ Failed to read shader files: %s, %s\n", vertex_path, fragment_path);
        return -1;
    }

    int result = build_shader(out, vsrc, fsrc, CCE_SHADER_CUSTOM, name, NULL);
    free(vsrc);
    free(fsrc);
    return result;
}