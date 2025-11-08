#include "../../build/include/cce.h"
#include <stdio.h>
#include <GL/gl.h>
#include <unistd.h>

int main() {
    int width = 1920;
    int height = 1080;
    printf("=== CCE Pixel Grid Test ===\n");
    
    if (cce_engine_init() != 0) {
        printf("Engine init failed\n");
        return -1;
    }

    set_engine_seed(0);
    
    Window* window = cce_window_create(width, height, CCE_NAME " " CCE_VERSION " | " "Moving Grid");
    if (!window) {
        printf("Window creation failed\n");
        cce_engine_cleanup();
        return -1;
    }
    
    cce_setup_2d_projection(width, height);
    
    printf("Starting pixel grid rendering...\n");
    
    int frame = 0;
    static int a = 0;

    while (cce_window_should_close(window) == 0 && frame < 300)
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        cce_draw_grid(0, 0, width/2, height, 10, frame * 10, frame * 10, DefaultStone);
        cce_draw_grid(width/2, 0, width, height, 5, frame * -5, frame * -5, DefaultGrass);
        cce_window_swap_buffers(window);
        
        cce_window_poll_events();
        
        frame++;
        if (frame % 60 == 0) {
            printf("Frame: %d\n", frame);
        }
        usleep(16666);
    }
    
    printf("Test completed\n");
    cce_window_destroy(window);
    cce_engine_cleanup();
    
    return 0;
}