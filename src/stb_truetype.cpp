#include "kalam.h"

#define STBTT_malloc(x, u) ((void)(u), gPlatform.alloc(x))
#define STBTT_free(x, u) ((void)(u), gPlatform.free(x))

#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
