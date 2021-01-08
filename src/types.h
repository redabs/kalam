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

typedef union {
    struct {
        u8 r,g,b,a;
    };
    u8 e[4];
} color_t;

typedef enum {
    UP    = 1,
    DOWN  = 1 << 1,
    LEFT  = 1 << 2,
    RIGHT = 1 << 3
} dir_t;

#define C_STR_AS_RANGE(C_STR) (range_t){.Data = (u8 *)C_STR, .Size = sizeof(C_STR) - sizeof(C_STR[0])}
typedef struct {
    u8 *Data; // NOTHING is implied or guaranteed wrt. the allocation type of Data, only that [Data, Data + Size) are accessible bytes.
    u64 Size;
} range_t;

#endif //TYPES_H
