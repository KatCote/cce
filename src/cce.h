#ifndef CCE_GUARD_H
#define CCE_GUARD_H

#define CCE_VERSION "0.1.2"
#define CCE_VERNAME "Initial"
#define CCE_NAME    "CastleCore Engine"

/* GLFW*/
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

Window* cce_window_create(int width, int height, const char* title);
void cce_window_destroy(Window* window);
int cce_window_should_close(const Window* window);
void cce_window_poll_events(void);
void cce_window_swap_buffers(Window* window);
void cce_window_make_current(Window* window);

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

TTF_Font* ttf_font_load(const char* filename, float font_size);
void ttf_font_free(TTF_Font* font);
void ttf_render_text(TTF_Font* font, const char* text, float x, float y, CCE_Palette palette, ...);
float ttf_text_width(TTF_Font* font, const char* text);
void ttf_render_text_to_layer(CCE_Layer* layer, TTF_Font* font, const char* text, 
                               int x, int y, float scale, CCE_Color color);
void ttf_render_text_to_layer_fmt(CCE_Layer* layer, TTF_Font* font, 
                                   int x, int y, float scale, CCE_Color color,
                                   const char* format, ...);

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
void cce_fps_timer_update(CCE_FPS_Timer* timer);
int cce_fps_timer_should_update(CCE_FPS_Timer* timer);
double cce_fps_timer_get_delta(CCE_FPS_Timer* timer);
double cce_fps_timer_get_fps(CCE_FPS_Timer* timer);
void cce_fps_timer_destroy(CCE_FPS_Timer* timer);

#endif