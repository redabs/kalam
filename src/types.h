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

inline b8
is_point_inside_rect(irect_t Rect, iv2_t Point) {
    b8 h = (Point.x > Rect.x) && (Point.x <= (Rect.x + Rect.w));
    b8 v = (Point.y > Rect.y) && (Point.y <= (Rect.y + Rect.h));
    
    return h && v;
}

inline irect_t
rect_overlap(irect_t r0, irect_t r1) {
    irect_t Rect = {0};
    Rect.x = MAX(r0.x, r1.x);
    Rect.y = MAX(r0.y, r1.y);
    Rect.w = MAX(MIN(r0.x + r0.w - Rect.x, r1.x + r1.w - Rect.x), 0);
    Rect.h = MAX(MIN(r0.y + r0.h - Rect.y, r1.y + r1.h - Rect.y), 0);
    
    return Rect;
}

typedef union {
    struct {
        u8 b, g, r, a;
    };
    u8 e[4];
    u32 Argb; // 0xAARRGGBB
} color_t;

inline color_t
color_u32(u32 Argb) {
    color_t Color = {.Argb = Argb};
    return Color;
}

typedef enum {
    UP    = 1,
    DOWN  = 1 << 1,
    LEFT  = 1 << 2,
    RIGHT = 1 << 3
} direction_t;

#define C_STR_AS_RANGE(C_STR) (range_t){.Data = (u8 *)C_STR, .Size = sizeof(C_STR) - sizeof(C_STR[0])}
typedef struct {
    u8 *Data; // NOTHING is implied or guaranteed wrt. the allocation type of Data, only that [Data, Data + Size) are accessible bytes.
    u64 Size;
} range_t;

#endif //TYPES_H
