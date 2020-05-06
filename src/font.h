#ifndef FONT_H
#define FONT_H

typedef struct {
    bitmap_t Bitmap;
    stbtt_bakedchar GlyphInfo[256];
} glyph_set_t;

#define GLYPH_SET_MAX 32
typedef struct {
    s32 GlyphSetCount;
    // TODO: Cache table with pointers to most recently used glyphs?
    struct {
        glyph_set_t *Set;
        s32 SetIndex; // SetIndex = GlyphIndex / 256; for all glyph indices included in the set. If your glyph's index divided by 256 is equal to this set's SetIndex then it contains your glyph's info.
    } GlyphSets[GLYPH_SET_MAX];
    f32 Height;
} font_t;

font_t f_load_ttf(char *Path, f32 Size);

#endif //FONT_H
