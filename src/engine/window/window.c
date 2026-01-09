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
#include "../../external/stb_image.h"

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

    // Cursor handling
    GLFWcursor* std_cursors[CCE_CURSOR_COUNT]; // only indices for standard shapes are used
    GLFWcursor* custom_cursor;
    CCE_CursorType base_cursor_type;
    int hover_override_enabled;
    CCE_CursorType hover_cursor_type;
    int cursor_inside;
};

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int cursor_type_valid(CCE_CursorType type)
{
    return type >= 0 && type < CCE_CURSOR_COUNT;
}

static GLFWcursor* cursor_handle_for_type(Window* window, CCE_CursorType type)
{
    if (!window) return NULL;

    if (!cursor_type_valid(type)) return NULL;
    if (type == CCE_CURSOR_CUSTOM) return window->custom_cursor;
    if (type == CCE_CURSOR_HIDDEN) return NULL;
    if (type == CCE_CURSOR_DISABLED) return NULL;

    return window->std_cursors[type];
}

static void apply_window_cursor(Window* window)
{
    if (!window || !window->handle) return;

    // Keep `cursor_inside` in sync even if the enter callback hasn't fired yet.
    window->cursor_inside = glfwGetWindowAttrib(window->handle, GLFW_HOVERED) ? 1 : 0;

    CCE_CursorType desired = window->base_cursor_type;
    if (window->hover_override_enabled && window->cursor_inside) {
        desired = window->hover_cursor_type;
    }

    if (!cursor_type_valid(desired)) {
        desired = CCE_CURSOR_ARROW;
    }

    if (desired == CCE_CURSOR_HIDDEN) {
        glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        glfwSetCursor(window->handle, NULL);
        return;
    }
    if (desired == CCE_CURSOR_DISABLED) {
        glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursor(window->handle, NULL);
        return;
    }

    // normal visible cursor
    glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursor(window->handle, cursor_handle_for_type(window, desired));
}

static void glfw_cursor_enter_callback(GLFWwindow* glfw_window, int entered)
{
    Window* window = (Window*)glfwGetWindowUserPointer(glfw_window);
    if (!window) return;

    window->cursor_inside = entered ? 1 : 0;
    apply_window_cursor(window);
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

    memset(window->std_cursors, 0, sizeof(window->std_cursors));
    window->custom_cursor = NULL;
    window->base_cursor_type = CCE_CURSOR_ARROW;
    window->hover_override_enabled = 0;
    window->hover_cursor_type = CCE_CURSOR_ARROW;
    window->cursor_inside = 0;
    
    if (!window->title)
    {
        free(window);
        glfwDestroyWindow(glfw_window);
        return NULL;
    }

    // Store pointer for callbacks and enable cursor enter/leave tracking.
    glfwSetWindowUserPointer(glfw_window, window);
    glfwSetCursorEnterCallback(glfw_window, glfw_cursor_enter_callback);

    // Create standard cursors (may return NULL if unsupported by platform/theme).
    window->std_cursors[CCE_CURSOR_ARROW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    window->std_cursors[CCE_CURSOR_IBEAM] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    window->std_cursors[CCE_CURSOR_CROSSHAIR] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    window->std_cursors[CCE_CURSOR_HAND] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    window->std_cursors[CCE_CURSOR_HRESIZE] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    window->std_cursors[CCE_CURSOR_VRESIZE] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    apply_window_cursor(window);

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
        if (window->custom_cursor) { glfwDestroyCursor(window->custom_cursor); }
        for (int i = 0; i < (int)CCE_CURSOR_COUNT; i++) {
            if (window->std_cursors[i]) { glfwDestroyCursor(window->std_cursors[i]); }
        }
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

int cce_window_set_cursor(Window* window, CCE_CursorType type)
{
    if (!window) return -1;
    if (!cursor_type_valid(type)) return -1;

    window->base_cursor_type = type;
    apply_window_cursor(window);
    return 0;
}

int cce_window_set_cursor_on_hover(Window* window, int enabled, CCE_CursorType type)
{
    if (!window) return -1;
    if (!cursor_type_valid(type)) return -1;

    window->hover_override_enabled = enabled ? 1 : 0;
    window->hover_cursor_type = type;
    apply_window_cursor(window);
    return 0;
}

int cce_window_set_cursor_image(Window* window, const char* filename, int hot_x, int hot_y)
{
    if (!window || !filename) return -1;

    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load(filename, &w, &h, &channels, 4);
    if (!data) {
        cce_printf("❌ Failed to load cursor image \"%s\": %s\n", filename, stbi_failure_reason());
        return -1;
    }

    if (w <= 0 || h <= 0) {
        stbi_image_free(data);
        return -1;
    }

    hot_x = clamp_int(hot_x, 0, w - 1);
    hot_y = clamp_int(hot_y, 0, h - 1);

    GLFWimage img = {
        .width = w,
        .height = h,
        .pixels = (unsigned char*)data,
    };

    GLFWcursor* cursor = glfwCreateCursor(&img, hot_x, hot_y);
    stbi_image_free(data);

    if (!cursor) {
        cce_printf("❌ Failed to create custom cursor for \"%s\"\n", filename);
        return -1;
    }

    if (window->custom_cursor) {
        glfwDestroyCursor(window->custom_cursor);
    }
    window->custom_cursor = cursor;
    apply_window_cursor(window);
    return 0;
}

void cce_window_clear_cursor_image(Window* window)
{
    if (!window) return;
    if (window->custom_cursor) {
        glfwDestroyCursor(window->custom_cursor);
        window->custom_cursor = NULL;
    }
    apply_window_cursor(window);
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
