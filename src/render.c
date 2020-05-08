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

void 
draw_rect_bitmap(framebuffer_t *Fb, s32 xPos, s32 yPos, irect_t Rect, bitmap_t *Bitmap) {
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
