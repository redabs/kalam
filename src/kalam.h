#ifndef KALAM_H
#define KALAM_H

typedef struct {
    u8 *Data;
    s64 Capacity;
    s64 Size; // In bytes of contents
    struct {
        s32 Line; // 0 based
        s32 ColumnIs; // 0 based, see README.md on defining cursor movement behavior
        s32 ColumnWas;
        s64 ByteOff;
    } Cursor;
} text_buffer_t;

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
    text_buffer_t Buffer;
    
} ctx_t;

void k_do_editor(platform_shared_t *Shared);
void k_init(platform_shared_t *Shared);

#endif //KALAM_H
