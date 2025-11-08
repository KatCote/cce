#include "window.h"
#include "../engine.h"

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
