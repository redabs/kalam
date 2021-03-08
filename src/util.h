#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h> // malloc, realloc, free

s32
round_f32(f32 v) {
    s32 s = (s32)(v + 0.5f);
    return s;
}

inline u64
fnv1a_64_ex(u64 Hash, range_t Data) {
    for(u64 i = 0; i < Data.Size; ++i) {
        Hash ^= Data.Data[i];
        Hash *= 0x100000001b3;
    }
    return Hash;
}

inline u64
fnv1a_64(range_t Data) {
    return fnv1a_64_ex(0xcbf29ce484222325, Data);
}

inline u32
fnv1a_32_ex(u32 Hash, range_t Data) {
    for(u64 i = 0; i < Data.Size; ++i) {
        Hash ^= Data.Data[i];
        Hash *= 0x01000193;
    }
    return Hash;
}

inline u32
fnv1a_32(range_t Range) {
    return fnv1a_32_ex(0x811c9dc5, Range);
}

u64
table_make_hash(range_t Key) {
    u64 Fnv = fnv1a_64(Key);
    
    return Fnv;
}

typedef struct {
    u64 Hash;
    mem_buffer_t Key;
    u64 Value;
} table_slot_t;

typedef struct {
    u64 SlotsAllocated;
    u64 SlotsUsed;
    u64 Collisions;
    u64 ValueSize;
    table_slot_t *Slots;
} table_t;

inline void
table_set_slot_unused(table_slot_t *Slot) {
    Slot->Hash = 0; 
}

inline b8
table_is_slot_used(table_slot_t *Slot) {
    return Slot->Hash != 0;
}

inline b8
table_key_equal(range_t k0, range_t k1) {
    if(k0.Size != k1.Size) {
        return false;
    }
    
    for(u64 i = 0; i < k0.Size; ++i) {
        if(k0.Data[i] != k1.Data[i]) {
            return false;
        }
    }
    
    return true;
}

inline table_t 
table_create() {
    table_t Table = {0};
    
    Table.SlotsAllocated = 1024;
    u64 Size = sizeof(Table.Slots[0]) * Table.SlotsAllocated;
    Table.Slots = malloc(Size);
    mem_zero(Table.Slots, Size);
    
    return Table;
}

inline void
table_destroy(table_t *Table) {
    free(Table->Slots);
}

inline void
table_next_slot(table_t *Table, u64 *Idx, table_slot_t **Slot) {
    // TODO: Quadratic probing
    u64 Next = (*Idx + 1) % Table->SlotsAllocated;
    *Slot = &Table->Slots[Next];
    *Idx = Next;
}

// Returns the index in the table where the item would go or where it already exists.
// Honestly this might be fucking stupid, I guess we'll find out.
inline u64
table_get_idx(table_t *Table, range_t Key, u64 Hash) {
    u64 Idx = Hash % Table->SlotsAllocated;
    table_slot_t *Slot = &Table->Slots[Idx];
    
    if(table_is_slot_used(Slot)) {
        b8 SameItem = Slot->Hash == Hash && table_key_equal(mem_buf_as_range(Slot->Key), Key);
        if(!SameItem) {
            Table->Collisions += 1;
            u64 i = Idx;
            table_next_slot(Table, &i, &Slot);
            
            // Find a slot that (isn't used) or (is used and contains the same item).
            while(table_is_slot_used(Slot) &&
                  (Slot->Hash != Hash && !table_key_equal(mem_buf_as_range(Slot->Key), Key))) {
                
                table_next_slot(Table, &i, &Slot);
                ASSERT(i != Idx); // No available slots and table is not full??
            }
            Idx = i;
        }
    }
    return Idx;
}

inline u64
table_next_table_size(u64 Size) {
    // next_pow_2(Size * (Size + 1) / 2)
    Size = (Size * (Size + 1)) >> 1;
    u64 LeadingZeroes = intrinsic_count_leading_zero_64bit(Size);
    Size = 1LL << (64 - LeadingZeroes);
    return Size;
}

inline void
table_grow_and_rehash(table_t *Table) {
    table_t NewTable = *Table;
    NewTable.Collisions = 0;
    
    NewTable.SlotsAllocated = table_next_table_size(NewTable.SlotsAllocated);
    
    NewTable.Slots = malloc(sizeof(Table->Slots[0]) * NewTable.SlotsAllocated);
    mem_zero(NewTable.Slots, sizeof(Table->Slots[0]) * NewTable.SlotsAllocated);
    
    for(u64 i = 0; i < Table->SlotsAllocated; ++i) {
        table_slot_t *Src = &Table->Slots[i];
        if(table_is_slot_used(Src)) {
            u64 Idx = table_get_idx(&NewTable, mem_buf_as_range(Src->Key), Src->Hash);
            NewTable.Slots[Idx] = *Src;
        }
    }
    
    free(Table->Slots);
    *Table = NewTable;
}

inline void
table_add(table_t *Table, range_t Key, u64 Value) {
    ASSERT(Table->SlotsAllocated > 0);
    
    if(((f64)Table->SlotsUsed / Table->SlotsAllocated) > 0.7) {
        table_grow_and_rehash(Table);
    }
    
    u64 Hash = table_make_hash(Key);
    ASSERT(Hash != 0);
    
    u64 Idx = table_get_idx(Table, Key, Hash);
    table_slot_t *Slot = &Table->Slots[Idx];
    Slot->Hash = Hash;
    mem_buf_clear(&Slot->Key);
    mem_buf_append_range(&Slot->Key, Key);
    Slot->Value = Value;
    
    Table->SlotsUsed += 1;
}

inline void
table_remove(table_t *Table, range_t Key) {
    u64 Hash = table_make_hash(Key);
    ASSERT(Hash != 0);
    
    u64 Idx = table_get_idx(Table, Key, Hash);
    table_slot_t *Slot = &Table->Slots[Idx];
    if(table_is_slot_used(Slot)) {
        table_set_slot_unused(Slot);
        Table->SlotsUsed -= 1;
    }
}

inline b8
table_get(table_t *Table, range_t Key, u64 *Value) {
    u64 Hash = table_make_hash(Key);
    u64 Idx = table_get_idx(Table, Key, Hash);
    *Value = Table->Slots[Idx].Value;
    b8 Valid = table_is_slot_used(&Table->Slots[Idx]);
    return Valid;
}

#endif //UTIL_H
