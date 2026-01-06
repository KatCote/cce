#include "../../build/include/cce.h"
#include <stdio.h>
#include <GL/gl.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int width = 1920;
    int height = 1080;
    
    printf("=== CCE Chunks Test ===\n");

    set_engine_seed(1337);
    
    if (cce_engine_init() != 0) {
        printf("Engine init failed\n");
        return -1;
    }

    set_engine_msaa(4);
    
    Window* window = cce_window_create(width, height, 
        CCE_NAME " " CCE_VERSION " | " "Chunks");
    
    if (!window) {
        printf("Window creation failed\n");
        cce_engine_cleanup();
        return -1;
    }
    
    cce_setup_2d_projection(width, height);

    Layer* layer = create_layer(width, height, "TCM1");
    
    cce_color* background_buffer = malloc(width * height * sizeof(cce_color));
    
    int chunk_count_x = (width + get_engine_chunk_size() - 1) / get_engine_chunk_size();
    int chunk_count_y = (height + get_engine_chunk_size() - 1) / get_engine_chunk_size();
    
    for (int chunk_y = 0; chunk_y < chunk_count_y; chunk_y++) {
        for (int chunk_x = 0; chunk_x < chunk_count_x; chunk_x++) {
            int start_x = chunk_x * get_engine_chunk_size();
            int start_y = chunk_y * get_engine_chunk_size();
            int end_x = start_x + get_engine_chunk_size();
            int end_y = start_y + get_engine_chunk_size();
            
            if (end_x > width) end_x = width;
            if (end_y > height) end_y = height;
            
            uint8_t r = (chunk_x * 60) % 256;
            uint8_t g = (chunk_y * 60) % 256;
            uint8_t b = ((chunk_x + chunk_y) * 30) % 256;
            
            for (int y = start_y; y < end_y; y++) {
                for (int x = start_x; x < end_x; x++) {
                    cce_color color = cce_get_color(0, 0, 0, 0, Manual, r, g, b, 255);
                    background_buffer[y * width + x] = color;
                    set_pixel(layer, x, y, color);
                }
            }
        }
    }
    
    float square_x = width / 2.0f;
    float square_y = height / 2.0f;
    float square_size = 100.0f;
    float square_speed_x = 5.0f;
    float square_speed_y = 3.0f;
    
    float circle_x = width / 3.0f;
    float circle_y = height / 3.0f;
    float circle_radius = 80.0f;
    float circle_speed_x = -4.0f;
    float circle_speed_y = 4.0f;
    
    int prev_square_left = 0, prev_square_right = 0, prev_square_top = 0, prev_square_bottom = 0;
    int prev_circle_left = 0, prev_circle_right = 0, prev_circle_top = 0, prev_circle_bottom = 0;
    
    prev_square_left = (int)(square_x - square_size / 2);
    prev_square_right = (int)(square_x + square_size / 2);
    prev_square_top = (int)(square_y - square_size / 2);
    prev_square_bottom = (int)(square_y + square_size / 2);
    
    prev_circle_left = (int)(circle_x - circle_radius);
    prev_circle_right = (int)(circle_x + circle_radius);
    prev_circle_top = (int)(circle_y - circle_radius);
    prev_circle_bottom = (int)(circle_y + circle_radius);
    
    int frame = 0;
    
    while (cce_window_should_close(window) == 0 && frame < get_engine_chunk_size())
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        for (int y = prev_square_top; y < prev_square_bottom; y++) {
            for (int x = prev_square_left; x < prev_square_right; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    set_pixel(layer, x, y, background_buffer[y * width + x]);
                }
            }
        }
        
        int prev_circle_r2 = (int)(circle_radius * circle_radius);
        for (int y = prev_circle_top; y < prev_circle_bottom; y++) {
            for (int x = prev_circle_left; x < prev_circle_right; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    int dx = x - (int)(circle_x - circle_speed_x);
                    int dy = y - (int)(circle_y - circle_speed_y);
                    
                    if (dx*dx + dy*dy <= prev_circle_r2) {
                        set_pixel(layer, x, y, background_buffer[y * width + x]);
                    }
                }
            }
        }

        square_x += square_speed_x;
        square_y += square_speed_y;
        
        if (square_x < square_size / 2 || square_x > width - square_size / 2) {
            square_speed_x = -square_speed_x;
            square_x += square_speed_x;
        }
        if (square_y < square_size / 2 || square_y > height - square_size / 2) {
            square_speed_y = -square_speed_y;
            square_y += square_speed_y;
        }
        
        circle_x += circle_speed_x;
        circle_y += circle_speed_y;
        
        if (circle_x < circle_radius || circle_x > width - circle_radius) {
            circle_speed_x = -circle_speed_x;
            circle_x += circle_speed_x;
        }
        if (circle_y < circle_radius || circle_y > height - circle_radius) {
            circle_speed_y = -circle_speed_y;
            circle_y += circle_speed_y;
        }
        
        prev_square_left = (int)(square_x - square_size / 2);
        prev_square_right = (int)(square_x + square_size / 2);
        prev_square_top = (int)(square_y - square_size / 2);
        prev_square_bottom = (int)(square_y + square_size / 2);
        
        prev_circle_left = (int)(circle_x - circle_radius);
        prev_circle_right = (int)(circle_x + circle_radius);
        prev_circle_top = (int)(circle_y - circle_radius);
        prev_circle_bottom = (int)(circle_y + circle_radius);
        
        for (int y = prev_square_top; y < prev_square_bottom; y++) {
            for (int x = prev_square_left; x < prev_square_right; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    set_pixel(layer, x, y, cce_get_color(0, 0, 0, 0, Red));
                }
            }
        }
        
        int circle_r2 = (int)(circle_radius * circle_radius);
        for (int y = prev_circle_top; y < prev_circle_bottom; y++) {
            for (int x = prev_circle_left; x < prev_circle_right; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    int dx = x - (int)circle_x;
                    int dy = y - (int)circle_y;
                    
                    if (dx*dx + dy*dy <= circle_r2) {
                        set_pixel(layer, x, y, cce_get_color(0, 0, 0, 0, Green));
                    }
                }
            }
        }
        
        set_pixel(layer, width/2, height/2, cce_get_color(0, 0, 0, 0, Manual, 255, 255, 255, 255));
        
        render_layer(layer);
        
        cce_window_swap_buffers(window);
        cce_window_poll_events();
        
        frame++;
        if (frame % 60 == 0) {
            printf("Frame: %d | Square: (%.0f, %.0f) | Circle: (%.0f, %.0f)\n", 
                   frame, square_x, square_y, circle_x, circle_y);
        }
        usleep(16666);
    }
    
    printf("Тест завершен.\n");
    
    free(background_buffer);

    destroy_layer(layer);
    
    cce_window_destroy(window);
    cce_engine_cleanup();
    
    return 0;
}