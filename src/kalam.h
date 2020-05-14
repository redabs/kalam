#ifndef KALAM_H
#define KALAM_H

typedef struct {
    u64 ByteOffset;
    u64 Size;
    u64 ColumnCount; // in characters
} line_t;

typedef struct {
    u8 *Data;
    u64 Capacity;
    u64 Used; // In bytes of contents
    mem_buffer_t Lines; // line_t
    struct {
        u64 Line; // 0 based
        u64 ColumnIs; // 0 based, see README.md on defining cursor movement behavior
        u64 ColumnWas;
        u64 ByteOffset;
    } Cursor;
} buffer_t;

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

typedef struct {
    stbtt_fontinfo StbInfo;
    platform_file_data_t File;
    
    f32 Size; 
    
    s32 Ascent;
    s32 Descent;
    s32 LineGap;
    
    s32 MHeight; // The height in pixels of 'M'
    s32 MWidth; // The width in pixels of 'M'. This is used under the presumption of a monospace font.
    
    u32 SetCount;
    struct {
        u32 FirstCodepoint;
        // Set contains glyphs in the range [FirstCodepoint, FirstCodePoint + 255]
        glyph_set_t *Set;
    } GlyphSets[GLYPH_SET_MAX];
    
} font_t;

typedef struct {
    font_t Font;
    ivec2_t p;
    buffer_t Buffer;
} ctx_t;

#endif //KALAM_H
