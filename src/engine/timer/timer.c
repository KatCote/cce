/*
===========================================================================
MIT License

Copyright (c) 2026 Stepan Pukhovskiy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
===========================================================================
*/

#include "timer.h"
#include "../engine.h"

#include <stdlib.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

CCE_FPS_Timer* cce_fps_timer_create(double target_fps)
{
    CCE_FPS_Timer* timer = malloc(sizeof(CCE_FPS_Timer));
    timer->target_fps = target_fps;
    timer->frame_time = 1.0 / target_fps;
    timer->last_time = glfwGetTime();
    timer->accumulator = 0.0;
    timer->frame_count = 0;
    timer->fps = 0.0;
    timer->fps_timer = 0.0;
    timer->fps_start_time = glfwGetTime();
    return timer;
}

int cce_fps_timer_should_update(CCE_FPS_Timer* timer)
{
    double current_time = glfwGetTime();
    double delta_time = current_time - timer->last_time;
    timer->last_time = current_time;
    
    if (delta_time > 0.25)
    { delta_time = 0.25; }
    
    timer->accumulator += delta_time;
    
    if (timer->accumulator >= timer->frame_time)
    {
        timer->accumulator -= timer->frame_time;
        
        timer->frame_count++;
        
        double elapsed_time = current_time - timer->fps_start_time;
        
        if (elapsed_time >= 1.0)
        {
            timer->fps = timer->frame_count / elapsed_time;
            timer->frame_count = 0;
            timer->fps_start_time = current_time;
        } else if (elapsed_time > 0.1 && timer->frame_count > 0)
        {
            timer->fps = timer->frame_count / elapsed_time;
        }
        
        return 1;
    }
    
    return 0;
}

void cce_fps_timer_destroy(CCE_FPS_Timer* timer)
{
    if (timer)
    { free(timer); }
}

double cce_fps_timer_get_fps(CCE_FPS_Timer* timer)
{
    if (timer)
    { return timer->fps; }
    return 0.0;
}

double cce_fps_timer_get_delta(CCE_FPS_Timer* timer)
{
    if (timer)
    { return timer->frame_time; }
    return 0.0;
}