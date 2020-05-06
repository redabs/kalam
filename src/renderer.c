#include "intrinsics.h"
#include "types.h"

void 
r_draw_rectangle(framebuffer_t *Fb, irect_t Rect, u32 Color) {
    s32 MinX = MAX(Rect.x, 0);
    s32 MaxX = MIN(MinX + Rect.w, Fb->Width);
    
    s32 MinY = MAX(Rect.y, 0); 
    s32 MaxY = MIN(MinY + Rect.h, Fb->Height);
    
    u32 *Row = (u32 *)Fb->Data + MinY * Fb->Width; 
    for(s32 y = MinY; y < MaxY; ++y) {
        u32 *Pixel = Row + MinX;
        for(s32 x = MinX; x < MaxX; ++x, ++Pixel) {
            *Pixel = Color;
        }
        Row += Fb->Width;
    }
}