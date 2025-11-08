#ifndef CCE_GUARD_H
#define CCE_GUARD_H

#define CCE_VERSION "0.1.0"
#define CCE_VERNAME "Initial"
#define CCE_NAME    "CastleCore Engine"

/* GLFW*/
typedef struct Window Window;

/* CCE */
typedef struct Color Color;
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
} Palette;
typedef struct TTF_Font TTF_Font;

/*
    I N I T
*/

int cce_engine_init(void);
void cce_engine_cleanup(void);
int cce_get_version(char ** ver_str_ptr);

void set_engine_seed(long new_engine_seed);
long get_engine_seed(void);

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
void cce_draw_grid(int x0, int y0, int x1, int y1, int pixel_size, int offset_x, int offset_y, Palette palette);
Color cce_get_color(int pos_x, int pos_y, int offset_x, int offset_y, Palette palette, ...);

/*
    T E X T
*/

TTF_Font* ttf_font_load(const char* filename, float font_size);
void ttf_font_free(TTF_Font* font);
void ttf_render_text(TTF_Font* font, const char* text, float x, float y, Palette palette, ...);
float ttf_text_width(TTF_Font* font, const char* text);

#endif