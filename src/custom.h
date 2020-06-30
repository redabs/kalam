#ifndef CUSTOM_H
#define CUSTOM_H

#define COLOR_BG 0xff162447
#define COLOR_SELECTION 0xff5555aa
#define COLOR_LINE_HIGHLIGHT 0xff1f4068
#define COLOR_TEXT 0xff90b080

#define COLOR_LINE_NUMBER 0xff505050
#define COLOR_LINE_NUMBER_CURRENT 0xfff0f050

#define COLOR_STATUS_BAR 0xff1b1b2f
#define COLOR_STATUS_NORMAL 0xffa8df65
#define COLOR_STATUS_INSERT 0xffe43f5a

#define COLOR_PANEL_SELECTED 0xffdddddd
#define COLOR_PANEL_LAST_SELECTED 0xff888888

#define STATUS_BAR_HEIGHT 20
#define BORDER_SIZE 2
#define LINE_NUMBER_PADDING_RIGHT 5

#define IGNORE_CAPS_NUMLOCK 0

typedef enum {
    // Normal
    OP_EnterInsertMode,
    OP_MoveCursorWithSelection,
    OP_DeleteSelection, 
    OP_MoveAndDropCursor, 
    
    // Insert
    OP_EscapeToNormal,
    OP_Delete,
    OP_DoChar,
    
    // Global
    OP_Home,
    OP_End,
    OP_MoveCursor,
    OP_ToggleSplitMode,
    OP_NewPanel,
    OP_KillPanel,
    OP_MovePanelSelection,
} operation_type_t;

typedef struct {
    operation_type_t Type;
    union {
        struct {
            dir_t Dir;
            s32 StepSize;
        } MoveCursor;
        
        struct {
            dir_t Dir;
        } MovePanelSelection;
        
        struct {
            dir_t Dir;
        } MoveCursorWithSelection;
        
        struct {
            u8 Character[4];
        } DoChar;
        
        struct {
            dir_t Dir;
        } MoveAndDropCursor;
    };
} operation_t;

typedef struct {
    b8 IsKey;
    union {
        u8 Character[4]; // IsKey = false
        input_key_t Key; // IsKey = true
    };
    input_modifier_t Modifiers; // == 0 means we ignore Caps and NumLock
    
    operation_t Operation;
} key_mapping_t;

key_mapping_t NormalMappings[] = {
    { .IsKey = false, .Character[0] = 0xc3, .Character[1] = 0xa4, .Operation.Type = OP_EnterInsertMode, },
    
    { .IsKey = false, .Character[0] = 'd', .Operation.Type = OP_DeleteSelection, },
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_MoveCursorWithSelection, .Operation.MoveCursorWithSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_MoveCursorWithSelection, .Operation.MoveCursorWithSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_MoveCursorWithSelection, .Operation.MoveCursorWithSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_MoveCursorWithSelection, .Operation.MoveCursorWithSelection.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MoveAndDropCursor, .Operation.MoveAndDropCursor.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MoveAndDropCursor, .Operation.MoveAndDropCursor.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MoveAndDropCursor, .Operation.MoveAndDropCursor.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MoveAndDropCursor, .Operation.MoveAndDropCursor.Dir = DOWN},
    
};

key_mapping_t InsertMappings[] = {
    { .IsKey = true, .Key = KEY_Escape, .Operation.Type = OP_EscapeToNormal, .Modifiers = IGNORE_CAPS_NUMLOCK, },
    { .IsKey = true, .Key = KEY_Delete,  .Operation.Type = OP_Delete,.Modifiers = IGNORE_CAPS_NUMLOCK, },
    
};

key_mapping_t GlobalMappings[] = {
    { .IsKey = true, .Key = KEY_Home, .Operation.Type = OP_Home, },
    { .IsKey = true, .Key = KEY_End,  .Operation.Type = OP_End, },
    
    { .IsKey = true, .Key = KEY_Left,  .Operation.Type = OP_MoveCursor, .Operation.MoveCursor.Dir = LEFT,  .Operation.MoveCursor.StepSize = 1, }, 
    { .IsKey = true, .Key = KEY_Right, .Operation.Type = OP_MoveCursor, .Operation.MoveCursor.Dir = RIGHT, .Operation.MoveCursor.StepSize = 1, }, 
    { .IsKey = true, .Key = KEY_Up,    .Operation.Type = OP_MoveCursor, .Operation.MoveCursor.Dir = UP,    .Operation.MoveCursor.StepSize = 1, }, 
    { .IsKey = true, .Key = KEY_Down,  .Operation.Type = OP_MoveCursor, .Operation.MoveCursor.Dir = DOWN,  .Operation.MoveCursor.StepSize = 1, }, 
    
    { .IsKey = true, .Key = KEY_X, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_ToggleSplitMode, },
    { .IsKey = true, .Key = KEY_N, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_NewPanel,},
    { .IsKey = true, .Key = KEY_W, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_KillPanel, },
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = DOWN},
    
};

#endif //CUSTOM_H
