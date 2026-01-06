#ifndef CCE_GUARD_H
#define CCE_GUARD_H

#define CCE_VERSION "0.1.0"
#define CCE_VERNAME "Initial"
#define CCE_NAME    "CastleCore Engine"

/* GLFW*/
typedef struct Window Window;

/* CCE */

// pct - Pixel Color Type
#define pct  unsigned char
#define bool unsigned char

typedef struct
{
    pct r;
    pct g;
    pct b;
    pct a;
} cce_color;

typedef struct
{
    int x, y;
    int w, h;
    cce_color* data;
    bool dirty;
    bool visible;
} Chunk;

struct Layer
{
    int scr_w, scr_h;
    int chunk_size;
    int chunk_count_x, chunk_count_y;
    Chunk*** chunks;
    unsigned int texture;
    char* name;
    int layer_id;
    bool enabled;
};

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

typedef enum Palette
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
} Palette;

typedef struct TTF_Font TTF_Font;
typedef struct Layer Layer;

/*
    I N I T
*/

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

void cce_setup_2d_projection(int width, int height);

void cce_draw_grid(int x0, int y0, int x1, int y1, int pixel_size, int offset_x, int offset_y, Palette palette);    // Deprecated
void cce_draw_cloud(int center_x, int center_y, int offset_x, int offset_y, float size, int seed);                  // Deprecated

cce_color cce_get_color(int pos_x, int pos_y, int offset_x, int offset_y, Palette palette, ...);
void set_pixel(Layer* layer, int screen_x, int screen_y, cce_color color);

Layer* create_layer(int screen_w, int screen_h, char * name);
void render_layer(Layer* layer);
void destroy_layer(Layer* layer);
void render_pie(Layer** layers, int count); // This is a rendering of several layers one after the other.

/*
    T E X T
*/

TTF_Font* ttf_font_load(const char* filename, float font_size);
void ttf_font_free(TTF_Font* font);
void ttf_render_text(TTF_Font* font, const char* text, float x, float y, Palette palette, ...);
float ttf_text_width(TTF_Font* font, const char* text);

#endif