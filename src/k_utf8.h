#ifndef K_UTF8_H
#define K_UTF8_H

// Returns the width of the converted character.
inline u8
utf8_to_codepoint(u8 *Utf8, u32 *Codepoint) {
    u32 Cp;
    u8 n;
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
    
    u8 CharWidth = n + 1;
    while(n--) {
        Cp = Cp << 6 | (*(++Utf8) & 0x3f);
    }
    *Codepoint = Cp;
    return CharWidth;
}

inline u8
utf8_char_size(u8 FirstCharByte) {
    switch(FirstCharByte & 0xf0) {
        case 0xf0: { return 4; }
        case 0xe0: { return 3; }
        case 0xd0:
        case 0xc0: { return 2; }
        default:   { return 1; }
    }
}

// Step Text back one utf-8 character and dont walk more than Limt number of bytes
inline u8
utf8_step_back_one(u8 *Text, u64 Limit) {
    Limit = MIN(4, Limit);
    u8 Count = 0;
    for(; Count < Limit; ++Count) {
        ++Count;
        if((*(Text - Count) & 0xc0) != 0x80) { 
            break;
        }
    }
    return Count;
}

inline u64
utf8_char_count(view<u8> Text) {
    u64 Result = 0;
    for(u8 CharSize = 0; Result < Text.Count; Result += CharSize) {
        CharSize = utf8_char_size(Text.Ptr[Result]);
    }
    return Result;
}

inline b8
utf8_char_equals(u8 *Char0, u8 *Char1) {
    u8 Size0 = utf8_char_size(*Char0);
    u8 Size1 = utf8_char_size(*Char1);
    
    if(Size0 != Size1) return false;
    
    b8 Equals = true;
    switch(Size0) {
        case 4: Equals &= (Char0[3] == Char1[3]);
        case 3: Equals &= (Char0[2] == Char1[2]);
        case 2: Equals &= (Char0[1] == Char1[1]);
        case 1: Equals &= (Char0[0] == Char1[0]); break;
        
        default: ASSERT(false);
    }
    return Equals;
}

inline b8
utf8_find_in_text(view<u8> Text, view<u8> SearchTerm, u64 *ByteLocationOut) {
    if(Text.Count < SearchTerm.Count) {
        return false;
    }
    
    for(u64 i = 0; i < Text.Count && (Text.Count - i) >= SearchTerm.Count; i += utf8_char_size(Text.Ptr[i])) {
        if(utf8_char_equals(Text.Ptr + i, SearchTerm.Ptr)) {
            // Found first occurence
            // compare rest of the word
            u64 j = 0;
            for(; j < MIN(Text.Count - i, SearchTerm.Count); j += utf8_char_size(*SearchTerm.Ptr)) {
                if(!utf8_char_equals(Text.Ptr + i + j, SearchTerm.Ptr + j)) {
                    break;
                }
            }
            
            if(j == SearchTerm.Count) {
                // Found the search term
                *ByteLocationOut = i;
                return true;
            }
        }
    }
    return false;
}

#endif
