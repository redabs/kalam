#ifndef UI_H
#define UI_H

#define UI_CONTAINER_MAX 8
#define UI_LAYOUT_MAX 32
#define UI_CONTROL_MAX 2048
#define UI_COMMAND_BUFFER_SIZE 1024 * 512
#define UI_NODE_MAX 2048

enum ui_interaction_type {
    UI_INTERACTION_None = 0,
    UI_INTERACTION_Press, // Widget was hot then became active.
    UI_INTERACTION_PressAndRelease, // Widget was active and is not longer.
    UI_INTERACTION_Hover, // Widget is hot
};


BIT_FLAG_ENUM(ui_container_flags, u8) {
    UI_CNT_Open  = (1 << 0),
    UI_CNT_PopUp = (1 << 1),
};

enum ui_cmd_type {
    UI_CMD_Clip = 0,
    UI_CMD_Rect,
    UI_CMD_Text,
};

struct ui_cmd_clip { irect Rect; ui_cmd_clip *Prev; }; // Replace Prev with last rect?
struct ui_cmd_rect { irect Rect; color Color; };
struct ui_cmd_text { u64 Size; iv2 Baseline; color Color; u8 *Text; };
struct ui_cmd {
    u64 Size;
    ui_cmd_type Type;
    union {
        ui_cmd_clip Clip;
        ui_cmd_rect Rect;
        ui_cmd_text Text;
    };
};

struct ui_layout {
    irect Rect;
    irect ContentSize;
    iv2 Cursor;
    iv2 Indent;
    s32 RowHeight;
    s32 ItemsPerRow;
    s32 ItemCount;
};

struct ui_control {
    u64 Id;
    irect Rect;
};

struct ui_container {
    u64 FrameOpened;
    u64 Id;
    ui_container_flags Flags;

    irect Rect;
    iv2 Scroll;

    u32 ControlSelected;
    u32 ControlCount;
    ui_control Controls[UI_CONTROL_MAX];

    ui_container *Prev;
};

struct ui_ctx {
    u64 Frame;

    u64 Hot;
    u64 Active;
    u64 Selected; // TODO: Remove this and all usage of it if we're not going to do keyboard navigation, which we probably won't
    u64 Focus;
    u64 TextEditFocus;

    input_state *Input;

    ui_cmd_clip *LastClip;

    ui_container *ActiveContainer; 
    u32 OpenContainerCount;
    ui_container Containers[UI_CONTAINER_MAX];
    ui_container *ContainerStack[UI_CONTAINER_MAX]; // Sorted by z-level, first is closest to your face

    struct { u64 Id; u64 LastUpdated; b8 Open; } TreeNodes[UI_NODE_MAX];

    u32 LayoutCount;
    ui_layout Layouts[UI_LAYOUT_MAX];

    u64 CmdUsed;
    u8 CmdBuf[UI_COMMAND_BUFFER_SIZE];
};

#endif
