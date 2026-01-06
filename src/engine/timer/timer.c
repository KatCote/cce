#include "timer.h"
#include "../engine.h"

#include <stdlib.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

CCE_FPS_Timer* cce_fps_timer_create(double target_fps) {
    CCE_FPS_Timer* timer = malloc(sizeof(CCE_FPS_Timer));
    timer->target_fps = target_fps;
    timer->frame_time = 1.0 / target_fps;
    timer->last_time = glfwGetTime();
    timer->accumulator = 0.0;
    timer->frame_count = 0;
    timer->fps = 0.0;
    timer->fps_timer = 0.0;
    timer->fps_start_time = glfwGetTime(); // Track when FPS measurement started
    return timer;
}

int cce_fps_timer_should_update(CCE_FPS_Timer* timer) {
    double current_time = glfwGetTime();
    double delta_time = current_time - timer->last_time;
    timer->last_time = current_time;
    
    // Ограничиваем дельта-тайм при лагах
    if (delta_time > 0.25) delta_time = 0.25;
    
    timer->accumulator += delta_time;
    
    if (timer->accumulator >= timer->frame_time) {
        timer->accumulator -= timer->frame_time;
        
        // Подсчёт FPS
        timer->frame_count++;
        
        // Track actual elapsed time since FPS measurement started
        double elapsed_time = current_time - timer->fps_start_time;
        
        if (elapsed_time >= 1.0) {
            // Calculate FPS using actual elapsed time
            timer->fps = timer->frame_count / elapsed_time;
            timer->frame_count = 0;
            timer->fps_start_time = current_time; // Reset start time
        } else if (elapsed_time > 0.1 && timer->frame_count > 0) {
            // Provide a preliminary FPS estimate after at least 0.1 seconds
            timer->fps = timer->frame_count / elapsed_time;
        }
        
        return 1;
    }
    
    return 0;
}