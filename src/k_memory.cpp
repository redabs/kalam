//
// Util
//

// Returns Dest + Size
inline void *
copy(void *Dest, void *Src, u64 Size) {
    u8 *d = (u8 *)Dest;
    u8 *s = (u8 *)Src;
    while(Size > 0) {
        *d++ = *s++;
        --Size;
    }
    return (void *)d;
}

#define zero_struct(StructPtr) zero((void *)(StructPtr), sizeof(*(StructPtr)))
inline void
zero(void *Dest, u64 Size) {
    u8 *d = (u8 *)Dest;
    while(Size > 0) {
        *d++ = 0;
        Size--;
    }
}

//
// Stretchy buffer
//

template<typename item_type> 
inline view<item_type>
make_view(buffer<item_type> Buffer) {
    view<item_type> Result = {};
    Result.Ptr = Buffer.Ptr;
    Result.Count = Buffer.Count;
    return Result;
}

template<typename item_type>
void
grow_to_fit(buffer<item_type> *Buffer, u64 Count) {
    if(Buffer->Capacity == 0) { Buffer->Capacity = 1; }

    ASSERT(Buffer->Count <= Buffer->Capacity);
    for(; (Buffer->Capacity - Buffer->Count) < Count; Buffer->Capacity *= 2);

    item_type *NewStorage = (item_type *)gPlatform.alloc(Buffer->Capacity * sizeof(item_type));
    ASSERT(NewStorage);

    copy(NewStorage, Buffer->Ptr, Buffer->Count * sizeof(item_type));
    gPlatform.free(Buffer->Ptr);
    Buffer->Ptr = NewStorage;
}

template<typename item_type>
void
maybe_grow(buffer<item_type> *Buffer, u64 Count) {
    if((Buffer->Count + Count) > Buffer->Capacity) {
        grow_to_fit(Buffer, Count);
    }
}

// Returns the starting index where Count sequential items can be accessed
// The allocated bytes are zeroed
template<typename item_type>
u64
add(buffer<item_type> *Buffer, u64 Count) {
    maybe_grow(Buffer, Count);

    u64 Idx = Buffer->Count;
    zero(Buffer->Ptr + Idx, Count * sizeof(item_type));
    Buffer->Count += Count;

    return Idx;
}

template<typename item_type>
void
push(buffer<item_type> *Buffer, view<item_type> Items) {
    u64 Start = add(Buffer, Items.Count);
    for(u64 i = 0; i < Items.Count; ++i) {
        Buffer->Ptr[Start + i] = Items.Ptr[i];
    }
}

template<typename item_type>
void
push(buffer<item_type> *Buffer, item_type Item) {
    u64 Idx = add(Buffer, 1);
    Buffer->Ptr[Idx] = Item;
}

template<typename item_type>
item_type
pop(buffer<item_type> *Buffer) {
    static item_type Empty;
    if(Buffer->Count == 0) { 
        return Empty;
    }
    
    item_type Result = Buffer->Data[--Buffer->Count];
    
    return Result;
}

template<typename item_type>
void
insert(buffer<item_type> *Buffer, item_type *Data, u64 Count, u64 Index) {
    ASSERT(Index <= Buffer->Count);
    maybe_grow(Buffer, Count);
    for(u64 i = Buffer->Count; i > Index; --i) {
        Buffer->Ptr[i + Count - 1] = Buffer->Ptr[i - 1];
    }

    for(u64 i = 0; i < Count; ++i) {
        Buffer->Ptr[i + Index] = Data[i];
    }
    Buffer->Count += Count;
}

template<typename item_type>
void
remove(buffer<item_type> *Buffer, u64 Index, u64 Count) {
    ASSERT((Index + Count) <= Buffer->Count);
    for(u64 i = 0; i < (Buffer->Count - Index - Count); ++i) {
        Buffer->Ptr[Index + i] = Buffer->Ptr[Index + Count + i];
    }

    Buffer->Count -= Count;
}

template<typename item_type>
void
clear(buffer<item_type> *Buffer) {
    Buffer->Count = 0;
}

template<typename item_type>
void
destroy(buffer<item_type> *Buffer) {
    if(Buffer->Ptr) { 
        gPlatform.free(Buffer->Ptr);
    }
    zero_struct(Buffer);
}

//
// Hash Table
//

inline u32
fnv1a_32_ex(u32 Hash, u8 *Data, u64 Size) {
    for(u64 i = 0; i < Size; ++i) {
        Hash ^= Data[i];
        Hash *= 0x01000193;
    }
    return Hash;
}

inline u32
fnv1a_32(u8 *Data, u64 Size) {
    return fnv1a_32_ex(0x811c9dc5, Data, Size);
}

inline u64
fnv1a_64_ex(u64 Hash, u8 *Data, u64 Size) {
    for(u64 i = 0; i < Size; ++i) {
        Hash ^= Data[i];
        Hash *= 0x00000100000001B3;
    }
    return Hash;
}

inline u64
fnv1a_64(u8 *Data, u64 Size) {
    return fnv1a_64_ex(0xcbf29ce484222325, Data, Size);
}

// Returns non-zero if there's a valid slot that Hash maps to
// A valid slot is one that either holds the Hash or is unoccupied
// Returns 0 if the table is full and the slot is not found.
inline u64
get_slot_index(table_slot *Slots, u64 Capacity, table_hash Hash) {
    if(Capacity == 0) {
        return 0;
    }
    
    // Tavble->Slots[0] is reserved, all indices are incremented by 1
    u64 IdxInit = Hash % Capacity + 1;
    u64 Idx = IdxInit;
    table_slot *Slot = Slots + Idx;
    // Probe until we can find an empty slot or one that holds the key we're looking for
    // Stop if we've come back around, if that is the case then the table does not contain
    // the key we're looking for and it must be full.
    for(u64 i = 1; Slot->Hash != 0 && (Slot->Hash != Hash); ++i) {
        // Quadratic probing
        // h(k, i) = h(k) + c1 * i + c2 * i * i (mod capacity)
        // We're choosing c1 = c2 = 1/2 = c0 and factoring out the divsion by 2 
        // h(k, i) = h(k) + (i + i * i) * c0 (mod capacity) = h(k) + (i + i * i) / 2 (mod capacity)
        Idx = (Hash + (i + i * i) / 2) % Capacity + 1;
        Slot = Slots + Idx;
        
        // TODO: Prove that we never loop back around.
        ASSERT(Idx != IdxInit);
    }
    
    return Idx;
}

inline void
grow_and_rehash(table *Table) {
    u64 NewCap = MAX(Table->Capacity, 32);
    // Allocate an additional slot for the reserved slot at Table->Slot[0]
    table_slot *NewStorage = (table_slot *)gPlatform.alloc((NewCap + 1) * sizeof(table_slot));
    zero((void *)NewStorage, (NewCap + 1) * sizeof(table_slot));
    
    for(u64 i = 0; i < Table->Used; ++i) {
        table_slot *OldSlot = Table->Slots + i + 1;
        if(OldSlot->Hash) {
            u64 NewIdx = get_slot_index(NewStorage, NewCap, OldSlot->Hash);
            ASSERT(NewIdx);
            NewStorage[NewIdx] = *OldSlot;
        }
    }
    
    Table->Capacity = NewCap;
    gPlatform.free((void *)Table->Slots);
    Table->Slots = NewStorage;
}

inline void
table_insert(table *Table, u64 Key, u64 Value) {
    if(Table->Capacity == 0 || ((Table->Used * 2) >= Table->Capacity)) {
        grow_and_rehash(Table);
    }
    
    
    table_hash Hash = fnv1a_64((u8*)&Key, sizeof(Key));
    u64 Idx = get_slot_index(Table->Slots, Table->Capacity, Hash);
    ASSERT(Idx);
    table_slot *Slot = Table->Slots + Idx;
    
    // Is it an empty slot?
    if(!Slot->Hash) {
        Table->Used++;
    }
    
    Slot->Hash = Hash;
    Slot->Key = Key;
    Slot->Value = Value;
}

inline void
table_remove(table *Table, u64 Key) {
    table_hash Hash = fnv1a_64((u8 *)&Key, sizeof(Key));
    u64 Idx = get_slot_index(Table->Slots, Table->Capacity, Hash);
    if(Idx) {
        table_slot *Slot = Table->Slots + Idx;
        // Is it a used slot?
        if(Slot->Hash) {
            zero(Slot, sizeof(table_slot));
            Table->Used--;
        }
    }
}

inline b8
table_find(table *Table, u64 Key, u64 *ValueOut) {
    table_hash Hash = fnv1a_64((u8*)&Key, sizeof(Key));
    u64 Idx = get_slot_index(Table->Slots, Table->Capacity, Hash);
    if(Idx) {
        table_slot *Slot = Table->Slots + Idx;
        if(Slot->Hash) {
            if(ValueOut) {
                *ValueOut = Slot->Value;
            }
            return true;
        }
    }
    return false;
}
