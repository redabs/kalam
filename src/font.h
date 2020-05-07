#ifndef FONT_H
#define FONT_H

#define GLYPH_SET_MAX 32
#define GLYPHS_PER_SET 256

typedef struct {
    bitmap_t Bitmap;
    stbtt_bakedchar Glyphs[GLYPHS_PER_SET];
} glyph_set_t;

typedef struct {
    stbtt_fontinfo StbInfo;
    platform_file_data_t File;
    
    f32 Height;
    f32 Size; 
    
    u32 SetCount;
    struct {
        u32 FirstCodepoint;
        // Set contains glyphs in the range [FirstCodepoint, FirstCodePoint + 255]
        glyph_set_t *Set;
    } GlyphSets[GLYPH_SET_MAX];
    
} font_t;

font_t f_load_ttf(char *Path, f32 Size);
u8 *f_utf8_to_codepoint(u8 *Utf8, u32 *Codepoint);
b32 f_get_glyph_set(font_t *Font, u32 Codepoint, glyph_set_t **Set);

#endif //FONT_H
