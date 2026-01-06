#include "init.h"
#include "../engine.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GLFW/glfw3.h>
#include <stdarg.h>

static int cce_initialized = 0;
long engine_seed = 10567348921509346;
int engine_msaa = 0;
RandPack RP;

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

    RP.r0 = rand();
    RP.r1 = rand();
    RP.r2 = rand();
    RP.r3 = rand();
    RP.r4 = rand();
    RP.r5 = rand();
    RP.r6 = rand();
    RP.r7 = rand();
    RP.r8 = rand();
    RP.r9 = rand();

    cce_printf("RandPack: %d (Engine Seed)\n", engine_seed);
    cce_printf("R0 - %d\n", RP.r0);
    cce_printf("R1 - %d\n", RP.r1);
    cce_printf("R2 - %d\n", RP.r2);
    cce_printf("R3 - %d\n", RP.r3);
    cce_printf("R4 - %d\n", RP.r4);
    cce_printf("R5 - %d\n", RP.r5);
    cce_printf("R6 - %d\n", RP.r6);
    cce_printf("R7 - %d\n", RP.r7);
    cce_printf("R8 - %d\n", RP.r8);
    cce_printf("R9 - %d\n", RP.r9);

    glEnable(GL_TEXTURE_2D);

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

void set_engine_msaa(int factor)
{ engine_msaa = factor; }

int get_engine_chunk_size(void)
{ return CHUNK_SIZE; }

int get_randpack_value(RandPackIndex index)
{
    switch (index)
    {
        case R0:
            return RP.r0;
        case R1:
            return RP.r1;
        case R2:
            return RP.r2;
        case R3:
            return RP.r3;
        case R4:
            return RP.r4;
        case R5:
            return RP.r5;
        case R6:
            return RP.r6;
        case R7:
            return RP.r7;
        case R8:
            return RP.r8;
        case R9:
            return RP.r9;
        default:
            return 715;
    }
}