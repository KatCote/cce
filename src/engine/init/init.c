#include "init.h"
#include "../engine.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GLFW/glfw3.h>
#include <stdarg.h>

static int cce_initialized = 0;
long engine_seed = 0;

void cce_printf(const char* format, ...)
{
    printf("CCE | ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

int cce_engine_init(void)
{
    if (cce_initialized)
    {
        cce_printf("⚠️ Engine already initialized\n");
        return 0;
    }

    cce_printf("Initializing CCE with OpenGLFW...\n");
    
    glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_FALSE);
    
    if (!glfwInit())
    {
        cce_printf("❌ Failed to initialize GLFW\n");
        return -1;
    }
    
    cce_initialized = 1;
    cce_printf("✅ GLFW initialized successfully\n");

    srand(engine_seed);

    return 0;
}

void cce_engine_cleanup(void)
{
    cce_printf("Cleaning up CCE...\n");
    
    if (cce_initialized)
    {
        glfwTerminate();
        cce_initialized = 0;
        cce_printf("✅ Engine cleanup completed\n");
    }
    else
    {
        cce_printf("⚠️ Engine was not initialized\n");
    }
}

int cce_get_version(char ** ver_str_ptr)
{
    strcpy(*ver_str_ptr, CCE_VERSION);
    return 0;
}

void set_engine_seed(long new_engine_seed)
{ engine_seed = new_engine_seed; srand(engine_seed); }

long get_engine_seed(void)
{ return engine_seed; }