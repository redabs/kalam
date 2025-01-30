#ifndef K_PLATFORM_H
#define K_PLATFORM_H

struct framebuffer {
    s32 Width;
    s32 Height;
    // 4 bytes per pixel in format:
    // 0: 0xBB 0xGG 0xRR 0xAA
    // 4: ...
    u32 *Pixels; 
};

#define PLATFORM_ALLOC(Name) void *Name(u64 Size)
typedef PLATFORM_ALLOC(platform_alloc_f);

#define PLATFORM_FREE(Name) void Name(void *Ptr)
typedef PLATFORM_FREE(platform_free_f);

#define PLATFORM_READ_FILE(Name) buffer<u8> Name(view<u8> Path)
typedef PLATFORM_READ_FILE(platform_read_file_f);

#define PLATFORM_WRITE_FILE(Name) void Name(view<u8> Path, view<u8> Data)
typedef PLATFORM_WRITE_FILE(platform_write_file_f);

struct platform_api {
    platform_alloc_f *alloc;
    platform_free_f *free;
    platform_read_file_f *read_file;
    platform_write_file_f *write_file;
}; 

extern platform_api gPlatform;

#endif
