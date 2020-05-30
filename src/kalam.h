#ifndef KALAM_H
#define KALAM_H

#define TAB_WIDTH 4 

typedef struct {
    s64 Offset;
    s64 Size; // bytes
    s64 Length; // characters
} line_t;

typedef struct {
    //u8 *Data; // stb
    mem_buffer_t Text;
    line_t *Lines; // stb
    struct {
        s64 Line; // 0 based
        s64 ColumnIs; // 0 based, see README.md on defining cursor movement behavior
        s64 ColumnWas;
        s64 Offset;
    } Cursor;
} buffer_t;

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

typedef struct {
    stbtt_fontinfo StbInfo;
    platform_file_data_t File;
    
    f32 Size; 
    
    s32 Ascent;
    s32 Descent;
    s32 LineGap;
    
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

typedef enum {
    SPLIT_Vertical = 0,
    SPLIT_Horizontal
} split_mode;

// Only the leaf nodes are actually regions where text is drawn.
typedef struct panel_t panel_t;
struct panel_t {
    panel_t *Parent;
    split_mode Split; 
    
    // Used when leaf node
    buffer_t *Buffer; // == 0 when not a leaf node (i.e. it doesn't hold a buffer to for editing)
    
    // Used when non-leaf node
    panel_t *Children[2]; 
    u8 LastSelected;
};

typedef struct panel_free_node_t panel_free_node_t;
struct panel_free_node_t {
    panel_t Panel;
    panel_free_node_t *Next;
};

#define PANEL_MAX 16
typedef struct {
    panel_t *Root;
    panel_t *Selected;
    panel_free_node_t Panels[PANEL_MAX];
    panel_free_node_t *FreeList;
} panel_ctx; 

typedef enum {
    MODE_Normal = 0,
    MODE_Insert
} mode;

typedef struct {
    font_t Font;
    iv2_t p;
    buffer_t Buffer;
    panel_ctx PanelCtx;
    mode Mode;
} ctx_t;

#endif //KALAM_H
