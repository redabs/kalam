u64
make_glyph_key(glyph_key_data KeyData) {
    u8 Buf[sizeof(glyph_key_data)];
    copy(Buf, (void *)&KeyData, sizeof(Buf));
    u64 Key = fnv1a_64(Buf, sizeof(Buf));
    return Key;
}

// Always wrties a ready to be used slot index to SlotIndexOut
// Returns true if the slot at SlotIndexOut matches KeyHash
b8
glyph_cache_get_index(glyph_cache *Cache, u64 KeyHash, u32 *SlotIndexOut) {
    u32 Row = (u32)(KeyHash % Cache->Rows);
    u32 RowIndex = Row * Cache->Columns; // Index of first slot in row
    
    // Find empty slot in row or evict least recently used.
    
    s64 EmptyIndex = -1;
    u32 LruIndex = RowIndex;
    u32 MinLru = Cache->Slots[RowIndex].LruCount; // Init to first slot in row
    
    for(u32 i = 0; i < Cache->Columns; ++i) {
        u32 SlotIndex = RowIndex + i;
        glyph_info *Slot = &Cache->Slots[SlotIndex];
        
        // Matching glyph
        if(KeyHash == Slot->Key) {
            *SlotIndexOut = SlotIndex;
            return true;
        }
        
        EmptyIndex = (!Slot->Codepoint) ? SlotIndex : EmptyIndex;
        
        // Least recently used slot
        if(Slot->LruCount < MinLru) {
            MinLru = Slot->LruCount;
            LruIndex = SlotIndex;
        }
    }
    
    // Choose empty slot if available, otherwise evict
    *SlotIndexOut = (EmptyIndex >= 0) ? (u32)EmptyIndex : LruIndex;
    return false;
}

glyph_info *
glyph_cache_get(glyph_cache *Cache, glyph_key_data KeyData) {
    u64 Key = make_glyph_key(KeyData);
    u32 SlotIndex; 
    b8 Match = glyph_cache_get_index(Cache, Key, &SlotIndex);
    glyph_info *Slot = &Cache->Slots[SlotIndex];
    if(!Match) {
        // Slot does not contain the glyph we're looking for.
        // Make the requested glyph and store write it to Slot.
        u32 LruCount = Slot->LruCount + 1;
        zero_struct(Slot);
        Slot->LruCount = LruCount;
        Slot->KeyData = KeyData;
        Slot->Key = Key;
        
        stbtt_GetCodepointBitmapBox(&Cache->FontInfo, KeyData.Codepoint, KeyData.Scale, KeyData.Scale, &Slot->x0, &Slot->y0, &Slot->x1, &Slot->y1);
        stbtt_GetCodepointHMetrics(&Cache->FontInfo, KeyData.Codepoint, &Slot->Advance, &Slot->LeftSideBearing);
        
        // TODO: eeeh  this is fucked if Width or height exceeds Cache->CellWidth Cache->CellHeight....
        s32 Width = Slot->x1 - Slot->x0;
        s32 Height = Slot->y1 - Slot->y0;
        
        ASSERT(Width >= 0 && (u32)Width <= Cache->CellWidth);
        ASSERT(Height >= 0 && (u32)Height <= Cache->CellHeight);
        
        u32 CellRow = SlotIndex / Cache->Columns;
        u32 CellColumn = SlotIndex % Cache->Columns;
        
        u32 TextureStride = Cache->CellWidth * Cache->Columns;
        Slot->TextureOffset = TextureStride * Cache->CellHeight * CellRow + Cache->CellWidth * CellColumn;
        u8 *TextureDestination = Cache->Texture + Slot->TextureOffset;
        
        // TODO: Fonts
        // stbtt_fontinfo *Fi = &gCache->FontInfoArray[KeyData->FontId];
        stbtt_fontinfo *Fi = &Cache->FontInfo;
        stbtt_MakeCodepointBitmap(Fi, TextureDestination, Width, Height, TextureStride, Slot->Scale, Slot->Scale, Slot->Codepoint);
    }
    
    return Slot;
}

glyph_cache
glyph_cache_make(view<u8> FontFilePath, f32 MaxFontPixelHeight) {
    glyph_cache Cache = {};
    Cache.MaxFontPixelHeight = MaxFontPixelHeight;
    
    Cache.FontFile = gPlatform.read_file(FontFilePath);
    
    s32 FontOffset = stbtt_GetFontOffsetForIndex(Cache.FontFile.Ptr, 0);
    if(FontOffset == -1) {
        // TODO: Handle
        ASSERT(false);
    }
    
    if(stbtt_InitFont(&Cache.FontInfo, Cache.FontFile.Ptr, FontOffset) == 0) {
        // TODO: Handle
        ASSERT(false);
    }
    
    s32 x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&Cache.FontInfo, &x0, &y0, &x1, &y1);
    Cache.UnscaledDescent = y1;
    f32 Scale = stbtt_ScaleForPixelHeight(&Cache.FontInfo, MaxFontPixelHeight);
    Cache.CellWidth = (u32)((x1 - x0) * Scale + 0.5f);
    Cache.CellHeight = (u32)((y1 - y0) * Scale + 0.5f);
    
    Cache.Rows = 24;
    Cache.Columns = 24;
    u32 SlotCount = Cache.Rows * Cache.Columns;
    Cache.Slots = (glyph_info *)gPlatform.alloc(SlotCount * sizeof(glyph_info));
    Cache.Texture = (u8 *)gPlatform.alloc(Cache.CellWidth * Cache.CellHeight * SlotCount);
    
    return Cache;
}