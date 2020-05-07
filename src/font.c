// Takes a pointer to a string of utf-8 encoded characters
// Returns a pointer to the next character in the string
u8 *
f_utf8_to_codepoint(u8 *Utf8, u32 *Codepoint) {
    u32 Cp;
    u8 ByteCount;
    switch(*Utf8 & 0xf0) {
        case 0xd0:
        case 0xc0: { // 110xxxxx 10xxxxxx 
            Cp = *Utf8 & 0x1f;
            ByteCount = 1;
        } break;
        
        case 0xe0: { // 1110xxxx 10xxxxxx 10xxxxxx 
            Cp = *Utf8 & 0xf;
            ByteCount = 2;
        } break;
        
        case 0xf0: { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            Cp = *Utf8 & 0x7;
            ByteCount = 3;
        } break;
        
        default: {
            Cp = *Utf8;
            ByteCount = 0;
        } break;
    }
    
    while(ByteCount--) {
        Cp = Cp << 6 | (*(++Utf8) & 0x3f);
    }
    *Codepoint = Cp;
    return Utf8 + 1;
}

glyph_set_t *
f_load_glyph_set(font_t *Font, u32 Codepoint) {
    if(Font->SetCount < GLYPH_SET_MAX) {
        glyph_set_t *Set = malloc(sizeof(glyph_set_t));
        u32 FirstCodepoint = Codepoint / GLYPHS_PER_SET;
        Font->GlyphSets[Font->SetCount].Set = Set;
        Font->GlyphSets[Font->SetCount].FirstCodepoint = FirstCodepoint;
        Font->SetCount++;
        
        bitmap_t *b = &Set->Bitmap;
        b->w = 128;
        b->h = 128;
        b->Data = malloc(b->w * b->h);
        while(stbtt_BakeFontBitmap(Font->File.Data, 0, Font->Size, b->Data, b->w, b->h, FirstCodepoint, GLYPHS_PER_SET, Set->Glyphs) <= 0) {
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
f_get_glyph_set(font_t *Font, u32 Codepoint, glyph_set_t **GlyphSet) {
    u32 FirstCodepoint = Codepoint / GLYPHS_PER_SET;
    for(u32 i = 0; i < Font->SetCount; ++i) {
        if(Font->GlyphSets[i].FirstCodepoint == FirstCodepoint) {
            *GlyphSet = Font->GlyphSets[i].Set;
            return true;
        }
    }
    
    *GlyphSet = f_load_glyph_set(Font, Codepoint);
    return (*GlyphSet) != 0;
}

font_t
f_load_ttf(char *Path, f32 Size) {
    font_t Font = {0};
    Font.Size = Size;
    
    ASSERT(platform_read_file(Path, &Font.File));
    
    s32 Status = stbtt_InitFont(&Font.StbInfo, Font.File.Data, 0);
    
    s32 Ascent, Descent, LineGap;
    stbtt_GetFontVMetrics(&Font.StbInfo, &Ascent, &Descent, &LineGap);
    
    f32 Scale = stbtt_ScaleForMappingEmToPixels(&Font.StbInfo, Size);
    Font.Height = (Ascent - Descent + LineGap) * Scale;
    
    
    return Font;
}

