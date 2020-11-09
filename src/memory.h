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

inline range_t
mem_buf_as_range(mem_buffer_t Buf) {
    return (range_t){.Data = Buf.Data, .Size = Buf.Used};
}

#define mem_buf_count(Buffer, Type) mem_buf_count_(Buffer, sizeof(Type))
inline u64 
mem_buf_count_(mem_buffer_t *Buffer, u64 ItemSize) {
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
mem_buf_need_grow(mem_buffer_t *Buffer, u64 Size) {
    return (Buffer->Used + Size) > Buffer->Capacity;
}

inline void
mem_buf_maybe_grow(mem_buffer_t *Buffer, u64 Size) {
    if(mem_buf_need_grow(Buffer, Size)) {
        mem_buf_grow(Buffer, Size);
    }
}

// mem_buf_null_terminate does not change the size of the data. It maybe grows the buffer to fit
// Count more bytes and writes Count null bytes after the data.
inline void
mem_buf_null_bytes(mem_buffer_t *Buffer, u64 Count) {
    mem_buf_maybe_grow(Buffer, Count);
    mem_zero(Buffer->Data + Buffer->Used, Count);
}

#define mem_buf_add_array(BUFFER, ELEMENT_TYPE, COUNT) (ELEMENT_TYPE *)mem_buf_add(BUFFER, sizeof(ELEMENT_TYPE) * COUNT)
#define mem_buf_add_struct(BUFFER, STRUCT_TYPE) (STRUCT_TYPE *)mem_buf_add(BUFFER, sizeof(STRUCT_TYPE))
inline void *
mem_buf_add(mem_buffer_t *Buffer, u64 Size) {
    if(mem_buf_need_grow(Buffer, Size)) {
        mem_buf_grow(Buffer, Size);
    }
    void *Dest = Buffer->Data + Buffer->Used;
    mem_zero(Dest, Size);
    Buffer->Used += Size;
    return Dest;
}

inline void
mem_buf_append(mem_buffer_t *Buffer, void *Data, u64 Size) {
    mem_copy(mem_buf_add(Buffer, Size), Data, Size);
}

inline void
mem_buf_append_range(mem_buffer_t *Buffer, range_t Range) {
    mem_buf_append(Buffer, Range.Data, Range.Size);
}

inline void 
mem_buf_pop_size(mem_buffer_t *Buffer, u64 Size) {
    Buffer->Used -= MIN(Buffer->Used, Size);
}

#define mem_buf_add_array_idx(BUFFER, ELEMENT_TYPE, COUNT, INDEX_PTR) (ELEMENT_TYPE *)mem_buf_add_idx(BUFFER, COUNT, sizeof(ELEMENT_TYPE), INDEX_PTR)
#define mem_buf_add_struct_idx(BUFFER, STRUCT_TYPE, INDEX_PTR) (STRUCT_TYPE *)mem_buf_add_idx(BUFFER, 1, sizeof(STRUCT_TYPE), INDEX_PTR)
inline void *
mem_buf_add_idx(mem_buffer_t *Buffer, u64 Count, u64 ElementSize, u64 *IndexOut) {
    *IndexOut = Buffer->Used / ElementSize;
    u8 *Result = (u8 *)mem_buf_add(Buffer, Count * ElementSize);
    return (void *)Result;
}

inline void
mem_buf_insert(mem_buffer_t *Buffer, u64 Offset, u64 Size, u8 *Data) {
    ASSERT(Size > 0);
    ASSERT(Offset <= Buffer->Used);
    
    mem_buf_maybe_grow(Buffer, Size);
    
    // Swiss cheese
    for(u64 i = Buffer->Used; i > Offset; --i) {
        Buffer->Data[i + Size - 1] = Buffer->Data[i - 1];
    }
    
    // Stuffing
    for(u64 i = 0; i < Size; ++i) {
        Buffer->Data[Offset + i] = Data[i];
    }
    Buffer->Used += Size;
}

inline void
mem_buf_delete(mem_buffer_t *Buffer, u64 Start, u64 Size) {
    Size = MIN(Buffer->Used - Start, Size);
    u64 End = Start + Size;
    for(u64 i = 0; i < (Buffer->Used - End); ++i) {
        Buffer->Data[Start + i] = Buffer->Data[Start + Size + i];
    }
    Buffer->Used -= Size;
}

inline void
mem_buf_free(mem_buffer_t *Buffer) {
    free(Buffer->Data);
}

inline void
mem_buf_clear(mem_buffer_t *Buffer) {
    Buffer->Used = 0;
}

#endif //MEMORY_H