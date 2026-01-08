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
    const int demo_monitor = 0;      // monitor index (0 = primary)
    const int demo_width = 1920;     // requested resolution (used for windowed; for fullscreen will be matched/closest)
    const int demo_height = 1080;
    const int demo_batch_size = 4;   // sprite "pixel batch" scale
    const int logo_duration = 0;

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

    CCE_Layer* bg_l1 = create_layer(width, height, "BG L1");
    CCE_Layer* bg_l2 = create_layer(width, height, "BG L2");
    CCE_Layer* bg_l3 = create_layer(width, height, "BG L3");
    CCE_Layer* bg_l4 = create_layer(width, height, "BG L4");

    CCE_Layer* ui_l1 = create_layer(width, height, "UI L1");
    CCE_Layer* ui_l2 = create_layer(width, height, "UI L2");
    CCE_Layer* ui_l3 = create_layer(width, height, "UI L3");
    CCE_Layer* ui_l4 = create_layer(width, height, "UI L4");

    CCE_Sprite logo = {0};

    CCE_Sprite button_glass = {0};
    CCE_Sprite button_fluid = {0};

    CCE_Sprite bg_level_1 = {0};
    CCE_Sprite bg_level_2 = {0};
    CCE_Sprite bg_level_3 = {0};

    strcpy(logo.path, "/home/katcote/cce/examples/assets/CCE.png");
    if (cce_sprite_load(&logo) != 0) {
        printf("Failed to load sprite: %s\n", logo.path);
        return -1;
    }

    strcpy(button_glass.path, "/home/katcote/cce/examples/assets/interface/Button2_Glass.png");
    if (cce_sprite_load(&button_glass) != 0) {
        printf("Failed to load sprite: %s\n", button_glass.path);
        return -1;
    }

    strcpy(button_fluid.path, "/home/katcote/cce/examples/assets/interface/Button2_Fluid.png");
    if (cce_sprite_load(&button_fluid) != 0) {
        printf("Failed to load sprite: %s\n", button_fluid.path);
        return -1;
    }

    strcpy(bg_level_1.path, "/home/katcote/cce/examples/assets/DemoBG_L1.png");
    if (cce_sprite_load(&bg_level_1) != 0) {
        printf("Failed to load sprite: %s\n", bg_level_1.path);
        return -1;
    }

    strcpy(bg_level_2.path, "/home/katcote/cce/examples/assets/DemoBG_L2.png");
    if (cce_sprite_load(&bg_level_2) != 0) {
        printf("Failed to load sprite: %s\n", bg_level_2.path);
        return -1;
    }

    strcpy(bg_level_3.path, "/home/katcote/cce/examples/assets/DemoBG_L3.png");
    if (cce_sprite_load(&bg_level_3) != 0) {
        printf("Failed to load sprite: %s\n", bg_level_3.path);
        return -1;
    }
    TTF_Font* font = cce_font_load("/home/katcote/cce/examples/fonts/Fixedsys.ttf", 6);

    // Pre-render static scene (same idea as test-shader): draw static content once,
    // then only update animated elements in the loop.
    {
        // Background is static.
        cce_draw_sprite(bg_l1, &bg_level_1, 0, 0, batch_size, cce_get_color(0, 0, 0, 0, Full), 0, 0);
        cce_draw_sprite(bg_l2, &bg_level_2, 0, 0, batch_size, cce_get_color(0, 0, 0, 0, Full), 0, 0);
        cce_draw_sprite(bg_l3, &bg_level_3, 0, 0, batch_size, cce_get_color(0, 0, 0, 0, Full), 0, 0);

        // Glass button is static; animated fluid will be drawn on top each frame.
        const int btn_scale = (batch_size > 1) ? (batch_size / 2) : 1;
        cce_draw_sprite(
            ui_l2,
            &button_glass,
            (width/2) - ((button_glass.width/2) * btn_scale),
            (height/2) - ((button_glass.height/2) * btn_scale),
            btn_scale,
            cce_get_color(0, 0, 0, 0, Full),
            0,
            0
        );

        // Static texts.
        int text_scale_1 = 3.0f * batch_size;
        char text_1[] = "Start";
        cce_draw_text(
            ui_l3,
            font,
            text_1,
            (width/2.0f) - (cce_text_width(font, text_1, text_scale_1)/2.0f),
            (height/2.0f) - (cce_text_height(font, text_1, text_scale_1)/2.0f) + cce_text_ascent(font, text_scale_1) + (btn_scale),
            text_scale_1,
            cce_get_color(0, 0, 0, 0, Manual, 0, 0, 0, 128)
        );
        cce_draw_text(
            ui_l4,
            font,
            text_1,
            width/2.0f - cce_text_width(font, text_1, text_scale_1)/2.0f,
            height/2.0f - cce_text_height(font, text_1, text_scale_1)/2.0f + cce_text_ascent(font, text_scale_1) - (btn_scale),
            text_scale_1,
            cce_get_color(0, 0, 0, 0, Full)
        );

        int text_scale_2 = 1.0f * batch_size;
        char text_2[] = "Version: " CCE_VERSION;
        cce_draw_text(
            ui_l4,
            font,
            text_2,
            batch_size,
            height - batch_size,
            text_scale_2,
            cce_get_color(0, 0, 0, 0, Alpha, 64)
        );
    }

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
                cce_draw_sprite(
                    ui_l1,
                    &logo,
                    (width/2) - ((logo.width/2) * batch_size),
                    (height/2) - ((logo.height/2) * batch_size),
                    batch_size,
                    frame < logo_duration ?
                    cce_get_color(0, 0, 0, 0, Full) :
                    cce_get_color(0, 0, 0, 0, Empty),
                    0,
                    0   
                );
            }
            else
            {
                cce_draw_sprite(
                    ui_l1,
                    &logo,
                    (width/2.0f) - ((logo.width/2) * batch_size),
                    (height/4.0f)*3 - ((logo.height/2) * batch_size) + (batch_size * 5.0f),
                    batch_size,
                    cce_get_color(0, 0, 0, 0, Full),
                    0,
                    0
                );

                const int btn_scale = (batch_size > 1) ? (batch_size / 2) : 1;
                const int fluid_dst_x = (width/2) - ((button_glass.width/2) * btn_scale);
                const int fluid_dst_y = (height/2) - ((button_glass.height/2) * btn_scale);

                // Clear only the region where the animated sprite was drawn last frame.
                // cce_draw_sprite uses a bottom-left dst_y; it converts internally to top-left.
                const int fluid_w = button_fluid.width * btn_scale;
                const int fluid_h = button_fluid.height * btn_scale;
                const int clear_x0 = fluid_dst_x;
                const int clear_x1 = fluid_dst_x + fluid_w - 1;
                const int clear_y0 = height - (fluid_dst_y + fluid_h);
                const int clear_y1 = height - 1 - fluid_dst_y;
                set_pixel_rect(ui_l1, clear_x0, clear_y0, clear_x1, clear_y1, cce_get_color(0, 0, 0, 0, Empty));

                // Animated overlay.
                cce_draw_sprite(
                    ui_l1,
                    &button_fluid,
                    fluid_dst_x,
                    fluid_dst_y,
                    btn_scale,
                    cce_get_color(0, 0, 0, 0, Manual, 236, 255, 0, 190),
                    160,
                    button_fluid_step_anim
                );
            }

            CCE_Layer* layers[] = {bg_l1, bg_l2, bg_l3, bg_l4, ui_l1, ui_l2, ui_l3, ui_l4};
            render_pie(layers, 8);

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

    destroy_layer(bg_l1);
    destroy_layer(bg_l2);
    destroy_layer(bg_l3);
    destroy_layer(bg_l4);
    destroy_layer(ui_l1);
    destroy_layer(ui_l2);
    destroy_layer(ui_l3);
    destroy_layer(ui_l4);

    cce_fps_timer_destroy(timer);

    cce_font_free(font);
    cce_sprite_free(&logo);
    cce_sprite_free(&button_glass);
    cce_sprite_free(&button_fluid);
    cce_sprite_free(&bg_level_1);
    cce_sprite_free(&bg_level_2);
    cce_sprite_free(&bg_level_3);

    cce_window_destroy(window);
    cce_engine_cleanup();

    return 0;
}
