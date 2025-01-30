#ifndef K_INPUT
#define K_INPUT
BIT_FLAG_ENUM(modifier, u8) {
    MOD_None = 0,
    MOD_Alt   = (1 << 0),
    MOD_Ctrl  = (1 << 1),
    MOD_Shift = (1 << 2),
};

BIT_FLAG_ENUM(mouse, u8) {
    MOUSE_Left     = (1 << 0),
    MOUSE_Right    = (1 << 1),
    MOUSE_Backward = (1 << 2),
    MOUSE_Forward  = (1 << 3),
    MOUSE_Middle   = (1 << 4)
};

enum key {
    KEY_Invalid = 0x0,
    KEY_Backspace = 0x8,
    KEY_Tab = 0x9,
    KEY_Return = 0x0d,
    KEY_Shift = 0x10,
    KEY_Ctrl = 0x11,
    KEY_Alt = 0x12,
    
    KEY_CapsLock = 0x14,
    
    KEY_Escape = 0x1b,
    
    KEY_Space = 0x20,
    KEY_PageUp = 0x21,
    KEY_PageDown = 0x22,
    KEY_End = 0x23,
    KEY_Home = 0x24,
    KEY_Left = 0x25,
    KEY_Up = 0x26,
    KEY_Right = 0x27,
    KEY_Down = 0x28,
    
    KEY_Insert = 0x2d,
    
    
    KEY_0 = 0x30,
    KEY_1 = 0x31,
    KEY_2 = 0x32,
    KEY_3 = 0x33,
    KEY_4 = 0x34,
    KEY_5 = 0x35,
    KEY_6 = 0x36,
    KEY_7 = 0x37,
    KEY_8 = 0x38,
    KEY_9 = 0x39,
    
    KEY_A = 0x41,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    
    KEY_Delete = 0x7f,
    
    KEY_Max,
};

struct key_event {
    b8 IsText;
    b8 Handled;
    modifier Modifiers;
    union {
        key Key; // IsText == false
        u8 Char[4]; // utf8, IsText == true
    };
};

#define KEY_EVENT_MAX 32
struct input_state {
    mouse MousePress, MouseDown;
    iv2 MousePos, LastMousePos;
    
    modifier Modifiers; // Used with scroll only, not with key inputs! key_input has their own modifiers.
    s32 Scroll; 
    
    u32 EventCount;
    key_event Events[KEY_EVENT_MAX];
};

inline void
reset_input_state(input_state *InputState) {
    InputState->MousePress = 0;
    InputState->Scroll = 0;
    InputState->EventCount = 0;
}

#endif
