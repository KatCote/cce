#ifndef CCE_ENGINE_GUARD_H
#define CCE_ENGINE_GUARD_H

#include "../cce.h"

extern long engine_seed;
extern int engine_msaa;

#define ERRLOG fprintf(stderr, "CCE | ERROR: %s:%d\n", __FILE__, __LINE__)
#define M_PI 3.14159265358979323846
#define CHUNK_SIZE 270
#define COMPRESS(a, b, c) b + (a * c) / 255
#define CCE_DEBUG 0

void cce_printf(const char* format, ...);

extern RandPack RP;

#endif