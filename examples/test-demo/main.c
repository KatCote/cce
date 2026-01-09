#define _POSIX_C_SOURCE 200809L

#include "../../build/include/cce.h"
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    // Demo launch mode (code-level switch; no user prompts).
    const int demo_fullscreen = 1;   // 0 = windowed, 1 = fullscreen
    const int demo_monitor = 1;      // monitor index (0 = primary)
    const int demo_width = 2560;     // requested resolution (used for windowed; for fullscreen will be matched/closest)
    const int demo_height = 1440;
    const int demo_batch_size = 6;   // sprite "pixel batch" scale
    const int logo_duration = 180;

    printf("=== CCE UI Test Stub ===\n");

    if (cce_engine_init() != 0) {
        printf("Engine initialization failed\n");
        return -1;
    }

    set_engine_msaa(8);

    CCE_WindowConfig win_cfg = {
        .title = CCE_NAME " " CCE_VERSION " | UI Test",
        .width = demo_width,
        .height = demo_height,
        .monitor_index = demo_monitor,
        .batch_size = demo_batch_size,
        .mode = demo_fullscreen ? CCE_WINDOW_FULLSCREEN : CCE_WINDOW_WINDOWED,
    };

    Window* window = cce_window_create_ex(&win_cfg);
    if (!window) {
        printf("Window creation failed\n");
        cce_engine_cleanup();
        return -1;
    }

    // Cursor demo: replace cursor on window hover with a custom image, otherwise keep normal arrow.
    // (The project currently has only this cursor image.)
    if (cce_window_set_cursor(window, CCE_CURSOR_ARROW) != 0) {
        printf("Failed to set base cursor\n");
    }
    if (cce_window_set_cursor_image(window, "/home/katcote/cce/examples/assets/cursor/Cursor_Default.png", 0, 0) != 0) {
        printf("Failed to load custom cursor image\n");
    } else {
        // Enable "on hover" cursor switch to the custom cursor.
        cce_window_set_cursor_on_hover(window, 1, CCE_CURSOR_CUSTOM);
    }

    int width = 0;
    int height = 0;
    cce_window_get_size(window, &width, &height);
    const int batch_size = cce_window_get_batch_size(window);

    cce_setup_2d_projection(width, height);

    CCE_FPS_Timer* timer = cce_fps_timer_create(60.0);

    if (!timer) {
        printf("FPS timer creation failed\n");
        cce_window_destroy(window);
        cce_engine_cleanup();
        return -1;
    }

    // GPU assets (no CPU layers).
    CCE_Texture tex_logo = {0};
    CCE_Texture tex_button_glass = {0};
    CCE_Texture tex_button_fluid = {0};
    CCE_Texture tex_bg1 = {0};
    CCE_Texture tex_bg2 = {0};
    CCE_Texture tex_bg3 = {0};
    CCE_Texture tex_bg4 = {0};
    
    if (cce_texture_load(&tex_logo, "/home/katcote/cce/examples/assets/CCE.png") != 0) return -1;
    if (cce_texture_load(&tex_button_glass, "/home/katcote/cce/examples/assets/interface/Button2_Glass.png") != 0) return -1;
    if (cce_texture_load(&tex_button_fluid, "/home/katcote/cce/examples/assets/interface/Button2_Fluid.png") != 0) return -1;
    if (cce_texture_load(&tex_bg1, "/home/katcote/cce/examples/assets/DemoBG_L1.png") != 0) return -1;
    if (cce_texture_load(&tex_bg2, "/home/katcote/cce/examples/assets/DemoBG_L2.png") != 0) return -1;
    if (cce_texture_load(&tex_bg3, "/home/katcote/cce/examples/assets/DemoBG_L3.png") != 0) return -1;
    if (cce_texture_load(&tex_bg4, "/home/katcote/cce/examples/assets/DemoBG_L4.png") != 0) return -1;

    TTF_Font* font = cce_font_load("/home/katcote/cce/examples/fonts/Fixedsys.ttf", 6);
    // Fixedsys is a pixel font; keep it crisp when upscaled in GPU mode.
    cce_font_set_smooth(font, 0);

    // GPU layers: "draw once and keep".
    // We bake static parts into layers once, and update only the animated layer when needed.
    CCE_Layer* layer_logo = cce_layer_create(width, height, "Logo Layer", CCE_LAYER_GPU);
    CCE_Layer* layer_bg = cce_layer_create(width, height, "BG Layer", CCE_LAYER_GPU);
    CCE_Layer* layer_bg_sub = cce_layer_create(width, height, "BG Sub Layer", CCE_LAYER_GPU);
    CCE_Layer* layer_ui = cce_layer_create(width, height, "UI Layer", CCE_LAYER_GPU);
    CCE_Layer* layer_ui_sub = cce_layer_create(width, height, "UI Sub Layer", CCE_LAYER_GPU);

    // Precompute shared UI layout.
    const float logo_scale = (float)batch_size;
    const float logo_w = (float)tex_logo.width * logo_scale;
    const float logo_h = (float)tex_logo.height * logo_scale;

    const float btn_scale = batch_size * 0.5f;
    const float btn_w = (float)tex_button_glass.width * btn_scale;
    const float btn_h = (float)tex_button_glass.height * btn_scale;
    const float btn_x = (width * 0.5f) - (btn_w * 0.5f);
    const float btn_y = (height * 0.5f) - (btn_h * 0.5f);

    // Bake logo-only splash layer.
    cce_layer_begin(layer_logo);
    cce_layer_clear(layer_logo, cce_get_color(0, 0, 0, 0, Manual, 19, 19, 19, 255));
    cce_draw_texture_region(
        &tex_logo,
        (width * 0.5f) - (logo_w * 0.5f),
        (height * 0.5f) - (logo_h * 0.5f),
        logo_w,
        logo_h,
        0, 0, 1, 1,
        cce_get_color(0, 0, 0, 0, Full)
    );
    cce_layer_end(layer_logo);

    // Bake static background.
    cce_layer_begin(layer_bg);
    cce_layer_clear(layer_bg, cce_get_color(0, 0, 0, 0, Empty));
    cce_draw_texture_region(&tex_bg1, 0, 0, (float)width, (float)height, 0, 0, 1, 1, cce_get_color(0, 0, 0, 0, Full));
    cce_draw_texture_region(&tex_bg2, 0, 0, (float)width, (float)height, 0, 0, 1, 1, cce_get_color(0, 0, 0, 0, Full));
    cce_draw_texture_region(&tex_bg3, 0, 0, (float)width, (float)height, 0, 0, 1, 1, cce_get_color(0, 0, 0, 0, Full));
    cce_layer_end(layer_bg);

    // Bake static UI (logo, glass overlay, text).
    cce_layer_begin(layer_ui);
    cce_layer_clear(layer_ui, cce_get_color(0, 0, 0, 0, Empty));

    // Logo on top.
    cce_draw_texture_region(
        &tex_logo,
        (width * 0.5f) - (logo_w * 0.5f),
        (height * 0.75f) - (logo_h * 0.5f) + (batch_size * 5.0f),
        logo_w,
        logo_h,
        0, 0, 1, 1,
        cce_get_color(0, 0, 0, 0, Full)
    );

    // Glass overlay (button frame).
    cce_draw_texture_region(&tex_button_glass, btn_x, btn_y, btn_w, btn_h, 0, 0, 1, 1, cce_get_color(0, 0, 0, 0, Alpha, 200));

    // Text (GPU).
    const char text_1[] = "Start";
    const float text_scale_1 = 3.0f * batch_size;
    const float text_w = cce_text_width(font, text_1, text_scale_1);
    const float text_h = cce_text_height(font, text_1, text_scale_1);

    cce_draw_text_gpu(
        font,
        text_1,
        (width * 0.5f) - (text_w * 0.5f),
        (height * 0.5f) + (text_h * 0.35f)  - batch_size * 0.5f,
        text_scale_1,
        cce_get_color(0, 0, 0, 0, Shadow, 200)
    );

    cce_draw_text_gpu(
        font,
        text_1,
        (width * 0.5f) - (text_w * 0.5f),
        (height * 0.5f) + (text_h * 0.35f) - batch_size,
        text_scale_1,
        cce_get_color(0, 0, 0, 0, Full)
    );

    const char text_2[] = CCE_VERSION;
    const float text_scale_2 = 1.0f * (float)batch_size;
    cce_draw_text_gpu(
        font,
        text_2,
        0 + batch_size,
        height - batch_size,
        text_scale_2,
        cce_get_color(0, 0, 0, 0, Alpha, 64)
    );
    cce_layer_end(layer_ui);

    int frame = 0;
    while (!cce_window_should_close(window))
    {
        if (cce_fps_timer_should_update(timer))
        {
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            static int button_fluid_step_anim = 1;
            if (frame % 6 == 0) {
                button_fluid_step_anim = (button_fluid_step_anim == 9) ? 0 : button_fluid_step_anim + 1;
            }

            if (frame <= logo_duration)
            {
                CCE_Layer* layers[] = {layer_logo};
                render_pie(layers, 1);
            }
            else
            {
                float u0 = 0.0f, u1 = 1.0f;

                cce_sprite_calc_frame_uv(&tex_bg4, tex_bg4.width / 2, 1, &u0, &u1);
                cce_layer_begin(layer_bg_sub);
                cce_layer_clear(layer_bg_sub, cce_get_color(0, 0, 0, 0, Empty));
                cce_draw_texture_region(&tex_bg4, 0, 0, width, height, u0, 0.0f, u1, 1.0f, cce_get_color(0, 0, 0, 0, Full));
                cce_layer_end(layer_bg_sub);

                cce_sprite_calc_frame_uv(&tex_button_fluid, tex_button_glass.width, button_fluid_step_anim, &u0, &u1);
                cce_layer_begin(layer_ui_sub);
                cce_layer_clear(layer_ui_sub, cce_get_color(0, 0, 0, 0, Empty));
                cce_draw_texture_region(
                    &tex_button_fluid,
                    btn_x,
                    btn_y,
                    btn_w,
                    btn_h,
                    u0, 0.0f, u1, 1.0f,
                    cce_get_color(0, 0, 0, 0, Manual, 236, 255, 0, 255)
                );
                cce_layer_end(layer_ui_sub);

                CCE_Layer* layers[] = {layer_bg, layer_bg_sub, layer_ui_sub, layer_ui};
                render_pie(layers, 4);
            }

            cce_window_swap_buffers(window);

            frame++;
            if (frame % 60 == 0)
            { printf("Frame: %d, FPS: %.1f\n", frame, cce_fps_timer_get_fps(timer)); }
        }

        cce_window_poll_events();
        struct timespec ts = {0};
        ts.tv_nsec = 100000; // 100 µs
        nanosleep(&ts, NULL);
    }

    cce_fps_timer_destroy(timer);

    cce_layer_destroy(layer_ui);
    cce_layer_destroy(layer_ui_sub);
    cce_layer_destroy(layer_bg);
    cce_layer_destroy(layer_bg_sub);
    cce_layer_destroy(layer_logo);

    cce_font_free(font);

    cce_texture_free(&tex_logo);
    cce_texture_free(&tex_button_glass);
    cce_texture_free(&tex_button_fluid);
    cce_texture_free(&tex_bg1);
    cce_texture_free(&tex_bg2);
    cce_texture_free(&tex_bg3);

    cce_window_destroy(window);
    cce_engine_cleanup();

    return 0;
}
