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

#endif //CUSTOM_H
