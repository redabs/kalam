#ifndef EVENT_H
#define EVENT_H

typedef enum {
    INPUT_EVENT_Press = 0,
    INPUT_EVENT_Release,
    INPUT_EVENT_Scroll,
} input_event_type;

typedef enum {
    INPUT_DEVICE_Keyboard = 0,
    INPUT_DEVICE_Mouse,
} input_device;

typedef enum {
    INPUT_MOD_Alt      = 1,
    INPUT_MOD_Control  = 1 << 1,
    INPUT_MOD_Shift    = 1 << 2,
    INPUT_MOD_CapsLock = 1 << 3,
    INPUT_MOD_NumLock  = 1 << 4,
} input_modifier;

typedef enum {
    INPUT_MOUSE_Left = 0,
    INPUT_MOUSE_Right,
    INPUT_MOUSE_Middle,
    INPUT_MOUSE_Forward,
    INPUT_MOUSE_Backward,
    
    INPUT_MOUSE_MAX
} input_mouse_button;

typedef struct {
    input_event_type Type;
    input_device Device;
    input_modifier Modifiers;
    union {
        struct {
            u64 KeyCode;
            u64 Character;
            b8 HasCharacterTranslation;
            b8 IsRepeatKey;
        } Key;
        
        struct {
            ivec2_t Position;
            input_mouse_button Button;
        } Mouse;
        
        struct {
            ivec2_t Position;
            s32 Delta; // TODO(Redab): Canonicalize scroll values across platforms.
        } Scroll;
    };
} input_event_t;

#define INPUT_EVENT_MAX 32
typedef struct {
    s32 Count;
    input_event_t Events[INPUT_EVENT_MAX];
} input_event_buffer_t;



#endif //EVENT_H
