#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max
#include <Windowsx.h>
#include <direct.h> // _wgetcwd
#include <shlwapi.h> // PathFileExistsW

#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "platform.h" 

b8 WmClose = false;

typedef struct {
    framebuffer_t Fb;
    BITMAPINFO BitmapInfo;
} win32_framebuffer_info_t;

platform_shared_t Shared;
win32_framebuffer_info_t FramebufferInfo;

b32
win32_get_file_size(HANDLE FileHandle, s64 *Out) {
    LARGE_INTEGER FileSize;
    if(GetFileSizeEx(FileHandle, &FileSize)) {
        *Out = FileSize.QuadPart;
        return true;
    }
    return false;
}

inline u8
utf16_to_utf8(u32 Utf16, u8 *Utf8) {
    if(Utf16 <= 0x7f) {
        Utf8[0] = Utf16 & 0x7f; 
        return 1;
        
    } else if(Utf16 <= 0x7ff) {
        Utf8[0] = 0xc0 | ((Utf16 >> 6) & 0x1f);
        Utf8[1] = 0x80 | (Utf16        & 0x3f);
        return 2;
        
    } else if(Utf16 >= 0xe000 && Utf16 <= 0xffff) {
        Utf8[0] = 0xe0 | ((Utf16 >> 12) & 0xf);
        Utf8[1] = 0x80 | ((Utf16 >> 6)  & 0x3f);
        Utf8[2] = 0x80 | ( Utf16        & 0x3f);
        return 3;
        
    } else {
        u32 High = Utf16 >> 16;
        u32 Low = Utf16 & 0xffff;
        // Surrogate pairs
        if((High >= 0xd800 && High <= 0xdfff)  && (Low >= 0xd800 && Low <= 0xdfff)) {
            High = (High - 0xd800) << 10;
            Low = Low - 0xdc00;
            u32 Codepoint = High + Low + 0x10000;
            
            Utf8[0] = 0xf0 | ((Codepoint >> 18) & 0x7);
            Utf8[1] = 0x80 | ((Codepoint >> 12) & 0x3f);
            Utf8[2] = 0x80 | ((Codepoint >> 6)  & 0x3f);
            Utf8[3] = 0x80 | ( Codepoint        & 0x3f);
        } 
        
        return 4;
    }
}


inline u32
codepoint_to_utf16(u32 Codepoint) {
    if(Codepoint <= 0xffff) {
        // Utf-16 encodes code points in this range as single 16-bit code units that are numerically equal to the corresponding code points.
        return Codepoint;
    } else {
        Codepoint -= 0x10000;
        u32 Result = 0;
        Result = ((Codepoint >> 10) + 0xd800) << 16; // High surrogate
        Result |= (Codepoint & 0x3ff) + 0xdc00; // Low surrogate
        return Codepoint;
    }
}

void
utf8_str_to_utf16_str(range_t U8, mem_buffer_t *U16) {
    for(u64 i = 0; i < U8.Size; ++i) {
        u8 *c = (u8 *)U8.Data + i;
        
        u32 Cp = 0;
        c = utf8_to_codepoint(c, &Cp);
        u32 Utf16 = codepoint_to_utf16(Cp);
        // Surrogate pairs are 4 bytes total
        if(Utf16 > 0xffff) {
            mem_buf_append(U16, &Utf16, 4);
        } else {
            mem_buf_append(U16, &Utf16, 2);
        }
    }
    // Because this function mostly exists to interface with windows and windows expects null terminated strings 
    // we might aswell null terminate the string in here in case the caller forgets to.
    mem_buf_null_bytes(U16, 2);
}

u64
utf16_c_str_to_utf8_str(u16 *U16Str, mem_buffer_t *U8) {
    u64 SizeWritten = 0;
    u16 *c = U16Str;
    while(*c) {
        b32 IsSurrogatePair = (*c >= 0xdc00);
        u32 Utf16 = IsSurrogatePair ? *(u32 *)c : *c;
        u8 Utf16Size = IsSurrogatePair ? 4 : 2;
        
        u8 Utf8[4];
        u8 n = utf16_to_utf8(Utf16, Utf8);
        mem_buf_append(U8, Utf8, n);
        
        c += Utf16Size >> 1;
        SizeWritten += n;
    }
    
    return SizeWritten;
}

b8
platform_get_files_in_directory(range_t Path, directory_t *Directory) {
    if(Path.Size == 0) {
        return false; 
    }
    mem_buf_clear(&Directory->Path);
    mem_buf_clear(&Directory->FileNameBuffer);
    sb_set_count(Directory->Files, 0);
    
    mem_buf_append_range(&Directory->Path, Path);
    
    WIN32_FIND_DATAW FoundFile;
    
    mem_buffer_t WidePath = {0};
    utf8_str_to_utf16_str(Path, &WidePath);
    
    mem_buf_append_range(&WidePath, C_STR_AS_RANGE(L"*"));
    mem_buf_null_bytes(&WidePath, sizeof(wchar_t));
    
    HANDLE FileHandle = FindFirstFileW((u16 *)WidePath.Data, &FoundFile);
    if(FileHandle != INVALID_HANDLE_VALUE) {
        for(b32 Go = true; Go; Go = FindNextFileW(FileHandle, &FoundFile)) {
            if((FoundFile.cFileName[0] == L'.' && FoundFile.cFileName[1] == 0) || 
               (FoundFile.cFileName[0] == L'.' && FoundFile.cFileName[1] == L'.' && FoundFile.cFileName[2] == 0)) {
                continue;
            }
            file_info_t *Fi = sb_add(Directory->Files, 1);
            mem_zero_struct(Fi);
            Fi->Name.Offset = Directory->FileNameBuffer.Used;
            Fi->Name.Size = utf16_c_str_to_utf8_str(FoundFile.cFileName, &Directory->FileNameBuffer);
            
            Fi->Flags |= (FoundFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_FLAGS_Directory : 0;
            Fi->Flags |= (FoundFile.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? FILE_FLAGS_Hidden : 0;
        }
    }
    
    mem_buf_free(&WidePath);
    return true;
}

b8 
platform_read_file(range_t Path, platform_file_data_t *FileData) {
    b8 Success = true;
    
    HANDLE FileHandle;
    {
        mem_buffer_t WidePath = {0};
        utf8_str_to_utf16_str(Path, &WidePath);
        mem_buf_null_bytes(&WidePath, sizeof(wchar_t));
        FileHandle = CreateFileW((wchar_t *)WidePath.Data, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        mem_buf_free(&WidePath);
    }
    
    s64 FileSize = 0;
    if(FileHandle != INVALID_HANDLE_VALUE) {
        if(win32_get_file_size(FileHandle, &FileSize)) {
            FileData->Data = VirtualAlloc(0, FileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            DWORD SizeRead = 0;
            if(ReadFile(FileHandle, FileData->Data, (DWORD)FileSize, &SizeRead, 0)) {
                FileData->Size = (s64)SizeRead;
                Success = true;
            } else {
                VirtualFree(FileData->Data, 0, MEM_RELEASE);
                // TODO: Diagnostics
                Success = false;
            }
        } else {
            // TODO: Diagnostics
            Success = false;
        }
    } else {
        // TODO: Diagnostics
        Success = false;
    }
    
    CloseHandle(FileHandle);
    return Success;
}

void
platform_free_file(platform_file_data_t *File) {
    VirtualFree(File->Data, 0, MEM_RELEASE);
}

void
resize_framebuffer(win32_framebuffer_info_t *FbInfo, s32 Width, s32 Height) {
    if(FbInfo->Fb.Data) {
        VirtualFree(FbInfo->Fb.Data, 0, MEM_RELEASE);
    }
    FbInfo->Fb.Width = Width;
    FbInfo->Fb.Height = Height;
    
    FbInfo->BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    FbInfo->BitmapInfo.bmiHeader.biWidth = Width;
    FbInfo->BitmapInfo.bmiHeader.biHeight = -Height;
    FbInfo->BitmapInfo.bmiHeader.biPlanes = 1;
    FbInfo->BitmapInfo.bmiHeader.biBitCount = 32;
    FbInfo->BitmapInfo.bmiHeader.biCompression = BI_RGB;
    
    
    FbInfo->Fb.Data = VirtualAlloc(0, Width * Height * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

LRESULT CALLBACK
window_callback(HWND Window, UINT Message, WPARAM WParameter, LPARAM LParameter) {
    LRESULT Result = 0;
    switch(Message) {
        case WM_CLOSE: {
            WmClose = true;
        } break;
        
        case WM_SIZE: {
            s32 Width = LOWORD(LParameter);
            s32 Height = HIWORD(LParameter);
            resize_framebuffer(&FramebufferInfo, Width, Height);
        } break;
        
        case WM_SYSCOMMAND:
        // Disable stupid alt-menu
        if(WParameter == SC_KEYMENU) {
            break;
        }
        // Fall through
        default: {
            Result = DefWindowProcW(Window, Message, WParameter, LParameter);
        } break;
    }
    
    return Result;
}

void
push_input_event(input_event_buffer_t *Buffer, input_event_t *Event) {
    ASSERT(Buffer->Count + 1 < INPUT_EVENT_MAX);
    Buffer->Events[Buffer->Count] = *Event;
    ++Buffer->Count;
}

void
handle_window_message(MSG *Message, HWND *WindowHandle, input_event_buffer_t *EventBuffer) {
    local_persist input_modifier_t Modifiers; 
    local_persist input_toggles_t Toggles; 
    input_event_t Event = {0};
    switch(Message->message) {
        case WM_CHAR: {
            // TODO: Surrogate pairs don't come packed into wParam as two 16-bit values, they are separated into two different WM_CHARs.
            Event.Type = INPUT_EVENT_Text;
            utf16_to_utf8((u32)Message->wParam, Event.Text.Character);
            if(Event.Text.Character[0] == '\r') {
                Event.Text.Character[0] = '\n';
            }
        } goto process_key;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN: 
        case WM_KEYUP: {
            b32 WasDown = (b32)(Message->lParam >> 30);
            b32 IsDown = !(Message->lParam >> 31);
            Event.Type = IsDown ? INPUT_EVENT_Press : INPUT_EVENT_Release;
            
            Event.Device = INPUT_DEVICE_Keyboard;
            Event.Key.IsRepeatKey = WasDown && IsDown;
            Event.Key.KeyCode = Message->wParam == VK_DELETE ? KEY_Delete : Message->wParam; 
        } 
        process_key: {
            Modifiers = GetKeyState(VK_CONTROL) & 0x8000 ? Modifiers | INPUT_MOD_Ctrl : Modifiers & ~(INPUT_MOD_Ctrl);
            Modifiers = GetKeyState(VK_MENU) & 0x8000 ? Modifiers | INPUT_MOD_Alt : Modifiers & ~(INPUT_MOD_Alt);
            Modifiers = GetKeyState(VK_SHIFT) & 0x8000 ? Modifiers | INPUT_MOD_Shift : Modifiers & ~(INPUT_MOD_Shift);
            
            Toggles = (GetKeyState(VK_CAPITAL) & 1) ? Toggles | INPUT_TOGGLES_CapsLock : Toggles & ~(INPUT_TOGGLES_CapsLock);
            Toggles = (GetKeyState(VK_NUMLOCK) & 1) ? Toggles | INPUT_TOGGLES_NumLock : Toggles & ~(INPUT_TOGGLES_NumLock);
            
            Event.Modifiers = Modifiers;
            Event.Toggles = Toggles;
            
            push_input_event(EventBuffer, &Event);
        } break;
        
        case WM_RBUTTONDOWN: 
        Event.Type = INPUT_EVENT_Press; 
        Event.Mouse.Button = INPUT_MOUSE_Right;
        goto process_mouse;
        
        case WM_RBUTTONUP:
        Event.Type = INPUT_EVENT_Release;
        Event.Mouse.Button = INPUT_MOUSE_Right;
        goto process_mouse;
        
        case WM_LBUTTONDOWN:
        Event.Type = INPUT_EVENT_Press;
        Event.Mouse.Button = INPUT_MOUSE_Left;
        goto process_mouse;
        
        case WM_LBUTTONUP: 
        Event.Type = INPUT_EVENT_Release;
        Event.Mouse.Button = INPUT_MOUSE_Left;
        process_mouse: {
            Event.Device = INPUT_DEVICE_Mouse;
            Event.Modifiers = Modifiers;
            Event.Toggles = Toggles;
            Event.Mouse.Position.x = (s32)GET_X_LPARAM(Message->lParam);
            Event.Mouse.Position.y = (s32)GET_Y_LPARAM(Message->lParam);
            
            push_input_event(EventBuffer, &Event);
        } break;
        
        case WM_MOUSEWHEEL: {
            Event.Device = INPUT_DEVICE_Mouse;
            Event.Type = INPUT_EVENT_Scroll;
            Event.Modifiers = Modifiers;
            Event.Toggles = Toggles;
            s32 Delta = GET_WHEEL_DELTA_WPARAM(Message->wParam);
            Event.Scroll.Delta = (Delta < 0) ? -1 : 1;
            
            POINT Point;
            GetCursorPos(&Point);
            ScreenToClient(*WindowHandle, &Point);
            Event.Scroll.Position.x = Point.x;
            Event.Scroll.Position.y = Point.y;
            
            push_input_event(EventBuffer, &Event);
        } break;
    }
}

mem_buffer_t 
get_current_directory() {
    DWORD PathBufferSizeInChars = MAX_PATH;
    wchar_t *Path = malloc(sizeof(wchar_t) * PathBufferSizeInChars);
    DWORD Length = GetCurrentDirectory((PathBufferSizeInChars - 1), Path); // Reserve a slot for the possibility to add a trailing backslash
    
    if(Length > PathBufferSizeInChars) {
        PathBufferSizeInChars = Length + 1; // plus one for potentially adding a trailing backslash
        Path = realloc(Path, PathBufferSizeInChars);
        Length = GetCurrentDirectory(PathBufferSizeInChars, Path);
    }
    
    { // Add trailing backslash if there is none 
        wchar_t *c = Path;
        while(*c) { ++c; }
        if(c != Path && *(c - 1) != L'\\') {
            *c = L'\\';
            *(c + 1) = 0;
        }
    }
    
    mem_buffer_t U8Path = {0};
    utf16_c_str_to_utf8_str(Path, &U8Path);
    
    free(Path);
    
    return U8Path;
}

// Writes new path to CurrentPath
b8
platform_push_subdirectory(mem_buffer_t *CurrentPath, range_t SubDirectory) {
    mem_buf_append_range(CurrentPath, SubDirectory);
    
    // We're always wide with Windows.
    mem_buffer_t Utf16Dest = {0};
    utf8_str_to_utf16_str(mem_buf_as_range(*CurrentPath), &Utf16Dest);
    
    b8 Success = (PathFileExistsW((wchar_t *)Utf16Dest.Data)) ? true : false; // BOOL -> b8
    if(!Success) {
        mem_buf_pop_size(CurrentPath, SubDirectory.Size);
    }
    
    mem_buf_free(&Utf16Dest);
    return Success;
}

int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCommand) {
    Shared.Framebuffer = &FramebufferInfo.Fb;
    
    WNDCLASSW WindowClass = {0};
    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc = window_callback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.lpszClassName = L"window_class";
    RegisterClassW(&WindowClass);
    
    // Including borders and decorations
    s32 WindowWidth = 900;
    s32 WindowHeight = 900;
    
    HWND WindowHandle = CreateWindowExW(0, WindowClass.lpszClassName, L"kalam", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 820, 380, WindowWidth, WindowHeight, 0, 0, Instance, 0);
    if(WindowHandle) {
        b32 IsUnicode = IsWindowUnicode(WindowHandle);
        ASSERT(IsUnicode);
        
        {
            RECT ClientRect;
            GetClientRect(WindowHandle, &ClientRect);
            s32 BufferWidth = ClientRect.right - ClientRect.left;
            s32 BufferHeight = ClientRect.bottom - ClientRect.top;
            
            resize_framebuffer(&FramebufferInfo, BufferWidth, BufferHeight);
        }
        
        {
            mem_buffer_t Path = get_current_directory();
            k_init(&Shared, (range_t){.Data = Path.Data, .Size = Path.Used});
            mem_buf_free(&Path);
        }
        
        LARGE_INTEGER CounterStart;
        QueryPerformanceCounter(&CounterStart);
        LARGE_INTEGER End;
        
        s64 Frequency;
        {
            LARGE_INTEGER F;
            QueryPerformanceFrequency(&F);
            Frequency = F.QuadPart;
        }
        f64 FrameTimer = 0;
        f64 SecondsElapsed = 0;
        
        b8 Running = true;
        while(Running && !WmClose) {
            
            MSG Message = {0};
            while(PeekMessageW(&Message, WindowHandle, 0, 0, PM_REMOVE)) {
                handle_window_message(&Message, &WindowHandle, &Shared.EventBuffer);
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
            }
            
            POINT CursorPoint;
            GetCursorPos(&CursorPoint);
            ScreenToClient(WindowHandle, &CursorPoint);
            Shared.MousePos = (iv2_t){.x = CursorPoint.x, .y = CursorPoint.y};
            k_do_editor(&Shared);
            
            HDC Dc = GetDC(WindowHandle);
            StretchDIBits(Dc, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, FramebufferInfo.Fb.Data, &FramebufferInfo.BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            ReleaseDC(WindowHandle, Dc);
            
            
            QueryPerformanceCounter(&End);
            s64 CounterElapsed = End.QuadPart - CounterStart.QuadPart;
            CounterStart = End;
            
            SecondsElapsed = (f64)CounterElapsed / Frequency;
            f64 MilliSecondsElapsed = 1000. * SecondsElapsed;
            FrameTimer += MilliSecondsElapsed;
            
            f64 Ms = (1000. / 30);
            if(FrameTimer < Ms) {
                DWORD SleepTime = (DWORD)(Ms - FrameTimer);
                Sleep(SleepTime);
            }
            
            FrameTimer = 0;
        }
    } else {
        // TODO Diagnostics!
    }
    
    return 0;
}