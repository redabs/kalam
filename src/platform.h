#ifndef PLATFORM_H
#define PLATFORM_H

typedef struct {
    void *Data;
    s64 Size;
} platform_file_data_t;

b32 platform_read_file(char *Path, platform_file_data_t *FileData);
void platform_free_file(platform_file_data_t *FileData);

#endif //PLATFORM_H
