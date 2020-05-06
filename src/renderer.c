void 
r_draw_rect(framebuffer_t *Fb, irect_t Rect, u32 Color) {
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

void 
r_draw_rect_bitmap(framebuffer_t *Fb, s32 xPos, s32 yPos, irect_t Rect, bitmap_t *Bitmap) {
    s32 MinX = MAX(xPos, 0);
    s32 MaxX = MIN(MinX + Rect.w, Fb->Width);
    
    s32 MinY = MAX(yPos, 0); 
    s32 MaxY = MIN(MinY + Rect.h, Fb->Height);
    
    s32 w = MaxX - MinX;
    s32 h = MaxY - MinY;
    
    u32 *DestRow = (u32 *)Fb->Data + MinY * Fb->Width; 
    u8 *SrcRow = Bitmap->Data + Rect.y * Bitmap->Stride;
    for(s32 y = 0; y < h; ++y) {
        u32 *DestPixel = DestRow + MinX;
        u8 *SrcPixel = SrcRow + Rect.x;
        for(s32 x = 0; x < w; ++x) {
            u32 c = (u32)*SrcPixel;
            *DestPixel = c << 16 | c << 8 | c;
            ++DestPixel;
            ++SrcPixel;
        }
        DestRow += Fb->Width;
        SrcRow += Bitmap->Stride;
    }
}