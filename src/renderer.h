#ifndef RENDERER_H
#define RENDERER_H

typedef struct {
    // Pixel format is 0xAARRGGBB, in little-endian, i.e. the blue channel is at the lowest address.
    u8 *Data;
    s32 Width;
    s32 Height;
    u8 BytesPerPixel;
} framebuffer_t;

typedef struct {
    s32 w;
    s32 h;
    s32 Stride;
    u8 *Data;
} bitmap_t;

void r_draw_rect(framebuffer_t *Fb, irect_t Rect, u32 Color);
void r_draw_rect_bitmap(framebuffer_t *Fb, s32 xPos, s32 yPos, irect_t Rect, bitmap_t *Bitmap);

#endif //RENDERER_H
