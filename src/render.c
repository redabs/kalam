glyph_set_t *
load_glyph_set(font_t *Font, u32 Codepoint) {
    if(Font->SetCount < GLYPH_SET_MAX) {
        glyph_set_t *Set = malloc(sizeof(glyph_set_t));
        u32 FirstCodepoint = Codepoint & (~0xff);
        Font->GlyphSets[Font->SetCount].Set = Set;
        Font->GlyphSets[Font->SetCount].FirstCodepoint = FirstCodepoint;
        Font->SetCount++;
        
        bitmap_t *b = &Set->Bitmap;
        b->w = 128;
        b->h = 128;
        b->Data = malloc(b->w * b->h);
        while(stbtt_BakeFontBitmap(Font->File.Data, 0, Font->Size, b->Data, b->w, b->h, FirstCodepoint, 256, Set->Glyphs) <= 0) {
            b->w *= 2;
            b->h *= 2;
            b->Data = realloc(b->Data, b->w * b->h);
        }
        b->Stride = b->w;
        
        return Set;
    }
    
    return 0;
}

b32
get_glyph_set(font_t *Font, u32 Codepoint, glyph_set_t **GlyphSet) {
    u32 FirstCodepoint = Codepoint & (~0xff);
    for(u32 i = 0; i < Font->SetCount; ++i) {
        if(Font->GlyphSets[i].FirstCodepoint == FirstCodepoint) {
            *GlyphSet = Font->GlyphSets[i].Set;
            return true;
        }
    }
    
    *GlyphSet = load_glyph_set(Font, Codepoint);
    return (*GlyphSet) != 0;
}

stbtt_bakedchar *
get_stbtt_bakedchar(font_t *Font, u32 Codepoint) {
    glyph_set_t *Set;
    if(get_glyph_set(Font, Codepoint, &Set)) {
        return &Set->Glyphs[Codepoint % 256];
    }
    return 0;
}

s32
get_x_advance(font_t *Font, u32 Codepoint) {
    s32 Result = Font->MWidth;
    if(Codepoint == '\t') {
        Result = Font->SpaceWidth * TAB_WIDTH;
    } else {
        stbtt_bakedchar *g = get_stbtt_bakedchar(Font, Codepoint);
        if(g) {
            Result = (s32)(g->xadvance + 0.5);
        }
    }
    return Result;
}

s32
text_width(font_t *Font, range_t Text) {
    s32 Result = 0;
    u8 *c = Text.Data;
    u8 *End = Text.Data + Text.Size;
    u32 Codepoint;
    while(c < End) {
        c = utf8_to_codepoint(c, &Codepoint);
        Result += get_x_advance(Font, Codepoint);
    }
    
    return Result;
}

// Returns {x, baseline}
iv2_t 
center_text_in_rect(font_t *Font, irect_t Rect, range_t Text) {
    s32 TextWidth = text_width(Font, Text);
    iv2_t Result = {.x = Rect.x + (Rect.w - TextWidth) / 2, .y = Rect.y + (Rect.h + Font->MHeight) / 2};
    return Result;
}

void
clear_framebuffer(framebuffer_t *Fb, color_t Color) {
    u64 Size = Fb->Width * Fb->Height;
    u32 *Dest = (u32 *)Fb->Data;
    for(u64 i = 0; i < Size; ++i) {
        Dest[i] = Color.Argb;
    }
}

void 
draw_rect(framebuffer_t *Fb, irect_t Rect, color_t Color) {
    s32 MinX = CLAMP(0, Fb->Width, MAX(Rect.x, Fb->Clip.x));
    s32 MaxX = CLAMP(0, Fb->Width, MIN(MinX + Rect.w - (MinX - Rect.x), Fb->Clip.x + Fb->Clip.w));
    
    s32 MinY = CLAMP(0, Fb->Height, MAX(Rect.y, Fb->Clip.y));
    s32 MaxY = CLAMP(0, Fb->Height, MIN(MinY + Rect.h - (MinY - Rect.y), Fb->Clip.y + Fb->Clip.h));
    
    if(Color.a == 0xff) {
        u32 *Row = (u32 *)Fb->Data + MinY * Fb->Width; 
        for(s32 y = MinY; y < MaxY; ++y) {
            u32 *Pixel = Row + MinX;
            for(s32 x = MinX; x < MaxX; ++x, ++Pixel) {
                *Pixel = *(u32*)&Color;
            }
            Row += Fb->Width;
        }
    } else {
        f32 a = (f32)(Color.a & 0xff) / 255.f;
        
        u32 *Row = (u32 *)Fb->Data + MinY * Fb->Width; 
        for(s32 y = MinY; y < MaxY; ++y) {
            u32 *Pixel = Row + MinX;
            for(s32 x = MinX; x < MaxX; ++x, ++Pixel) {
                f32 dr = (f32)((*Pixel >> 16) & 0xff);
                f32 dg = (f32)((*Pixel >> 8) & 0xff);
                f32 db = (f32)((*Pixel) & 0xff);
                
                u8 r = (u8)((1. - a) * dr + a * Color.r);
                u8 g = (u8)((1. - a) * dg + a * Color.g);
                u8 b = (u8)((1. - a) * db + a * Color.b);
                
                *Pixel = 0xff000000 | r << 16 | g << 8 | b;
            }
            Row += Fb->Width;
        }
    }
}

void
draw_glyph_bitmap(framebuffer_t *Fb, s32 xPos, s32 yPos, color_t Color, irect_t Rect, bitmap_t *Bitmap) {
    s32 BoxMinX = CLAMP(0, Fb->Width, MAX(xPos, Fb->Clip.x));
    s32 BoxMaxX = CLAMP(0, Fb->Width, MIN(MAX(xPos + Rect.w, 0), Fb->Clip.x + Fb->Clip.w));
    s32 xOff = MIN(BoxMinX - xPos, Rect.w); 
    // When the box we're drawing into is clipped xOff gives us the x-offset into the bitmap
    // that we should start sampling from.
    
    s32 BoxMinY = CLAMP(0, Fb->Height, MAX(yPos, Fb->Clip.y));
    s32 BoxMaxY = CLAMP(0, Fb->Height, MIN(MAX(yPos + Rect.h, 0), Fb->Clip.y + Fb->Clip.h));
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
            
            u8 r = (u8)((1. - a) * dr + a * Color.r);
            u8 g = (u8)((1. - a) * dg + a * Color.g);
            u8 b = (u8)((1. - a) * db + a * Color.b);
            
            *DestPixel = 0xff000000 | r << 16 | g << 8 | b;
            ++DestPixel;
            ++SrcPixel;
        }
        DestRow += Fb->Width;
        SrcRow += Bitmap->Stride;
    }
}

// Text is null-terminated if End is 0
void
draw_text_line(framebuffer_t *Fb, font_t *Font, s32 x, s32 Baseline, color_t Color, range_t String) {
    u8 *End = String.Data + String.Size;
    u8 *c = String.Data;
    s32 CursorX = x;
    while(c < End) {
        u32 Codepoint;
        if(*c == '\n' || *c == '\r') { 
            c += utf8_char_width(c);
        } else if(*c == '\t') {
            CursorX += Font->SpaceWidth * TAB_WIDTH;
            c += utf8_char_width(c);
        } else {
            c = utf8_to_codepoint(c, &Codepoint);
            
            glyph_set_t *Set;
            if(get_glyph_set(Font, Codepoint, &Set)) {
                stbtt_bakedchar *g = &Set->Glyphs[Codepoint % 256];
                irect_t gRect = {g->x0, g->y0, g->x1 - g->x0, g->y1 - g->y0};
                s32 yOff = (s32)(g->yoff + 0.5);
                s32 xOff = (s32)(g->xoff + 0.5);
                draw_glyph_bitmap(Fb, CursorX + xOff, Baseline + yOff, Color, gRect, &Set->Bitmap);
                
                CursorX += (s32)(g->xadvance + 0.5);
            }
        }
    }
}
