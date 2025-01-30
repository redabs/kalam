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

#endif
