#ifndef UI_H
#define UI_H

#define UI_CONTAINER_MAX 8
#define UI_LAYOUT_MAX 32
#define UI_COMMAND_BUFFER_SIZE 1024 * 512
#define UI_NOODLE_SIZE 64
#define UI_NOODLE_MAX 512 

typedef u64 ui_id_t;
typedef struct font_t font_t;

typedef enum {
    UI_INTERACTION_None = 0,
    UI_INTERACTION_Press, // Widget was hot then became active.
    UI_INTERACTION_PressAndRelease, // Widget was active and is not longer.
    UI_INTERACTION_Hover, // Widget is hot
} ui_interaction_type_t;

typedef enum {
    UI_CNT_Open  = (1 << 0),
    UI_CNT_PopUp = (1 << 1),
} ui_container_flags_t;

typedef enum {
    UI_CMD_Clip,
    UI_CMD_Rect,
    UI_CMD_Text,
} ui_cmd_type_t;

typedef struct ui_cmd_clip_t {
    irect_t Rect;
    struct ui_cmd_clip_t *Prev;
} ui_cmd_clip_t;

typedef struct {
    irect_t Rect;
    color_t Color;
} ui_cmd_rect_t;

typedef struct {
    u64 Size;
    iv2_t Baseline; 
    color_t Color;
    u8 *Text;
} ui_cmd_text_t;

typedef struct {
    u64 Size;
    ui_cmd_type_t Type;
    union {
        ui_cmd_clip_t Clip;
        ui_cmd_rect_t Rect;
        ui_cmd_text_t Text;
    };
} ui_cmd_t;

typedef struct ui_noodle_t {
    union {
        u8 Data[UI_NOODLE_SIZE];
        struct ui_noodle_t *Next;
    };
} ui_noodle_t;

typedef struct {
    irect_t Rect;
    irect_t ContentSize;
    iv2_t Cursor;
    iv2_t Indent;
    s32 RowHeight;
    s32 ItemsPerRow;
    s32 ItemCount;
} ui_layout_t;

typedef struct {
    ui_id_t Id;
    irect_t Rect;
} ui_control_t;

typedef struct ui_container_t {
    u64 FrameOpened;
    ui_id_t Id;
    ui_container_flags_t Flags;
    
    irect_t Rect;
    iv2_t Scroll;
    
    u32 ControlSelected;
    MEM_STACK(ui_control_t, 128) ControlStack;
    
    struct ui_container_t *Prev;
} ui_container_t;

typedef struct {
    struct font_t *Font;
    u64 Frame;
    
    ui_id_t Hot;
    ui_id_t Active; 
    ui_id_t Selected;
    ui_id_t Focus;
    ui_id_t TextEditFocus;
    
    input_state_t *Input;
    
    ui_cmd_clip_t *LastClip;
    
    ui_container_t *ActiveContainer; 
    u32 OpenContainerCount;
    ui_container_t Containers[UI_CONTAINER_MAX];
    ui_container_t *ContainerStack[UI_CONTAINER_MAX]; // Sorted by z-level, first is closest to your face
    
    struct {
        table_t Table;
        ui_noodle_t *Free;
        ui_noodle_t Noodles[UI_NOODLE_MAX];
    } Pool;
    
    MEM_STACK(ui_layout_t, UI_LAYOUT_MAX) LayoutStack;
    
    struct { u64 Used; u8 Data[UI_COMMAND_BUFFER_SIZE]; } CmdBuf;
} ui_ctx_t;

#endif //UI_H