#ifndef TYPES_H
#define TYPES_H

typedef struct {
    s32 x;
    s32 y;
} iv2_t;

typedef struct {
    // Top left
    s32 x;
    s32 y;
    s32 w;
    s32 h;
} irect_t;

typedef enum {
    UP    = 1,
    DOWN  = 1 << 1,
    LEFT  = 1 << 2,
    RIGHT = 1 << 3
} dir_t;

typedef struct {
    void *Data; // NOTHING is implied or guaranteed wrt. the allocation type of Data, only that [Data, Data + Size) is accessible bytes.
    u64 Size;
} range_t;

inline range_t
mem_buffer_as_range(mem_buffer_t Buf) {
    return (range_t){.Data = Buf.Data, .Size = Buf.Used};
}

#endif //TYPES_H
