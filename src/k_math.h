#ifndef K_MATH_H
#define K_MATH_H

struct iv2 {
    s32 x, y;
};

struct irect {
    // Top-left
    union {
        iv2 p;
        struct {
            s32 x, y;
        };
    };
    s32 w, h;
};

inline b8
is_point_inside_rect(irect Rect, iv2 Point) {
    b8 h = (Point.x > Rect.x) && (Point.x <= (Rect.x + Rect.w));
    b8 v = (Point.y > Rect.y) && (Point.y <= (Rect.y + Rect.h));

    return h && v;
}

inline irect
rect_overlap(irect r0, irect r1) {
    irect Rect = {0};
    Rect.x = MAX(r0.x, r1.x);
    Rect.y = MAX(r0.y, r1.y);
    Rect.w = MAX(MIN(r0.x + r0.w - Rect.x, r1.x + r1.w - Rect.x), 0);
    Rect.h = MAX(MIN(r0.y + r0.h - Rect.y, r1.y + r1.h - Rect.y), 0);

    return Rect;
}

union color {
    struct {
        u8 b, g, r, a;
    };
    u8 e[4];
    u32 Argb; // 0xAARRGGBB
};

inline color
color_u32(u32 Argb) {
    color C = {0};
    C.Argb = Argb;
    return C;
}

#endif
