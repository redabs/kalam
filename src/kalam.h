#ifndef KALAM_H
#define KALAM_H

typedef struct {
    u8 *Data;
    u64 Capacity;
    u64 Size; // In bytes
    struct {
        s64 ByteOff;
        s64 CharOff;
    } Cursor;
} text_buffer_t;

typedef struct {
    text_buffer_t Buffer;
} ctx_t;


void k_do_editor(platform_shared_t *Shared);
void k_init(platform_shared_t *Shared);

#endif //KALAM_H
