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

#include "window.h"
#include "../engine.h"

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct Window {
    GLFWwindow* handle;
    int width;
    int height;
    char* title;
    int batch_size;
    int monitor_index;
    int fullscreen;
};

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static GLFWmonitor* get_monitor_by_index(int monitor_index)
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (!monitors || count <= 0) {
        return glfwGetPrimaryMonitor();
    }

    int idx = clamp_int(monitor_index, 0, count - 1);
    return monitors[idx] ? monitors[idx] : glfwGetPrimaryMonitor();
}

static const GLFWvidmode* choose_vidmode(GLFWmonitor* monitor, int req_w, int req_h)
{
    if (!monitor) return NULL;

    const GLFWvidmode* current = glfwGetVideoMode(monitor);
    if (!current) return NULL;

    // If no resolution requested, keep current resolution but still try to prefer ~60Hz.
    if (req_w <= 0 || req_h <= 0) {
        req_w = current->width;
        req_h = current->height;
    }

    int mode_count = 0;
    const GLFWvidmode* modes = glfwGetVideoModes(monitor, &mode_count);
    if (!modes || mode_count <= 0) {
        return current;
    }

    // Choose the closest resolution first; among equal resolution distance choose refresh closest to 60Hz.
    const int target_hz = 60;
    int best = -1;
    long long best_res_dist = 0;
    int best_hz_dist = 0;

    for (int i = 0; i < mode_count; i++) {
        const int w = modes[i].width;
        const int h = modes[i].height;
        const int hz = modes[i].refreshRate;

        long long dx = (long long)w - (long long)req_w;
        long long dy = (long long)h - (long long)req_h;
        long long res_dist = dx * dx + dy * dy;

        int hz_dist = hz - target_hz;
        if (hz_dist < 0) hz_dist = -hz_dist;

        if (best < 0 ||
            res_dist < best_res_dist ||
            (res_dist == best_res_dist && hz_dist < best_hz_dist) ||
            (res_dist == best_res_dist && hz_dist == best_hz_dist && hz > modes[best].refreshRate))
        {
            best = i;
            best_res_dist = res_dist;
            best_hz_dist = hz_dist;
        }
    }

    if (best < 0) return current;
    return &modes[best];
}

static void center_window_on_monitor(GLFWwindow* glfw_window, GLFWmonitor* monitor, int width, int height)
{
    if (!glfw_window || !monitor) return;
    int wx = 0, wy = 0, mw = 0, mh = 0;
    glfwGetMonitorWorkarea(monitor, &wx, &wy, &mw, &mh);
    if (mw <= 0 || mh <= 0) return;

    int x = wx + (mw - width) / 2;
    int y = wy + (mh - height) / 2;
    glfwSetWindowPos(glfw_window, x, y);
}

Window* cce_window_create_ex(const CCE_WindowConfig* cfg)
{
    if (!cfg) {
        ERRLOG;
        return NULL;
    }

    const char* title = cfg->title ? cfg->title : CCE_NAME;
    const int batch_size = (cfg->batch_size > 0) ? cfg->batch_size : 1;
    const int fullscreen = (cfg->mode == CCE_WINDOW_FULLSCREEN) ? 1 : 0;

    GLFWmonitor* monitor = get_monitor_by_index(cfg->monitor_index);
    const GLFWvidmode* mode = fullscreen ? choose_vidmode(monitor, cfg->width, cfg->height) : NULL;

    int width = cfg->width;
    int height = cfg->height;
    if (fullscreen) {
        if (!mode) {
            cce_printf("❌ Failed to query monitor video mode\n");
            return NULL;
        }
        width = mode->width;
        height = mode->height;
    } else {
        if (width <= 0) width = 1280;
        if (height <= 0) height = 720;
    }

    glfwDefaultWindowHints();

    if (engine_msaa > 0)
    {
        glfwWindowHint(GLFW_SAMPLES, (GLint) engine_msaa);
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    if (fullscreen && mode) {
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }

    GLFWwindow* glfw_window = glfwCreateWindow(width, height, title, fullscreen ? monitor : NULL, NULL);

    if (!glfw_window)
    {
        cce_printf("❌ Failed to create OpenGL 3.3 core window\n");
        return NULL;
    }

    if (!fullscreen) {
        center_window_on_monitor(glfw_window, monitor, width, height);
    }

    glfwMakeContextCurrent(glfw_window);
    if (engine_msaa > 0) {
        glEnable(GL_MULTISAMPLE);
    }

    Window* window = malloc(sizeof(Window));
    if (!window)
    {
        glfwDestroyWindow(glfw_window);
        return NULL;
    }

    window->handle = glfw_window;
    window->width = width;
    window->height = height;
    window->title = copy_string(title);
    window->batch_size = batch_size;
    window->monitor_index = cfg->monitor_index;
    window->fullscreen = fullscreen;

    if (!window->title)
    {
        free(window);
        glfwDestroyWindow(glfw_window);
        return NULL;
    }

    cce_printf("✅ Window created: \"%s\" (%dx%d)%s (monitor %d, batch %d)\n",
        window->title,
        window->width,
        window->height,
        fullscreen ? " [fullscreen]" : "",
        window->monitor_index,
        window->batch_size
    );

    return window;
}

Window* cce_window_create(int width, int height, const char* title)
{
    CCE_WindowConfig cfg = {
        .title = title,
        .width = width,
        .height = height,
        .monitor_index = 0,
        .batch_size = 1,
        .mode = CCE_WINDOW_WINDOWED,
    };
    return cce_window_create_ex(&cfg);
}

void cce_window_destroy(Window* window)
{
    if (window)
    {
        cce_printf("Destroying window: \"%s\"\n", window->title);
        if (window->handle) { glfwDestroyWindow(window->handle); }
        if (window->title)  { free(window->title); }
        free(window);
    }
}

int cce_window_should_close(const Window* window)
{
    return window ? glfwWindowShouldClose(window->handle) : 1;
}

void cce_window_poll_events(void)
{
    glfwPollEvents();
}

void cce_window_swap_buffers(Window* window)
{
    if (window && window->handle) { glfwSwapBuffers(window->handle); }
}

void cce_window_make_current(Window* window)
{
    if (window && window->handle) { glfwMakeContextCurrent(window->handle); }
}

int cce_window_get_size(const Window* window, int* out_w, int* out_h)
{
    if (!window) return -1;
    if (out_w) *out_w = window->width;
    if (out_h) *out_h = window->height;
    return 0;
}

int cce_window_get_batch_size(const Window* window)
{
    if (!window) return 1;
    return window->batch_size > 0 ? window->batch_size : 1;
}

static char* copy_string(const char* src)
{
    if (!src) return NULL;
    
    size_t len = strlen(src);
    char* dest = malloc(len + 1);
    if (!dest) return NULL;
    
    strcpy(dest, src);

    return dest;
}
