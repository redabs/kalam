#ifndef UI_H
#define UI_H

#define UI_CONTAINER_MAX 8
#define UI_COMMAND_BUFFER_SIZE 1024 * 32

typedef u64 ui_id_t;

typedef enum {
    UI_INTERACTION_None = 0,
    UI_INTERACTION_Press, // Widget was hot then became active.
    UI_INTERACTION_PressAndRelease, // Widget was active and is not longer.
    UI_INTERACTION_Drag, // Widget was active atleast one frame back and still is.
    UI_INTERACTION_Hover, // Widget is hot
} ui_interaction_type_t;

typedef enum {
    UI_DRAW_CMD_PushClip,
    UI_DRAW_CMD_PopClip,
    UI_DRAW_CMD_Rect,
    UI_DRAW_CMD_Text,
} ui_draw_cmd_type_t;

typedef struct {
    irect_t Rect;
} ui_draw_cmd_push_clip_t;

typedef struct {
    irect_t Rect;
    color_t Color;
} ui_draw_cmd_irect_t;

typedef struct {
    iv2_t Baseline; 
    color_t Color;
    u8 Size; // In bytes
    u8 Text[1];
} ui_draw_cmd_text_t;

typedef struct {
    u32 Size;
    ui_draw_cmd_type_t Type;
    union {
        ui_draw_cmd_push_clip_t PushClip;
        ui_draw_cmd_irect_t Rect;
        ui_draw_cmd_text_t Text;
    };
} ui_draw_cmd_t;

typedef struct {
    b8 Open;
    b8 IsPopUp;
    
    ui_id_t Id;
    irect_t Rect;
    
    u64 FrameOpened;
    
    s32 ScrollX;
    s32 ScrollY;
    s32 ContentWidth;
    s32 ContentHeight;
} ui_container_t;

typedef struct {
    ui_id_t Active; 
    ui_id_t Hot;
    
    u64 Frame;
    
    // Dragging requires a mouse position delta, or the previous mouse position.
    iv2_t MousePos;
    iv2_t MousePosPrev;
    
    // Events is set by kalam during initialization to point to the buffer that is filled by the
    // platform layer.
    input_event_buffer_t *Events;
    
    // The container currently being modified between calls to ui_begin_container and ui_end_container
    ui_container_t *ActiveContainer; 
    
    
    // OpenContainerCount records how used slots Containers has. Containers is an array with holes;
    // linear search to find an available slot.
    s32 OpenContainerCount;
    ui_container_t Containers[UI_CONTAINER_MAX];
    
    // The size of draw commands is recorded in the draw command itself.
    struct {
        ui_draw_cmd_t *Top;
        u8 Data[UI_COMMAND_BUFFER_SIZE];
    } DrawCommandStack;
} ui_ctx_t;

#endif //UI_H
