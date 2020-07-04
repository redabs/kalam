#ifndef KALAM_H
#define KALAM_H

#define TAB_WIDTH 4 

typedef struct {
    u8 *Data;
    s32 w;
    s32 h;
    s32 Stride;
} bitmap_t;

typedef struct {
    bitmap_t Bitmap;
    stbtt_bakedchar Glyphs[256];
} glyph_set_t;

#define GLYPH_SET_MAX 32

typedef struct {
    stbtt_fontinfo StbInfo;
    platform_file_data_t File;
    
    f32 Size; 
    
    s32 Ascent;
    s32 Descent;
    s32 LineGap;
    
    s32 MHeight; // The height in pixels of 'M'
    s32 MWidth; // The width in pixels of 'M'. This is used under the presumption of a monospace font.
    s32 SpaceWidth;
    
    u32 SetCount;
    struct {
        u32 FirstCodepoint;
        // Set contains glyphs in the range [FirstCodepoint, FirstCodePoint + 255]
        glyph_set_t *Set;
    } GlyphSets[GLYPH_SET_MAX];
    
} font_t;

typedef struct {
    font_t Font;
    iv2_t p;
    buffer_t *Buffers; // stb
    panel_ctx_t PanelCtx;
} ctx_t;

#endif //KALAM_H