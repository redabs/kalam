#ifndef KALAM_H
#define KALAM_H

#define TAB_WIDTH 4 

typedef struct panel_t panel_t;

typedef struct {
    u64 Offset;
    u64 Size; // size in bytes of entire line including newline character
    u64 Length; // characters, does not count newline character
    u8 NewlineSize; // The size of the terminator, e.g. 1 for \n, 2 for \r\n and 0 when line is not newline terminated (it's the last line)
} line_t;

typedef struct {
    u64 Anchor;
    u64 Cursor;
    u64 ColumnWas; // 0 based
    u64 ColumnIs; // 0 based
    u64 Idx; // Unique over selections. The selection with the greatest Idx is the one that was created last.
} selection_t;

typedef enum {
    BUFFER_TYPE_Internal, // scratch, messages, debug log and other non file-backed buffers.
    BUFFER_TYPE_File,
} buffer_type_t;

typedef struct selection_group_t selection_group_t;
struct selection_group_t {
    panel_t *Owner;
    selection_t *Selections; // stb
    u64 SelectionIdxTop;
};

typedef struct {
    // TODO: FileInformation
    buffer_type_t Type;
    mem_buffer_t Text;
    line_t *Lines; // stb
    selection_group_t *SelectionGroups; // stb
} buffer_t;

typedef enum {
    SPLIT_Vertical = 0,
    SPLIT_Horizontal
} split_mode_t;

typedef enum {
    MODE_Normal = 0,
    MODE_Insert,
    MODE_Select,
} mode_t;

typedef struct {
    mem_buffer_t SearchTerm;
    selection_group_t SelectionGroup; // Used as the working set when panel is in MODE_Select
} mode_select_ctx_t;

// Only the leaf nodes are actually regions where text is drawn.
struct panel_t {
    panel_t *Next; // Next free panel, or next leaf node
    
    panel_t *Parent;
    split_mode_t Split; 
    buffer_t *Buffer;
    
    b32 IsLeaf;
    
    // Used when leaf node
    mode_t Mode;
    union {
        mode_select_ctx_t Select;
    } ModeCtx;
    
    s32 ScrollX, ScrollY;
    
    // Used when non-leaf node
    panel_t *Children[2]; 
    u8 LastSelected;
};

#define PANEL_MAX 16
typedef struct {
    panel_t *Root; // In the binary tree of panels
    panel_t *Selected; // The currently selected leaf node that the user has selected
    panel_t Panels[PANEL_MAX]; // Storage of all panels
    panel_t *FreeList;
} panel_ctx_t; 

typedef struct {
    u8 *Data;
    s32 w;
    s32 h;
    s32 Stride;
} bitmap_t;

typedef struct {
    bitmap_t Bitmap;
    stbtt_bakedchar Glyphs[256];
} glyph_set_t;

#define GLYPH_SET_MAX 32

typedef struct font_t {
    stbtt_fontinfo StbInfo;
    platform_file_data_t File;
    
    f32 Size; 
    
    s32 Ascent;
    s32 Descent;
    s32 LineGap;
    s32 LineHeight;
    
    s32 MHeight; // The height in pixels of 'M'
    s32 MWidth; // The width in pixels of 'M'. This is used under the presumption of a monospace font.
    s32 SpaceWidth;
    
    u32 SetCount;
    struct {
        u32 FirstCodepoint;
        // Set contains glyphs in the range [FirstCodepoint, FirstCodePoint + 255]
        glyph_set_t *Set;
    } GlyphSets[GLYPH_SET_MAX];
} font_t;

typedef struct {
    u64 Offset;
    u64 Size;
    file_flags_t Flags;
} file_name_info_t;

typedef struct {
    directory_t CurrentDirectory;
    
    font_t Font;
    buffer_t *Buffers; // stb, TODO: Stbs forces us to update live pointer whenever there's a reallocation, maybe store these in batches instead?
    
    panel_ctx_t PanelCtx;
    ui_ctx_t UiCtx;
} ctx_t;

u64 offset_to_line_index(buffer_t *Buf, u64 Offset);

#endif //KALAM_H