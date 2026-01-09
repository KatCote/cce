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

#ifndef CCE_GUARD_H
#define CCE_GUARD_H

#define CCE_VERSION "0.2.2"
#define CCE_VERNAME "Embryo"
#define CCE_NAME    "CastleCore Engine"

/* GLFW */

typedef struct Window Window;

/* CCE */

// pct - Pixel Color Type
#define pct  unsigned char
#define bool unsigned char

/*
    I N I T
*/

typedef struct RandPack
{
    int r0;
    int r1;
    int r2;
    int r3;
    int r4;
    int r5;
    int r6;
    int r7;
    int r8;
    int r9;
} RandPack;

typedef enum RandPackIndex
{
    R0 = 0,
    R1 = 1,
    R2 = 2,
    R3 = 3,
    R4 = 4,
    R5 = 5,
    R6 = 6,
    R7 = 7,
    R8 = 8,
    R9 = 9,
} RandPackIndex;

int cce_engine_init(void);
void cce_engine_cleanup(void);
int cce_get_version(char ** ver_str_ptr);

void set_engine_seed(long new_engine_seed);
long get_engine_seed(void);
void set_engine_msaa(int factor);
int get_engine_chunk_size(void);
int get_randpack_value(RandPackIndex index);

/*
    W I N D O W
*/

typedef enum
{
    CCE_WINDOW_WINDOWED = 0,
    CCE_WINDOW_FULLSCREEN = 1,
} CCE_WindowMode;

// Cursor types (GLFW-backed). `CCE_CURSOR_CUSTOM` uses an image set via cce_window_set_cursor_image.
// Note: HIDDEN/DISABLED are modes (GLFW_CURSOR_HIDDEN / GLFW_CURSOR_DISABLED).
typedef enum
{
    CCE_CURSOR_ARROW = 0,
    CCE_CURSOR_IBEAM = 1,
    CCE_CURSOR_CROSSHAIR = 2,
    CCE_CURSOR_HAND = 3,
    CCE_CURSOR_HRESIZE = 4,
    CCE_CURSOR_VRESIZE = 5,
    CCE_CURSOR_CUSTOM = 6,
    CCE_CURSOR_HIDDEN = 7,
    CCE_CURSOR_DISABLED = 8,
    CCE_CURSOR_COUNT = 9, // internal sentinel
} CCE_CursorType;

typedef struct
{
    const char* title;
    int width;          // requested window size (or fullscreen mode size)
    int height;         // requested window size (or fullscreen mode size)
    int monitor_index;  // used when mode == CCE_WINDOW_FULLSCREEN (or for centering)
    int batch_size;     // user-defined scale factor for tests/demos (e.g. sprite pixel batch)
    CCE_WindowMode mode;
} CCE_WindowConfig;

Window* cce_window_create(int width, int height, const char* title);
Window* cce_window_create_ex(const CCE_WindowConfig* cfg);
void cce_window_destroy(Window* window);
int cce_window_should_close(const Window* window);
void cce_window_poll_events(void);
void cce_window_swap_buffers(Window* window);
void cce_window_make_current(Window* window);
int cce_window_get_size(const Window* window, int* out_w, int* out_h);
int cce_window_get_batch_size(const Window* window);

// Cursor control
// Sets the "base" cursor for the window. If hover override is enabled and the cursor is inside the window,
// the hover cursor is used instead.
int cce_window_set_cursor(Window* window, CCE_CursorType type);

// Enables/disables cursor override while the mouse cursor is inside the window.
// When enabled, the cursor switches to `type` on enter and reverts to the base cursor on leave.
int cce_window_set_cursor_on_hover(Window* window, int enabled, CCE_CursorType type);

// Sets a custom cursor image (typically PNG). Used when cursor type is `CCE_CURSOR_CUSTOM`.
// hot_x/hot_y are the cursor hotspot coordinates in pixels.
int cce_window_set_cursor_image(Window* window, const char* filename, int hot_x, int hot_y);
void cce_window_clear_cursor_image(Window* window);

/*
    R E N D E R
*/

typedef struct
{
    pct r;
    pct g;
    pct b;
    pct a;
} CCE_Color;

/*
    G P U   2 D
*/

typedef struct
{
    unsigned int id; // OpenGL texture id
    int width;
    int height;
} CCE_Texture;

// Forward declarations to avoid circular headers.
typedef struct CCE_Layer CCE_Layer;
typedef struct CCE_Shader CCE_Shader;

// Loads an image file into an OpenGL texture. Requires an active GL context.
int cce_texture_load(CCE_Texture* out, const char* filename);
void cce_texture_free(CCE_Texture* tex);

// Draw a sub-rectangle of the texture as a quad in screen space (top-left origin).
// UVs are normalized [0..1].
int cce_draw_texture_region(
    const CCE_Texture* tex,
    float x, float y,
    float w, float h,
    float u0, float v0,
    float u1, float v1,
    CCE_Color tint
);

// Low-level batched draw: vertices are an array of (x,y,u,v) floats, `vertex_count` is number of vertices.
// Requires an active GL context + projection set by cce_setup_2d_projection.
int cce_draw_triangles_textured(
    unsigned int texture_id,
    const float* verts_xyuv,
    int vertex_count,
    CCE_Color tint
);

typedef struct
{
    int x, y;
    int w, h;
    CCE_Color* data;
    bool dirty;
    bool visible;
} CCE_Chunk;

typedef enum
{
    CCE_LAYER_CPU = 0,
    CCE_LAYER_GPU = 1,
} CCE_LayerBackend;

typedef enum
{
    CCE_LAYER_SHADER_NONE = 0,
    // Apply shader every frame (does not modify base; uses an internal processed texture).
    CCE_LAYER_SHADER_EACH_FRAME = 1,
    // Recompute processed texture only when the layer changes / is cleared.
    CCE_LAYER_SHADER_BAKE_ON_DIRTY = 2,
} CCE_LayerShaderMode;

struct CCE_Layer
{
    int scr_w, scr_h;
    char* name;
    int layer_id;
    bool enabled;

    // Backend selector.
    CCE_LayerBackend backend;

    // === Shared output ===
    // Base texture that `render_pie` draws (and that legacy shader APIs sample).
    unsigned int texture;

    // === CPU backend data (legacy / software layer) ===
    int chunk_size;
    int chunk_count_x, chunk_count_y;
    CCE_Chunk*** chunks;
    bool has_dirty; // fast-path: if false, skip scanning chunks for updates
    unsigned int pbo_ids[2];
    int current_pbo_index;
    int pbo_size;

    // === GPU backend data (render-target layer) ===
    unsigned int fbo; // framebuffer that renders into `texture`

    // === Per-layer postprocess ===
    const CCE_Shader* shader;
    CCE_LayerShaderMode shader_mode;
    float shader_coefficient;
    int shader_has_tint;
    CCE_Color shader_tint;
    unsigned int shader_texture; // processed texture (output of shader)
    unsigned int shader_fbo;     // framebuffer for shader_texture
    int shader_dirty;
};

typedef enum CCE_Palette
{
    Red             = -3,
    Green           = -2,
    Blue            = -1,
    Manual          = 0,
    DefaultGrass    = 1,
    DefaultStone    = 2,
    DefaultDark     = 3,
    DefaultLight    = 4,
    DefaultLeaves   = 5,
    DefaultCloud    = 6,
    Empty           = 7,
    Full            = 8,
    Alpha           = 9,
    Shadow          = 10,
} CCE_Palette;

void cce_setup_2d_projection(int width, int height);

CCE_Color cce_get_color(int pos_x, int pos_y, int offset_x, int offset_y, CCE_Palette palette, ...);
void cce_set_pixel(CCE_Layer* layer, int screen_x, int screen_y, CCE_Color color);
void cce_set_pixel_rect(CCE_Layer* layer, int x0, int y0, int x1, int y1, CCE_Color color);

// Layer creation
// `cce_layer_create` now creates a GPU render-target layer by default (baked drawing; minimal CPU per frame).
// Use `cce_layer_cpu_create` for the legacy chunk-based CPU layer.
CCE_Layer* cce_layer_create(int screen_w, int screen_h, char * name, CCE_LayerBackend backend);
CCE_Layer* cce_layer_cpu_create(int screen_w, int screen_h, char * name);
CCE_Layer* cce_layer_gpu_create(int screen_w, int screen_h, char * name);

// GPU layer recording helpers.
// You can call draw functions without begin/end (they will auto-wrap), but batching with begin/end is faster.
int cce_layer_begin(CCE_Layer* layer);
int cce_layer_end(CCE_Layer* layer);
int cce_layer_clear(CCE_Layer* layer, CCE_Color color);

// Per-layer shader (applied to the layer's texture).
int cce_layer_set_shader(CCE_Layer* layer, const CCE_Shader* shader, CCE_LayerShaderMode mode, float coefficient);
int cce_layer_set_shader_tint(CCE_Layer* layer, CCE_Color tint);

void render_layer(CCE_Layer* layer);
void cce_layer_destroy(CCE_Layer* layer);
void render_pie(CCE_Layer** layers, int count); // This is a rendering of several layers one after the other.

/*
    T E X T
*/

typedef struct TTF_Font TTF_Font;

TTF_Font* cce_font_load(const char* filename, float font_size);
void cce_font_free(TTF_Font* font);
// Controls texture filtering for GPU text rendering.
// smooth=0 => nearest (crisp/pixelated), smooth=1 => linear (smooth/blurred when upscaled).
void cce_font_set_smooth(TTF_Font* font, int smooth);
float cce_text_width(TTF_Font* font, const char* text, float scale);
float cce_text_ascent(TTF_Font* font, float scale);
float cce_text_height(TTF_Font* font, const char* text, float scale);
void cce_draw_text(CCE_Layer* layer, TTF_Font* font, const char* text, int x, int y, float scale, CCE_Color color);
void cce_draw_text_fmt(CCE_Layer* layer, TTF_Font* font, int x, int y, float scale, CCE_Color color, const char* format, ...);
int cce_draw_text_gpu(TTF_Font* font, const char* text, float x, float y, float scale, CCE_Color color);

/*
    T I M E R
*/

typedef struct {
    double target_fps;
    double frame_time;
    double last_time;
    double accumulator;
    int frame_count;
    double fps;
    double fps_timer;
    double fps_start_time;
} CCE_FPS_Timer;

CCE_FPS_Timer* cce_fps_timer_create(double target_tps);
int cce_fps_timer_should_update(CCE_FPS_Timer* timer);
double cce_fps_timer_get_fps(CCE_FPS_Timer* timer);
void cce_fps_timer_destroy(CCE_FPS_Timer* timer);

/*
    S P R I T E
*/

typedef struct
{
    int width;
    int height;
    int channels;      // Always 4 after load (RGBA)
    unsigned char* data;
    char path[256];
    // Optional GPU cache for fast drawing on GPU layers.
    // NOTE: may be lazily uploaded on first draw; kept in sync by cce_sprite_free().
    unsigned int texture_id;
} CCE_Sprite;

int cce_sprite_load(CCE_Sprite* out);
void cce_sprite_free(CCE_Sprite* img);
int cce_draw_sprite(CCE_Layer* layer, const CCE_Sprite* sprite, int dst_x, int dst_y, int batch_size, CCE_Color modifier, int frame_step_px, int current_step);
void cce_sprite_calc_frame_uv(const CCE_Texture* tex, int frame_width_px, int frame_index, float* u0, float* u1);

/*
    S H A D E R
*/

typedef enum
{
    CCE_SHADER_TINT = 0,
    CCE_SHADER_GRAYSCALE = 1,
    CCE_SHADER_GLOW = 2,
    CCE_SHADER_BLOOM = 3,
    CCE_SHADER_CUSTOM = 4,
} CCE_ShaderType;

struct CCE_Shader
{
    unsigned int program;
    unsigned int vertex_id;
    unsigned int fragment_id;
    CCE_ShaderType type;
    char name[64];
    char path[256];
    int uniform_texture;
    int uniform_coeff;
    int uniform_tint;
    int uniform_resolution;
    bool loaded;
};

int cce_shader_load_from_file(CCE_Shader* out, const char* path, CCE_ShaderType type, const char* name);
void cce_shader_unload(CCE_Shader* shader);
int cce_shader_apply(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, int arg_count, ...);
int cce_shader_apply_tint(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, CCE_Color tint);
int cce_shader_apply_grayscale(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_apply_glow(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_apply_bloom(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_apply_bloom_radius(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, float radius);

#endif