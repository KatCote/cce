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

#define CCE_VERSION "0.2.1"
#define CCE_VERNAME "Initial"
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

typedef struct
{
    int x, y;
    int w, h;
    CCE_Color* data;
    bool dirty;
    bool visible;
} CCE_Chunk;

struct CCE_Layer
{
    int scr_w, scr_h;
    int chunk_size;
    int chunk_count_x, chunk_count_y;
    CCE_Chunk*** chunks;
    unsigned int texture;
    char* name;
    int layer_id;
    bool enabled;
    unsigned int pbo_ids[2];
    int current_pbo_index;
    int pbo_size;
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
} CCE_Palette;

typedef struct CCE_Layer CCE_Layer;

void cce_setup_2d_projection(int width, int height);

CCE_Color cce_get_color(int pos_x, int pos_y, int offset_x, int offset_y, CCE_Palette palette, ...);
void set_pixel(CCE_Layer* layer, int screen_x, int screen_y, CCE_Color color);
void set_pixel_rect(CCE_Layer* layer, int x0, int y0, int x1, int y1, CCE_Color color);

CCE_Layer* create_layer(int screen_w, int screen_h, char * name);
void render_layer(CCE_Layer* layer);
void destroy_layer(CCE_Layer* layer);
void render_pie(CCE_Layer** layers, int count); // This is a rendering of several layers one after the other.

/*
    T E X T
*/

typedef struct TTF_Font TTF_Font;

TTF_Font* cce_font_load(const char* filename, float font_size);
void cce_font_free(TTF_Font* font);
float cce_text_width(TTF_Font* font, const char* text, float scale);
float cce_text_ascent(TTF_Font* font, float scale);
float cce_text_height(TTF_Font* font, const char* text, float scale);
void cce_draw_text(CCE_Layer* layer, TTF_Font* font, const char* text, int x, int y, float scale, CCE_Color color);
void cce_draw_text_fmt(CCE_Layer* layer, TTF_Font* font, int x, int y, float scale, CCE_Color color, const char* format, ...);

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
} CCE_Sprite;

int cce_sprite_load(CCE_Sprite* out);
void cce_sprite_free(CCE_Sprite* img);
int cce_draw_sprite(CCE_Layer* layer, const CCE_Sprite* sprite, int dst_x, int dst_y, int batch_size, CCE_Color modifier, int frame_step_px, int current_step);

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

typedef struct
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
} CCE_Shader;

int cce_shader_load_from_file(CCE_Shader* out, const char* path, CCE_ShaderType type, const char* name);
void cce_shader_unload(CCE_Shader* shader);
int cce_shader_apply(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, int arg_count, ...);
int cce_shader_apply_tint(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, CCE_Color tint);
int cce_shader_apply_grayscale(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_apply_glow(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_apply_bloom(const CCE_Shader* shader, CCE_Layer* layer, float coefficient);
int cce_shader_apply_bloom_radius(const CCE_Shader* shader, CCE_Layer* layer, float coefficient, float radius);

#endif