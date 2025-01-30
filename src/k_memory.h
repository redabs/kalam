#ifndef MEMORY_H
#define MEMORY_H

//
// Util
//

template<typename item_type>
struct view {
    item_type *Ptr;
    u64 Count;
};

#define C_STR_VIEW(CString) (make_view((u8 *)CString, sizeof(CString) - sizeof(CString[0])))
inline view<u8>
make_view(u8 *Data, u64 Size) {
    view<u8> Result = {};
    Result.Ptr = Data;
    Result.Count = Size;
    return Result;
}

//
// Stretchy buffer
//

template<typename item_type>
struct buffer {
    u64 Count;
    u64 Capacity;
    item_type *Ptr;
};

//
// Hash Table
//

typedef u64 table_hash;

struct table_slot {
    u64 Value;
    u64 Key;
    u64 Hash;
};

struct table {
    u64 Used;
    u64 Capacity;
    table_slot *Slots;
};

#endif
