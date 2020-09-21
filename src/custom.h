#ifndef CUSTOM_H
#define CUSTOM_H

#define COLOR_BG 0xff161616
#define COLOR_SELECTION 0xff5555aa
#define COLOR_LINE_HIGHLIGHT 0xff101010
#define COLOR_TEXT 0xffd1c87e
#define COLOR_CURSOR 0xff8888aa
#define COLOR_MAIN_CURSOR 0xffccccff

#define COLOR_LINE_NUMBER 0xff505050
#define COLOR_LINE_NUMBER_CURRENT 0xfff0f050

#define COLOR_STATUS_BAR 0xff262626
#define COLOR_STATUS_NORMAL 0xffa8df65
#define COLOR_STATUS_INSERT 0xffe43f5a

#define COLOR_PANEL_SELECTED 0xffdddddd
#define COLOR_PANEL_LAST_SELECTED 0xff888888

#define STATUS_BAR_HEIGHT 20
#define BORDER_SIZE 2
#define LINE_NUMBER_PADDING_RIGHT 5

typedef enum {
    
    OP_SetMode,
    
    // TODO: These operations should not be mode specific. I.e. there should be no OP_Normal_Home, only OP_Normal with key mappings in the individual modes that implement the mode specific behavior.
    // Normal
    OP_Normal_Home,
    OP_Normal_End,
    OP_EnterInsertMode,
    OP_DeleteSelection, 
    OP_ExtendSelection, 
    OP_DropSelectionAndMove,
    OP_ClearSelections,
    
    // Insert
    OP_Insert_Home,
    OP_Insert_End,
    OP_EscapeToNormal,
    OP_Delete,
    OP_DoChar,
    
    // Global
    OP_ToggleSplitMode,
    OP_NewPanel,
    OP_KillPanel,
    OP_MovePanelSelection,
    OP_MoveSelection,
} operation_type_t;

typedef struct {
    operation_type_t Type;
    union {
        struct {
            dir_t Dir;
        } ExtendSelection;
        
        struct {
            dir_t Dir;
        } MoveSelection;
        
        struct {
            dir_t Dir;
        } MovePanelSelection;
        
        struct {
            u8 Character[4];
        } DoChar;
        
        struct {
            dir_t Dir;
        } DropSelectionAndMove;
        
        struct {
            mode_t Mode;
        } SetMode;
    };
} operation_t;

typedef struct {
    b8 IsKey;
    union {
        u8 Character[4]; // IsKey = false
        input_key_t Key; // IsKey = true
    };
    input_modifier_t Modifiers; // == 0 means we ignore Caps and NumLock
    
    // TODO: This can be an array so we can have smaller operations chained together
    operation_t Operation;
} key_mapping_t;

key_mapping_t NormalMappings[] = {
    { .IsKey = true, .Key = KEY_Home, .Operation.Type = OP_Normal_Home, },
    { .IsKey = true, .Key = KEY_End,  .Operation.Type = OP_Normal_End, },
    { .IsKey = false, .Character[0] = 'i', .Operation.Type = OP_EnterInsertMode, },
    { .IsKey = false, .Character[0] = 'd', .Operation.Type = OP_DeleteSelection, },
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_Space, .Operation.Type = OP_ClearSelections, },
    { .IsKey = false, .Character[0] = 's', .Operation.Type = OP_SetMode, .Operation.SetMode.Mode = MODE_Select },
};

key_mapping_t InsertMappings[] = {
    { .IsKey = true, .Key = KEY_Home, .Operation.Type = OP_Insert_Home, },
    { .IsKey = true, .Key = KEY_End, .Operation.Type = OP_Insert_End, },
    { .IsKey = true, .Key = KEY_Escape, .Operation.Type = OP_EscapeToNormal},
    { .IsKey = true, .Key = KEY_Delete, .Operation.Type = OP_Delete},
    
};

key_mapping_t SelectMappings[] = {
    { .IsKey = true, .Key = KEY_Escape, .Operation.Type = OP_SetMode, .Operation.SetMode.Mode = MODE_Normal },
    
};

key_mapping_t GlobalMappings[] = {
    { .IsKey = true, .Key = KEY_X, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_ToggleSplitMode, },
    { .IsKey = true, .Key = KEY_N, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_NewPanel,},
    { .IsKey = true, .Key = KEY_W, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_KillPanel, },
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_Left,  .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = DOWN},
    
};

#endif //CUSTOM_H
