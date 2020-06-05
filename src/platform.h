#ifndef PLATFORM_H
#define PLATFORM_H

#define GLYPH_SET_MAX 32

typedef struct {
    // Pixel format is 0xAARRGGBB, in little-endian, i.e. the blue channel is at the lowest address.
    u8 *Data;
    s32 Width;
    s32 Height;
    irect_t Clip;
} framebuffer_t;

typedef struct {
    void *Data;
    s64 Size;
} platform_file_data_t;

typedef struct {
    framebuffer_t *Framebuffer;
    input_event_buffer_t EventBuffer;
} platform_shared_t;

// From the platform to editor
b32 platform_read_file(char *Path, platform_file_data_t *FileData);
void platform_free_file(platform_file_data_t *FileData);

// From the editor to the platform
void k_do_editor(platform_shared_t *Shared);
void k_init(platform_shared_t *Shared);

#endif //PLATFORM_H
