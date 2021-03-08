#ifndef PLATFORM_H
#define PLATFORM_H

typedef enum {
    MOD_Alt   = (1 << 0),
    MOD_Ctrl  = (1 << 1),
    MOD_Shift = (1 << 2),
} modifier_t;

typedef enum {
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
    
    KEY_MAX,
} key_t;

typedef enum {
    MOUSE_Left     = (1 << 0),
    MOUSE_Right    = (1 << 1),
    MOUSE_Backward = (1 << 2),
    MOUSE_Forward  = (1 << 3),
    MOUSE_Middle   = (1 << 4)
} mouse_t;

typedef struct {
    b8 IsText;
    b8 Handled;
    modifier_t Mods;
    union {
        key_t Key; // IsText == false
        u8 Char[4]; // utf8, IsText == true
    };
} key_input_t;

typedef struct {
    mouse_t MousePress, MouseDown;
    iv2_t MousePos, LastMousePos;
    modifier_t Mods;
    s32 Scroll; 
    
    MEM_STACK(key_input_t, 32) Keys;
} input_state_t;

typedef struct {
    // Pixel format is 0xAARRGGBB, in little-endian, i.e. the blue channel is at the lowest address.
    u8 *Data;
    s32 Width;
    s32 Height;
    irect_t Clip;
} framebuffer_t;

typedef struct {
    void *Data;
    s64 Size;
} platform_file_data_t;

typedef struct {
    framebuffer_t *Framebuffer;
    input_state_t InputState;
} platform_shared_t;

typedef enum {
    FILE_FLAGS_Unknown    = 0,
    FILE_FLAGS_Directory  = 1 << 1,
    FILE_FLAGS_Hidden     = 1 << 2,
} file_flags_t; 

typedef struct {
    struct { u64 Offset; u64 Size; } Name; // Stored in FileNameBuffer in directory_t to avoid allocating each file name string individually.
    file_flags_t Flags;
} file_info_t;

typedef struct {
    mem_buffer_t Path; // Always ends with a directory delimiter
    mem_buffer_t FileNameBuffer;
    file_info_t *Files; // stb
} directory_t;

// From the platform to editor
b8 platform_read_file(range_t Path, platform_file_data_t *FileData);
void platform_free_file(platform_file_data_t *FileData);

// platform_get_files_in_directory should not include current path (".") or parent path ("..") in the result written to Directory.
b8 platform_get_files_in_directory(range_t Path, directory_t *Directory);

b8 platform_push_subdirectory(mem_buffer_t *CurrentPath, range_t SubDirectory);

// From the editor to the platform
void k_do_editor(platform_shared_t *Shared);
void k_init(platform_shared_t *Shared, range_t CurrentDirectory);

#endif //PLATFORM_H