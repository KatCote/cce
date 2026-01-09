#define _POSIX_C_SOURCE 200809L

#include "../../build/include/cce.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static void draw_grid_to_layer(CCE_Layer* layer, int x0, int y0, int x1, int y1, int pixel_size, int offset_x, int offset_y, CCE_Palette palette)
{
    if (!layer) return;
    if ((x0 > x1) || (y0 > y1)) return;
    if (pixel_size < 1) return;
    
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= layer->scr_w) x1 = layer->scr_w - 1;
    if (y1 >= layer->scr_h) y1 = layer->scr_h - 1;
    
    int cols = (x1 - x0 + pixel_size - 1) / pixel_size;
    int rows = (y1 - y0 + pixel_size - 1) / pixel_size;
    
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int pixel_x = x0 + col * pixel_size;
            int pixel_y = y0 + row * pixel_size;
            
            int px1 = pixel_x + pixel_size - 1;
            int py1 = pixel_y + pixel_size - 1;
            if (px1 >= layer->scr_w) px1 = layer->scr_w - 1;
            if (py1 >= layer->scr_h) py1 = layer->scr_h - 1;
            
            CCE_Color color = cce_get_color(pixel_x, pixel_y, offset_x, offset_y, palette);
            
            cce_set_pixel_rect(layer, pixel_x, pixel_y, px1, py1, color);
        }
    }
}

int main() {
    int width = 1920;
    int height = 1080;
    printf("=== CCE Glow Shader Test (Layers) ===\n");

    CCE_Sprite sprite_fireplace = {0};
    CCE_Sprite sprite_fireplace2 = {0};

    strcpy(sprite_fireplace.path, "/home/katcote/cce/examples/assets/StreetFireplace_Base.png");
    strcpy(sprite_fireplace2.path, "/home/katcote/cce/examples/assets/StreetFireplace_Fire.png");

    if (cce_sprite_load(&sprite_fireplace) != 0) {
        printf("Failed to load sprite: %s\n", sprite_fireplace.path);
        return -1;
    }
    if (cce_sprite_load(&sprite_fireplace2) != 0) {
        printf("Failed to load sprite: %s\n", sprite_fireplace2.path);
        cce_sprite_free(&sprite_fireplace);
        return -1;
    }

    set_engine_seed(1337);
    
    if (cce_engine_init() != 0) {
        printf("Engine init failed\n");
        cce_sprite_free(&sprite_fireplace);
        cce_sprite_free(&sprite_fireplace2);
        return -1;
    }

    set_engine_msaa(8);
    
    Window* window = cce_window_create(width, height, CCE_NAME " " CCE_VERSION " | Glow Shader");
    if (!window) {
        printf("Window creation failed\n");
        cce_engine_cleanup();
        return -1;
    }
    
    cce_setup_2d_projection(width, height);
    
    CCE_FPS_Timer* timer = cce_fps_timer_create(60.0);
    
    TTF_Font* font = cce_font_load("/home/katcote/cce/examples/fonts/Fixedsys.ttf", 72);

    int batch_size = 8;
    
    CCE_Layer* grid_layer1 = cce_layer_cpu_create(width, height, "Grid Layer 1");
    CCE_Layer* grid_layer2 = cce_layer_cpu_create(width, height, "Grid Layer 2");
    CCE_Layer* sprite_layer = cce_layer_cpu_create(width, height, "Sprite Layer");
    CCE_Layer* light_layer = cce_layer_cpu_create(width, height, "Light Layer");
    CCE_Layer* text_layer = cce_layer_cpu_create(width, height, "Text Layer");

    // Load shader for glow pass on the light layer.
    CCE_Shader glow_shader = {0};

    cce_shader_load_from_file(&glow_shader, "./shaders/glow.frag", CCE_SHADER_GLOW, "glow");

    CCE_Shader bloom_shader = {0};

    cce_shader_load_from_file(&bloom_shader, "./shaders/bloom.frag", CCE_SHADER_BLOOM, "bloom");

    int frame = 0;
    float fps = 0.0f;

    // Static scene setup.
    cce_draw_sprite(
        sprite_layer,
        &sprite_fireplace,
        width/4 - sprite_fireplace.height/2 * batch_size,
        height/3 - sprite_fireplace.height/2 * batch_size,
        batch_size,
        cce_get_color(0, 0, 0, 0, Manual, 255, 255, 255, 255),
        0,
        0
    );
    cce_draw_sprite(
        sprite_layer,
        &sprite_fireplace,
        (width/4)*3 - sprite_fireplace.height/2 * batch_size,
        height/3 - sprite_fireplace.height/2 * batch_size,
        batch_size,
        cce_get_color(0, 0, 0, 0, Manual, 255, 255, 255, 255),
        0,
        0
    );

    cce_draw_sprite(
        sprite_layer,
        &sprite_fireplace,
        width/4 - sprite_fireplace.height/2 * batch_size,
        (height/3)*2 - sprite_fireplace.height/2 * batch_size,
        batch_size,
        cce_get_color(0, 0, 0, 0, Manual, 255, 255, 255, 255),
        0,
        0
    );
    cce_draw_sprite(
        sprite_layer,
        &sprite_fireplace,
        (width/4)*3 - sprite_fireplace.height/2 * batch_size,
        (height/3)*2 - sprite_fireplace.height/2 * batch_size,
        batch_size,
        cce_get_color(0, 0, 0, 0, Manual, 255, 255, 255, 255),
        0,
        0
    );
    
    draw_grid_to_layer(grid_layer1, 0, 0, width/2, height, 10, 0, 0, DefaultStone);
    draw_grid_to_layer(grid_layer2, width/2, 0, width, height, 5, 5, 5, DefaultGrass);

    while (cce_window_should_close(window) == 0 && frame < 60000)
    {
        if (cce_fps_timer_should_update(timer))
        {
            fps = timer->fps;
            
            CCE_Color empty = cce_get_color(0, 0, 0, 0, Empty);
            cce_set_pixel_rect(text_layer, 0, 0, width - 1, height - 1, empty);
            cce_set_pixel_rect(light_layer, 0, 0, width - 1, height - 1, empty);

            static int frame_step_anim = 1;
            if (frame % 6 == 0) {
                frame_step_anim = (frame_step_anim == 3) ? 0 : frame_step_anim + 1;
            }

            cce_draw_sprite(
                light_layer,
                &sprite_fireplace2,
                (width/4)*3 - sprite_fireplace2.height/2 * batch_size,
                height/3 - sprite_fireplace2.height/2 * batch_size,
                batch_size,
                cce_get_color(0, 0, 0, 0, Manual, 255, 255, 255, 255),
                32,
                frame_step_anim
            );

            cce_draw_sprite(
                light_layer,
                &sprite_fireplace2,
                width/4 - sprite_fireplace2.height/2 * batch_size,
                (height/3)*2 - sprite_fireplace2.height/2 * batch_size,
                batch_size,
                cce_get_color(0, 0, 0, 0, Manual, 255, 255, 255, 255),
                32,
                frame_step_anim
            );
            
            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", fps);
            CCE_Color text_color = cce_get_color(0, 0, 0, 0, DefaultLight);
            cce_draw_text(text_layer, font, fps_text, 50, 80, 1.0f, text_color);
            
            // Draw base layers first (without light).
            CCE_Layer* base_layers[] = {grid_layer1, grid_layer2, sprite_layer, light_layer, text_layer};
            render_pie(base_layers, 5);

            // Apply glow shader to the animated fire layer.
            cce_shader_apply_glow(&glow_shader, light_layer, 1.0f);
            cce_shader_apply_bloom_radius(&bloom_shader, light_layer, 1.0f, 8.0f);

            // Draw HUD/text on top.
            // CCE_Layer* hud_layers[] = {light_layer, text_layer};
            // render_pie(hud_layers, 2);

            cce_window_swap_buffers(window);
            
            frame++;
            if (frame % 60 == 0) {
                printf("Frame: %d, FPS: %.1f\n", frame, fps);
            }
        }
        
        cce_window_poll_events();
        struct timespec ts = {0};
        ts.tv_nsec = 100000; // 100 µs
        nanosleep(&ts, NULL);
    }
    
    printf("Test completed\n");
    
    cce_layer_destroy(grid_layer1);
    cce_layer_destroy(grid_layer2);
    cce_layer_destroy(sprite_layer);
    cce_layer_destroy(light_layer);
    cce_layer_destroy(text_layer);
    cce_sprite_free(&sprite_fireplace);
    cce_sprite_free(&sprite_fireplace2);
    cce_shader_unload(&glow_shader);
    
    cce_font_free(font);
    cce_fps_timer_destroy(timer);
    cce_window_destroy(window);
    cce_engine_cleanup();
    
    return 0;
}
