#ifndef EVENT_H
#define EVENT_H

typedef enum {
    INPUT_EVENT_Press = 0,
    INPUT_EVENT_Release,
    INPUT_EVENT_Scroll,
} input_event_type_t;

typedef enum {
    INPUT_DEVICE_Keyboard = 0,
    INPUT_DEVICE_Mouse,
} input_device_t;

typedef enum {
    INPUT_MOD_Alt      = 1,
    INPUT_MOD_Ctrl     = 1 << 1,
    INPUT_MOD_Shift    = 1 << 2,
    INPUT_MOD_CapsLock = 1 << 3,
    INPUT_MOD_NumLock  = 1 << 4,
} input_modifier_t;

typedef enum {
    INPUT_MOUSE_Left = 0,
    INPUT_MOUSE_Right,
    INPUT_MOUSE_Middle,
    INPUT_MOUSE_Forward,
    INPUT_MOUSE_Backward,
    
    INPUT_MOUSE_MAX
} input_mouse_button_t;

enum input_key_t {
    KEY_Backspace = 0x8,
    KEY_Tab = 0x9,
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
    
    KEY_MAX,
};

typedef struct {
    input_event_type_t Type;
    input_device_t Device;
    input_modifier_t Modifiers;
    union {
        struct {
            u64 KeyCode;
            u8 Character[4]; // utf8
            b8 HasCharacterTranslation;
            b8 IsRepeatKey;
        } Key;
        
        struct {
            iv2_t Position;
            input_mouse_button_t Button;
        } Mouse;
        
        struct {
            iv2_t Position;
            s32 Delta; // TODO(Redab): Canonicalize scroll values across platforms.
        } Scroll;
    };
} input_event_t;

#define INPUT_EVENT_MAX 32
typedef struct {
    s32 Count;
    input_event_t Events[INPUT_EVENT_MAX];
} input_event_buffer_t;

void e_push_input_event(input_event_buffer_t *Buffer, input_event_t *Event);

#endif //EVENT_H
