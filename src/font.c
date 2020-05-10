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

font_t
load_ttf(char *Path, f32 Size) {
    font_t Font = {0};
    Font.Size = Size;
    
    ASSERT(platform_read_file(Path, &Font.File));
    
    s32 Status = stbtt_InitFont(&Font.StbInfo, Font.File.Data, 0);
    
    stbtt_GetFontVMetrics(&Font.StbInfo, &Font.Ascent, &Font.Descent, &Font.LineGap);
    f32 Scale = stbtt_ScaleForMappingEmToPixels(&Font.StbInfo, Size);
    Font.Ascent = (s32)(Font.Ascent * Scale);
    Font.Descent = (s32)(Font.Descent * Scale);
    Font.LineGap = (s32)(Font.LineGap * Scale);
    
    {
        glyph_set_t *Set;
        if(get_glyph_set(&Font, 'M', &Set)) {
            stbtt_bakedchar *g = &Set->Glyphs['M'];
            Font.MHeight = (s32)(g->y1 - g->y0 + 0.5);
        }
    }
    
    
    return Font;
}
