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

#endif //TYPES_H
