font_t
f_load_ttf(char *Path, f32 Size) {
    font_t Font = {0};
    
    platform_file_data_t File;
    stbtt_fontinfo Info;
    
    ASSERT(platform_read_file(Path, &File));
    
    s32 Status = stbtt_InitFont(&Info, File.Data, 0);
    
    s32 Ascent, Descent, LineGap;
    stbtt_GetFontVMetrics(&Info, &Ascent, &Descent, &LineGap);
    
    f32 Scale = stbtt_ScaleForMappingEmToPixels(&Info, Size);
    Font.Height = (Ascent - Descent + LineGap) * Scale;
    
    // Load the glyph sets that include the printable ASCII characters; they could be in more than one set.
    for(u8 Char = 0x20; Char < 0x7f; ++Char) {
        s32 GlyphIndex = stbtt_FindGlyphIndex(&Info, Char);
        s32 SetIndex = GlyphIndex / 256;
        glyph_set_t *Set = 0;
        for(s32 i = 0; i < Font.GlyphSetCount; ++i) {
            if(Font.GlyphSets[i].SetIndex == SetIndex) {
                Set = Font.GlyphSets[i].Set;
                break;
            }
        }
        
        if(!Set) {
            ASSERT(Font.GlyphSetCount < GLYPH_SET_MAX);
            Set = Font.GlyphSets[Font.GlyphSetCount].Set = malloc(sizeof(glyph_set_t));
            Font.GlyphSets[Font.GlyphSetCount].SetIndex = SetIndex;
            ++Font.GlyphSetCount;
            
            bitmap_t *Bitmap = &Set->Bitmap;
            Bitmap->w = 128;
            Bitmap->h = 128;
            Bitmap->Stride = Bitmap->w;
            Bitmap->Data = malloc(Bitmap->w * Bitmap->h);
            while(stbtt_BakeFontBitmap(File.Data, 0, Size, Bitmap->Data, Bitmap->w, Bitmap->h, SetIndex * 256, 256, Set->GlyphInfo) <= 0) {
                Bitmap->w *= 2;
                Bitmap->h *= 2;
                Bitmap->Stride = Bitmap->w;
                Bitmap->Data = realloc(Bitmap->Data, Bitmap->w * Bitmap->h);
            }
        }
    }
    
    platform_free_file(&File);
    
    return Font;
}

