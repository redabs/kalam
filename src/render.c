// Takes a pointer to a string of utf-8 encoded characters
// Returns a pointer to the next character in the string
inline u8 *
utf8_to_codepoint(u8 *Utf8, u32 *Codepoint) {
    u32 Cp;
    int n;
    switch(*Utf8 & 0xf0) {
        case 0xf0: { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            Cp = *Utf8 & 0x7;
            n = 3;
        } break;
        
        case 0xe0: { // 1110xxxx 10xxxxxx 10xxxxxx 
            Cp = *Utf8 & 0xf;
            n = 2;
        } break;
        
        case 0xd0:
        case 0xc0: { // 110xxxxx 10xxxxxx 
            Cp = *Utf8 & 0x1f;
            n = 1;
        } break;
        
        default: {
            Cp = *Utf8;
            n = 0;
        } break;
    }
    
    while(n--) {
        Cp = Cp << 6 | (*(++Utf8) & 0x3f);
    }
    *Codepoint = Cp;
    return Utf8 + 1;
}

void
clear_framebuffer(framebuffer_t *Fb, u32 Color) {
    u64 Size = Fb->Width * Fb->Height;
    u32 *Dest = (u32 *)Fb->Data;
    for(u64 i = 0; i < Size; ++i) {
        Dest[i] = Color;
    }
}
/*
The quick brown fucker yeeted the lazy zoomer
The quick brown fucker yeeted the lazy zoomer
*/
void 
draw_rect(framebuffer_t *Fb, irect_t Rect, u32 Color) {
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

#include <math.h>

void
draw_glyph_bitmap(framebuffer_t *Fb, s32 xPos, s32 yPos, irect_t Rect, bitmap_t *Bitmap) {
    s32 BoxMinX = MAX(xPos, 0);
    s32 BoxMaxX = MIN(MAX(xPos + Rect.w, 0), Fb->Width);
    s32 xOff = MIN(BoxMinX - xPos, Rect.w); 
    // When the box we're drawing into is clipped xOff gives us the x-offset into the bitmap
    // that we should start sampling from.
    
    s32 BoxMinY = MAX(yPos, 0);
    s32 BoxMaxY = MIN(MAX(yPos + Rect.h, 0), Fb->Height);
    s32 yOff = MIN(BoxMinY - yPos, Rect.h); 
    
    s32 w = BoxMaxX - BoxMinX;
    s32 h = BoxMaxY - BoxMinY;
    
    u32 *DestRow = (u32 *)Fb->Data + BoxMinY * Fb->Width;
    u8 *SrcRow = Bitmap->Data + (Rect.y + yOff) * Bitmap->Stride;
    for(s32 y = 0; y < h; ++y) {
        u32 *DestPixel = DestRow + BoxMinX;
        u8 *SrcPixel = SrcRow + Rect.x + xOff;
        for(s32 x = 0; x < w; ++x) {
            u8 s = (u8)MIN(pow((f32)*SrcPixel, 1.2), 255.f);
            u8 dr = (*DestPixel >> 16) & 0xff;
            u8 dg = (*DestPixel >> 8)  & 0xff;
            u8 db =  *DestPixel        & 0xff;
            
            f32 a = (f32)(*SrcPixel) / 255.f;
            
            u8 r = (u8)((1. - a) * dr + a * s);
            u8 g = (u8)((1. - a) * dg + a * s);
            u8 b = (u8)((1. - a) * db + a * s);
            
            *DestPixel = 0xff000000 | r << 16 | g << 8 | b;
            ++DestPixel;
            ++SrcPixel;
        }
        DestRow += Fb->Width;
        SrcRow += Bitmap->Stride;
    }
}
