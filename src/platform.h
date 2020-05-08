#ifndef PLATFORM_H
#define PLATFORM_H

#define GLYPH_SET_MAX 32

typedef struct {
    // Pixel format is 0xAARRGGBB, in little-endian, i.e. the blue channel is at the lowest address.
    u8 *Data;
    s32 Width;
    s32 Height;
    u8 BytesPerPixel;
} framebuffer_t;

typedef struct {
    void *Data;
    s64 Size;
} platform_file_data_t;

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
    
    f32 Height;
    f32 Size; 
    
    u32 SetCount;
    struct {
        u32 FirstCodepoint;
        // Set contains glyphs in the range [FirstCodepoint, FirstCodePoint + 255]
        glyph_set_t *Set;
    } GlyphSets[GLYPH_SET_MAX];
    
} font_t;

typedef struct {
    framebuffer_t *Framebuffer;
    font_t Font;
    input_event_buffer_t EventBuffer;
} platform_shared_t;

b32 platform_read_file(char *Path, platform_file_data_t *FileData);
void platform_free_file(platform_file_data_t *FileData);

#endif //PLATFORM_H
