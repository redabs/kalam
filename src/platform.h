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
    iv2_t MousePos;
} platform_shared_t;

typedef enum {
    FILE_FLAGS_Unknown    = 0,
    FILE_FLAGS_Directory  = 1 << 1,
    FILE_FLAGS_Hidden     = 1 << 2,
} file_flags_t; 

typedef struct {
    struct { u64 Offset; u64 Size; } Name; // Stored in FileNameBuffer in directory_t to avoid allocating each file name string individually.
    file_flags_t Flags;
} file_info_t;

typedef struct {
    mem_buffer_t Path; // Always ends with a directory delimiter
    mem_buffer_t FileNameBuffer;
    file_info_t *Files; // stb
} directory_t;

// From the platform to editor
b8 platform_read_file(range_t Path, platform_file_data_t *FileData);
void platform_free_file(platform_file_data_t *FileData);

// platform_get_files_in_directory should not include current path (".") or parent path ("..") in the result written to Directory.
b8 platform_get_files_in_directory(range_t Path, directory_t *Directory);

b8 platform_push_subdirectory(mem_buffer_t *CurrentPath, range_t SubDirectory);

// From the editor to the platform
void k_do_editor(platform_shared_t *Shared);
void k_init(platform_shared_t *Shared, range_t CurrentDirectory);

#endif //PLATFORM_H