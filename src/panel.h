#ifndef PANEL_H
#define PANEL_H

typedef enum {
    BUFFER_TYPE_Internal, // scratch, messages, debug log and other non file-backed buffers.
    BUFFER_TYPE_File,
} buffer_type_t;

typedef struct {
    buffer_type_t Type;
    union {
        u8 *Path;
        u8 *Name; // When Type is BUFFER_TYPE_Internal
    }; 
    mem_buffer_t Text;
    line_t *Lines; // stb
} buffer_t;

typedef enum {
    SPLIT_Vertical = 0,
    SPLIT_Horizontal
} split_mode_t;

typedef enum {
    MODE_Normal = 0,
    MODE_Insert
} mode_t;

// Only the leaf nodes are actually regions where text is drawn.
typedef struct panel_t panel_t;
struct panel_t {
    panel_t *Parent;
    split_mode_t Split; 
    
    // Used when leaf node
    buffer_t *Buffer; // == 0 when not a leaf node (i.e. it doesn't hold a buffer to for editing)
    mode_t Mode;
    selection_t *Selections; // stb
    u64 SelectionIdxTop;
    
    s32 ScrollX, ScrollY;
    
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
} panel_ctx_t; 

#endif //PANEL_H
