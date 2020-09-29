#ifndef INTRINSICS_H
#define INTRINSICS_H

#include <stdbool.h>
#include <stdint.h>
#include <float.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s32 b32;
typedef s8 b8;

typedef float f32;
typedef double f64;

#define local_persist static
#define internal static
#define file_scope static

#define ARRAY_COUNT(A) (sizeof(A) / sizeof(A[0]))

#define ASSERT(C) if(!(C)) { *(int *)0 = 0; } 

#define OFFSET_OF(Type, Member) ((size_t) &(((Type *)0)->Member))
#define REBASE(MemberInstance, StructName, MemberName) (StructName *)((u8 *)MemberInstance - OFFSET_OF(StructName, MemberName))

#define KILOBYTES(Value) (         (Value) * 1024ULL)
#define MEGABYTES(Value) (KILOBYTES(Value) * 1024ULL)
#define GIGABYTES(Value) (MEGABYTES(Value) * 1024ULL)
#define TERABYTES(Value) (GIGABYTES(Value) * 1024ULL)

#define ARE_BITS_SET(FIELD, MASK) ((FIELD & MASK) == MASK ? 1 : 0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define CLAMP(Low, High, x) MAX(MIN(x, High), Low)
#define ABS(x) ((x < 0) ? -(x) : (x))

// Takes a pointer to a string of utf-8 encoded characters
// Returns a pointer to the next character in the string
inline u8 *
utf8_to_codepoint(u8 *Utf8, u32 *Codepoint) {
    u32 Cp;
    int n;
    switch(*Utf8 & 0xf0) {
        case 0xf0: { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            Cp = *Utf8 & 0x7;
            n = 3;
        } break;
        
        case 0xe0: { // 1110xxxx 10xxxxxx 10xxxxxx 
            Cp = *Utf8 & 0xf;
            n = 2;
        } break;
        
        case 0xd0:
        case 0xc0: { // 110xxxxx 10xxxxxx 
            Cp = *Utf8 & 0x1f;
            n = 1;
        } break;
        
        default: {
            Cp = *Utf8;
            n = 0;
        } break;
    }
    
    while(n--) {
        Cp = Cp << 6 | (*(++Utf8) & 0x3f);
    }
    *Codepoint = Cp;
    return Utf8 + 1;
}

inline u8
utf16_to_utf8(u32 Utf16, u8 *Utf8) {
    if(Utf16 <= 0x7f) {
        Utf8[0] = Utf16 & 0x7f; 
        return 1;
        
    } else if(Utf16 <= 0x7ff) {
        Utf8[0] = 0xc0 | ((Utf16 >> 6) & 0x1f);
        Utf8[1] = 0x80 | (Utf16        & 0x3f);
        return 2;
        
    } else if(Utf16 >= 0xe000 && Utf16 <= 0xffff) {
        Utf8[0] = 0xe0 | ((Utf16 >> 12) & 0xf);
        Utf8[1] = 0x80 | ((Utf16 >> 6)  & 0x3f);
        Utf8[2] = 0x80 | ( Utf16        & 0x3f);
        return 3;
        
    } else {
        u32 High = Utf16 >> 16;
        u32 Low = Utf16 & 0xffff;
        // Surrogate pairs
        if((High >= 0xd800 && High <= 0xdfff)  && (Low >= 0xd800 && Low <= 0xdfff)) {
            High = (High - 0xd800) << 10;
            Low = Low - 0xdc00;
            u32 Codepoint = High + Low + 0x10000;
            
            Utf8[0] = 0xf0 | ((Codepoint >> 18) & 0x7);
            Utf8[1] = 0x80 | ((Codepoint >> 12) & 0x3f);
            Utf8[2] = 0x80 | ((Codepoint >> 6)  & 0x3f);
            Utf8[3] = 0x80 | ( Codepoint        & 0x3f);
        } 
        
        return 4;
    }
}


inline u32
codepoint_to_utf16(u32 Codepoint) {
    if(Codepoint <= 0xffff) {
        // Utf-16 encodes code points in this range as single 16-bit code units that are numerically equal to the corresponding code points.
        return Codepoint;
    } else {
        Codepoint -= 0x10000;
        u32 Result = 0;
        Result = ((Codepoint >> 10) + 0xd800) << 16; // High surrogate
        Result |= (Codepoint & 0x3ff) + 0xdc00; // Low surrogate
        return Codepoint;
    }
}


inline u8
utf8_char_width(u8 *Char) {
    switch(*Char & 0xf0) {
        case 0xf0: { return 4; } break;
        case 0xe0: { return 3; } break;
        case 0xd0:
        case 0xc0: { return 2; } break;
        default:   { return 1; } break;
    }
}

inline u8 *
utf8_move_back_one(u8 *Text) {
    u8 *c = Text - 1;
    while((*c & 0xc0) == 0x80) {
        --c;
    }
    return c;
}

inline u64
c_str_len(u8 *Str) {
    u64 l = 0;
    while(*Str++) {
        ++l;
    }
    return l;
}

#endif //INTRINSICS_H