#ifndef INTRINSICS_H
#define INTRINSICS_H

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

#define ARRAY_COUNT(A) (sizeof(A) / sizeof(A[0]))

#define ASSERT(C) ((C) ? (void)0 : *(int *)0 = 0)

#define OFFSET_OF(Type, Member) ((size_t) &(((Type *)0)->Member))
#define REBASE(MemberInstance, StructName, MemberName) (StructName *)((u8 *)MemberInstance - OFFSET_OF(StructName, MemberName))

#define KILOBYTES(Value) (         (Value) * 1024ULL)
#define MEGABYTES(Value) (KILOBYTES(Value) * 1024ULL)
#define GIGABYTES(Value) (MEGABYTES(Value) * 1024ULL)
#define TERABYTES(Value) (GIGABYTES(Value) * 1024ULL)

#define ARE_BITS_SET(FIELD, MASK) ((FIELD & MASK) == MASK ? 1 : 0)

#endif