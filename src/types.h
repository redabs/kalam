#ifndef TYPES_H
#define TYPES_H

typedef struct {
    s32 x;
    s32 y;
} ivec2_t;

typedef struct {
    // Top left
    s32 x;
    s32 y;
    s32 w;
    s32 h;
} irect_t;

typedef struct {
    u8 *Data;
    u64 Length;
} string_t;

typedef struct {
    // Pixel format is 0xAARRGGBB, in little-endian, i.e. the blue channel is at the lowest address.
    u8 *Data;
    s32 Width;
    s32 Height;
    u8 BytesPerPixel;
} framebuffer_t;

#endif //TYPES_H
