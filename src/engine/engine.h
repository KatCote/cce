#ifndef CCE_ENGINE_GUARD_H
#define CCE_ENGINE_GUARD_H

#include "../cce.h"

extern long engine_seed;

#define ERRLOG fprintf(stderr, "CCE | ERROR: %s:%d\n", __FILE__, __LINE__)

void cce_printf(const char* format, ...);

struct Color {
    float r;
    float g;
    float b;
};

#endif