#ifndef KALAM_H
#define KALAM_H

#include "deps/stb_truetype.h"

#include "k_intrinsics.h"
#include "k_math.h"
#include "k_input.h"
#include "k_memory.h"
#include "k_platform.h"
#include "k_utf8.h"
#include "k_ui.h"

enum direction {
    DIRECTION_Up = 0,
    DIRECTION_Down,
    DIRECTION_Left,
    DIRECTION_Right,
};

// glyph_key_data is data that identifies a glyph but is also general info about that glyph used for
// layouting. This is a hack to not have to access those members behind another struct member of 
// glyph_info.
#define GLYPH_KEY_DATA \
struct { \
u32 Codepoint; \
f32 Scale; \
u8 FontId; \
}

typedef GLYPH_KEY_DATA glyph_key_data;
struct glyph_info {
    u32 LruCount;
    union {
        GLYPH_KEY_DATA;
        glyph_key_data KeyData;
    };
    u64 Key;

    u64 TextureOffset;

    s32 Advance;
    s32 LeftSideBearing;
    s32 x0, y0, x1, y1;
};

struct glyph_cache {
    u32 CellWidth;
    u32 CellHeight;
    u32 Rows;
    u32 Columns;
    f32 MaxFontPixelHeight;
    s32 UnscaledDescent;

    stbtt_fontinfo FontInfo;
    buffer<u8> FontFile;

    u8 *Texture; // Width = CellWidth * Columns, Height = CellHeight * Rows
    glyph_info *Slots; // Count = Rows * Columns
};

enum line_ending_type {
    LINE_ENDING_None = 0,
    LINE_ENDING_Lf   = 0x0A,   // 0x0A \n
    LINE_ENDING_CrLf = 0x0A0D, // 0x0D 0x0A \r\n
    LINE_ENDING_Cr   = 0x0D,   // 0x0D \r
    LINE_ENDING_Rs   = 0x1E,   // 0x1E \036
    LINE_ENDING_Nl   = 0x15,   // 0x15 \025
};

inline u8
line_ending_size(line_ending_type Type) {
    switch(Type) {
        case LINE_ENDING_None: return 0;

        case LINE_ENDING_Lf:
        case LINE_ENDING_Cr:
        case LINE_ENDING_Rs:
        case LINE_ENDING_Nl: return 1;

        case LINE_ENDING_CrLf: return 2;

        default: {
            ASSERT(false);
            return 1;
        } break;
    }
}

struct line {
    u64 Offset;
    u64 Size; // Size in bytes of entire line, not including line ending
    u64 Length; // Character count, not including line ending
    line_ending_type LineEnding;
    // 1    This is a wrapp| Wrapped = false
    // 2(1) ed line.       | Wrapped = true
    b8 Wrapped;
};

struct selection {
    u64 Cursor;
    u64 Anchor;
    u64 Column;
    u8 Panel; // Owner of the selection
};

enum edit_mode {
    EDIT_MODE_Command,
    EDIT_MODE_Insert,
    EDIT_MODE_Select, // Reduces selection to search term
    EDIT_MODE_SelectInner,
};

inline view<u8>
edit_mode_string(edit_mode Mode) {
    switch(Mode) {
        case EDIT_MODE_Command: { return C_STR_VIEW("COMMAND"); } 
        case EDIT_MODE_Insert: { return C_STR_VIEW("INSERT"); }
        case EDIT_MODE_Select: { return C_STR_VIEW("SELECT"); }
        case EDIT_MODE_SelectInner: { return C_STR_VIEW("SelectInner"); }

    }
    ASSERT(false);
    return C_STR_VIEW("");
}

struct file_buffer {
    buffer<u8> Text;
    buffer<line> Lines;
    buffer<u8> Path;
    edit_mode Mode;
    buffer<selection> Selections;

    // Used in EDIT_MODE_Select
    buffer<selection> SelectSelections; // Selections used in select mode
    buffer<u8> SelectTerm;
};

struct kalam_ctx {
    iv2 Scroll;
    u64 BufferIdx;

    buffer<file_buffer> Buffers;

    glyph_cache GlyphCache;
};

void kalam_init();
void kalam_update_and_render(input_state *Input, framebuffer *Fb, f32 Dt);

#endif
