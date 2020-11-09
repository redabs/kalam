#ifndef PLATFORM_H
#define PLATFORM_H

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

typedef enum {
    FILE_FLAGS_Unknown    = 0,
    FILE_FLAGS_Directory  = 1 << 1,
    FILE_FLAGS_Hidden     = 1 << 2,
} file_flags_t; 


// TODO: File names are dynamically allocated individually, they should all have the same storage for faster deallocation.
typedef struct {
    mem_buffer_t FileName;
    file_flags_t Flags;
} file_info_t;

typedef struct {
    file_info_t *Files; // stb
} files_in_directory_t;

inline void
free_files_in_directory(files_in_directory_t *Dir) {
    for(s64 i = 0; i < sb_count(Dir->Files); ++i) {
        mem_buf_free(&Dir->Files[i].FileName);
    }
    sb_free(Dir->Files);
}

// From the platform to editor
b32 platform_read_file(range_t Path, platform_file_data_t *FileData);
void platform_free_file(platform_file_data_t *FileData);
files_in_directory_t platform_get_files_in_directory(range_t Path);

// From the editor to the platform
void k_do_editor(platform_shared_t *Shared);
void k_init(platform_shared_t *Shared, range_t WorkingDirectory);

#endif //PLATFORM_H
