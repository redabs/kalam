#ifndef MEMORY_H
#define MEMORY_H

inline void *
mem_copy(void *Dest, void *Src, u64 Size) {
    u8 *D = (u8 *)Dest;
    u8 *S = (u8 *)Src;
    for(u64 i = 0; i < Size; ++i) {
        *D++ = *S++;
    }
    return D;
} 

#define mem_zero_struct(Ptr) mem_zero((u8 *)Ptr, sizeof(*Ptr))
inline void
mem_zero(u8 *Ptr, u64 Size) {
    for(u64 i = 0; i < Size; ++i) {
        Ptr[i] = 0;
    }
}

#include <stdlib.h> // malloc, realloc, free
typedef struct {
    u64 Capacity; // Bytes allocated
    u64 Used; // Bytes used.
    u8 *Data;
} mem_buffer_t;

#define mem_buf_count(Buffer, Type) mem_buf_count_(Buffer, sizeof(Type))
inline s64 
mem_buf_count_(mem_buffer_t *Buffer, s64 ItemSize) {
    return Buffer->Used / ItemSize;
}

// Size is the additional size you need to fit by growing the buffer.
// It is not the size the buffer will grow by. If Grow succeeds
// the buffer will have grown enough to accomodate Size additional bytes.
inline void 
mem_buf_grow(mem_buffer_t *Buffer, u64 Size) {
    u64 NewCapacity = (Buffer->Capacity == 0) ? 1 : Buffer->Capacity;
    while((NewCapacity - Buffer->Used) < Size) {
        NewCapacity *= 2;
    }
    
    if(Buffer->Data) {
        Buffer->Data = (u8 *)realloc((void *)Buffer->Data, NewCapacity);
    } else {
        Buffer->Data = (u8 *)malloc(NewCapacity);
    }
    ASSERT(Buffer->Data != 0);
    
    Buffer->Capacity = NewCapacity;
}

inline b32 
mem_buf_need_grow(mem_buffer_t *Buffer, s64 Size) {
    return (Buffer->Used + Size) > Buffer->Capacity;
}

#define mem_buf_push_array(BUFFER, STRUCT_TYPE, COUNT) (STRUCT_TYPE *)mem_buf_push(BUFFER, sizeof(STRUCT_TYPE) * COUNT)
#define mem_buf_push_struct(BUFFER, STRUCT_TYPE) (STRUCT_TYPE *)mem_buf_push(BUFFER, sizeof(STRUCT_TYPE))
inline void *
mem_buf_push(mem_buffer_t *Buffer, s64 Size) {
    if(mem_buf_need_grow(Buffer, Size)) {
        mem_buf_grow(Buffer, Size);
    }
    void *Dest = Buffer->Data + Buffer->Used;
    mem_zero(Dest, Size);
    Buffer->Used += Size;
    return Dest;
}

#define mem_buf_push_array_idx(BUFFER, STRUCT_TYPE, COUNT, INDEX_PTR) (STRUCT_TYPE *)mem_buf_push_idx(BUFFER, COUNT, sizeof(STRUCT_TYPE), INDEX_PTR)
#define mem_buf_push_struct_idx(BUFFER, STRUCT_TYPE, INDEX_PTR) (STRUCT_TYPE *)mem_buf_push_idx(BUFFER, 1, sizeof(STRUCT_TYPE), INDEX_PTR)
inline void *
mem_buf_push_idx(mem_buffer_t *Buffer, s64 Count, s64 ElementSize, s64 *IndexOut) {
    *IndexOut = Buffer->Used / ElementSize;
    u8 *Result = (u8 *)mem_buf_push(Buffer, Count * ElementSize);
    return (void *)Result;
}

#endif //MEMORY_H