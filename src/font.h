#ifndef FONT_H
#define FONT_H

typedef struct {
    s32 Index; // offset = Index * 256
    bitmap_t Bitmap;
    stbtt_bakedchar GlyphInfo[256];
} glyph_set_t;

#define GLYPH_SET_MAX 32
typedef struct {
    s32 GlyphSetCount;
    glyph_set_t *GlyphSets[GLYPH_SET_MAX];
    f32 Height;
} font_t;

font_t f_load_ttf(char *Path, f32 Size);

#endif //FONT_H
