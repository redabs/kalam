#ifndef CUSTOM_H
#define CUSTOM_H

color_t COLOR_BG = {.Argb = 0xff161616};
color_t COLOR_SELECTION = {.Argb = 0xff5555aa};
color_t COLOR_LINE_HIGHLIGHT = {.Argb = 0xff101010};
color_t COLOR_TEXT = {.Argb = 0xffd1c87e};
color_t COLOR_CURSOR = {.Argb = 0xff8888aa};
color_t COLOR_MAIN_CURSOR = {.Argb = 0xffccccff};

color_t COLOR_LINE_NUMBER = {.Argb = 0xff505050};
color_t COLOR_LINE_NUMBER_CURRENT = {.Argb = 0xfff0f050};

color_t COLOR_STATUS_BAR = {.Argb = 0xff262626};
color_t COLOR_STATUS_NORMAL = {.Argb = 0xffa8df65};
color_t COLOR_STATUS_INSERT = {.Argb = 0xffe43f5a};

color_t COLOR_PANEL_SELECTED = {.Argb = 0xffdddddd};
color_t COLOR_PANEL_LAST_SELECTED = {.Argb = 0xff888888};

#define STATUS_BAR_HEIGHT 20
#define BORDER_SIZE 2
#define LINE_NUMBER_PADDING_RIGHT 5

typedef enum {
    
    OP_SetMode,
    
    // TODO: These operations should not be mode specific. I.e. there should be no OP_Normal_Home, only OP_Home with key mappings in the individual modes that implement the mode specific behavior.
    // Normal
    OP_Normal_Home,
    OP_Normal_End,
    OP_DeleteSelection, 
    OP_ExtendSelection, 
    OP_DropSelectionAndMove,
    OP_ClearSelections,
    OP_OpenFileSelection, 
    OP_SelectEntireBuffer,
    
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
            direction_t Dir;
        } ExtendSelection;
        
        struct {
            direction_t Dir;
        } MoveSelection;
        
        struct {
            direction_t Dir;
        } MovePanelSelection;
        
        struct {
            u8 Character[4];
        } DoChar;
        
        struct {
            direction_t Dir;
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
    { .IsKey = false, .Character[0] = 'i', .Operation.Type = OP_SetMode, .Operation.SetMode.Mode = MODE_Insert},
    { .IsKey = false, .Character[0] = 's', .Operation.Type = OP_SetMode, .Operation.SetMode.Mode = MODE_Select },
    { .IsKey = false, .Character[0] = 'f', .Operation.Type = OP_OpenFileSelection },
    { .IsKey = false, .Character[0] = 'd', .Operation.Type = OP_DeleteSelection, },
    { .IsKey = false, .Character[0] = '%', .Operation.Type = OP_SelectEntireBuffer, },
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Shift, .Operation.Type = OP_ExtendSelection, .Operation.ExtendSelection.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Alt, .Operation.Type = OP_DropSelectionAndMove, .Operation.DropSelectionAndMove.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_Left,  .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_X, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_ToggleSplitMode, },
    { .IsKey = true, .Key = KEY_N, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_NewPanel,},
    { .IsKey = true, .Key = KEY_W, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_KillPanel, },
    
    { .IsKey = true, .Key = KEY_Left,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Modifiers = INPUT_MOD_Ctrl, .Operation.Type = OP_MovePanelSelection, .Operation.MovePanelSelection.Dir = DOWN},
    
    { .IsKey = true, .Key = KEY_Space, .Operation.Type = OP_ClearSelections, },
};

key_mapping_t InsertMappings[] = {
    { .IsKey = true, .Key = KEY_Home, .Operation.Type = OP_Insert_Home, },
    { .IsKey = true, .Key = KEY_End, .Operation.Type = OP_Insert_End, },
    { .IsKey = true, .Key = KEY_Escape, .Operation.Type = OP_EscapeToNormal},
    { .IsKey = true, .Key = KEY_Delete, .Operation.Type = OP_Delete},
    
    { .IsKey = true, .Key = KEY_Left,  .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = LEFT},
    { .IsKey = true, .Key = KEY_Right, .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = RIGHT},
    { .IsKey = true, .Key = KEY_Up,    .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = DOWN},
    
};

key_mapping_t SelectMappings[] = {
    { .IsKey = true, .Key = KEY_Escape, .Operation.Type = OP_SetMode, .Operation.SetMode.Mode = MODE_Normal },
    
};

key_mapping_t FileSelectMappings[] = {
    { .IsKey = true, .Key = KEY_Escape, .Operation.Type = OP_SetMode, .Operation.SetMode.Mode = MODE_Normal },
    
    { .IsKey = true, .Key = KEY_Up,    .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = UP},
    { .IsKey = true, .Key = KEY_Down,  .Operation.Type = OP_MoveSelection, .Operation.MoveSelection.Dir = DOWN},
    
};

#endif //CUSTOM_H
