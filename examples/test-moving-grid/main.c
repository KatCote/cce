#include "../../build/include/cce.h"
#include <stdio.h>
#include <GL/gl.h>
#include <unistd.h>

typedef struct {
    float x, y, radius;
} CloudBlob;

void draw_grid_to_layer(CCE_Layer* layer, int x0, int y0, int x1, int y1, int pixel_size, int offset_x, int offset_y, CCE_Palette palette)
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
            
            set_pixel_rect(layer, pixel_x, pixel_y, px1, py1, color);
        }
    }
}

void draw_cloud_to_layer(CCE_Layer* layer, int center_x, int center_y, int offset_x, int offset_y, float size, int seed)
{
    if (!layer) return;
    
    int num_blobs = 8 + 1 % 6;
    CloudBlob blobs[num_blobs];
    
    for (int i = 0; i < num_blobs; i++) {
        blobs[i].x = center_x + (seed*i % (int)(size * 1.5f) - size * 0.75f);
        blobs[i].y = center_y + (seed*i % (int)(size) - size * 0.5f);
        blobs[i].radius = size * (0.3f + (seed*i % 70) / 100.0f);
    }
    
    int pixel_batch_size = 25;
    
    for (int i = 0; i < num_blobs; i++) {
        CCE_Color color = cce_get_color(blobs[i].x, blobs[i].y, 0, 0, DefaultCloud);
        
        int r = (int)blobs[i].radius;
        int cx = (int)(blobs[i].x + offset_x);
        int cy = (int)(blobs[i].y + offset_y);
        int r2 = r * r;
        
        for (int y = -r; y <= r; y += pixel_batch_size) {
            for (int x = -r; x <= r; x += pixel_batch_size) {
                int batch_x0 = cx + x;
                int batch_y0 = cy + y;
                int batch_x1 = batch_x0 + pixel_batch_size - 1;
                int batch_y1 = batch_y0 + pixel_batch_size - 1;
                
                int corners_in_circle = 0;
                int corners[4][2] = {
                    {x, y},
                    {x + pixel_batch_size - 1, y},
                    {x + pixel_batch_size - 1, y + pixel_batch_size - 1},
                    {x, y + pixel_batch_size - 1}
                };
                
                for (int c = 0; c < 4; c++) {
                    int dx = corners[c][0];
                    int dy = corners[c][1];
                    if (dx*dx + dy*dy <= r2) {
                        corners_in_circle++;
                    }
                }
                
                if (corners_in_circle > 0) {
                    if (batch_x0 < 0) batch_x0 = 0;
                    if (batch_y0 < 0) batch_y0 = 0;
                    if (batch_x1 >= layer->scr_w) batch_x1 = layer->scr_w - 1;
                    if (batch_y1 >= layer->scr_h) batch_y1 = layer->scr_h - 1;
                    
                    // Рисуем только если батч не пустой
                    if (batch_x0 <= batch_x1 && batch_y0 <= batch_y1) {
                        set_pixel_rect(layer, batch_x0, batch_y0, batch_x1, batch_y1, color);
                    }
                }
            }
        }
    }
}

int main() {
    int width = 1920;
    int height = 1080;
    printf("=== CCE Moving Grid Test (Layers) ===\n");

    set_engine_seed(1337);
    
    if (cce_engine_init() != 0) {
        printf("Engine init failed\n");
        return -1;
    }

    set_engine_msaa(8);
    
    Window* window = cce_window_create(width, height, CCE_NAME " " CCE_VERSION " | " "Moving Grid");
    if (!window) {
        printf("Window creation failed\n");
        cce_engine_cleanup();
        return -1;
    }
    
    cce_setup_2d_projection(width, height);
    
    CCE_FPS_Timer* timer = cce_fps_timer_create(60.0);
    
    TTF_Font* font = cce_font_load("/home/katcote/cce/examples/fonts/Fixedsys.ttf", 72);
    
    CCE_Layer* grid_layer1 = create_layer(width, height, "Grid Layer 1");
    CCE_Layer* grid_layer2 = create_layer(width, height, "Grid Layer 2");
    CCE_Layer* cloud_layer = create_layer(width, height, "Cloud Layer");
    CCE_Layer* text_layer = create_layer(width, height, "Text Layer");
    
    printf("Starting pixel grid rendering with layers...\n");
    
    int frame = 0;
    float fps = 0.0f;

    while (cce_window_should_close(window) == 0 && frame < 300)
    {
        if (cce_fps_timer_should_update(timer))
        {
            fps = timer->fps;
            
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            CCE_Color empty = cce_get_color(0, 0, 0, 0, Empty);
            set_pixel_rect(grid_layer1, 0, 0, width/2 - 1, height - 1, empty);
            set_pixel_rect(grid_layer2, width/2, 0, width - 1, height - 1, empty);
            set_pixel_rect(cloud_layer, 0, 0, width - 1, height - 1, empty);
            set_pixel_rect(text_layer, 0, 0, width - 1, height - 1, empty);
            
            draw_grid_to_layer(grid_layer1, 0, 0, width/2, height, 10, frame * 10, frame * 10, DefaultStone);
            draw_grid_to_layer(grid_layer2, width/2, 0, width, height, 5, frame * -5, frame * -5, DefaultGrass);
            draw_cloud_to_layer(cloud_layer, width/2 - 150, height/2, frame, 0, 250, 190843375);
            
            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", fps);
            CCE_Color text_color = cce_get_color(0, 0, 0, 0, DefaultLight);
            cce_draw_text(text_layer, font, fps_text, 50, 80, 1.0f, text_color);
            
            CCE_Layer* layers[] = {grid_layer1, grid_layer2, cloud_layer, text_layer};
            render_pie(layers, 4);
            
            cce_window_swap_buffers(window);
            
            frame++;
            if (frame % 60 == 0) {
                printf("Frame: %d, FPS: %.1f\n", frame, fps);
            }
        }
        
        cce_window_poll_events();
        usleep(100);
    }
    
    printf("Test completed\n");
    
    destroy_layer(grid_layer1);
    destroy_layer(grid_layer2);
    destroy_layer(cloud_layer);
    destroy_layer(text_layer);
    
    cce_font_free(font);
    cce_fps_timer_destroy(timer);
    cce_window_destroy(window);
    cce_engine_cleanup();
    
    return 0;
}