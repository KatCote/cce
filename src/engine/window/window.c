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
};

Window* cce_window_create(int width, int height, const char* title)
{
    glfwDefaultWindowHints();

    if (engine_msaa > 0)
    {
        glfwWindowHint(GLFW_SAMPLES, (GLint) engine_msaa);
        glEnable(GL_MULTISAMPLE);
    }
    
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
    
    GLFWwindow* glfw_window = glfwCreateWindow(width, height, title, NULL, NULL);
    
    if (!glfw_window)
    {
        cce_printf("Trying OpenGL 2.1...\n");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        
        glfw_window = glfwCreateWindow(width, height, title, NULL, NULL);
        
        if (!glfw_window)
        {
            cce_printf("❌ Failed to create OpenGL 2.1 window\n");
            return NULL;
        }
    }
    
    glfwMakeContextCurrent(glfw_window);
    
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
    
    if (!window->title)
    {
        free(window);
        glfwDestroyWindow(glfw_window);
        return NULL;
    }

    cce_printf("✅ Window created: \"%s\"\n", window->title);

    return window;
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

static char* copy_string(const char* src)
{
    if (!src) return NULL;
    
    size_t len = strlen(src);
    char* dest = malloc(len + 1);
    if (!dest) return NULL;
    
    strcpy(dest, src);

    return dest;
}
