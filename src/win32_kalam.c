#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max
#include <Windowsx.h>
#include <direct.h> // _wgetcwd

#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "memory.h"
#include "types.h"
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
}

void
utf16_c_str_to_utf8_str(u16 *U16Str, mem_buffer_t *U8) {
    u16 *c = U16Str;
    while(*c) {
        b32 IsSurrogatePair = (*c >= 0xdc00);
        u32 Utf16 = IsSurrogatePair ? *(u32 *)c : *c;
        u8 Utf16Size = IsSurrogatePair ? 4 : 2;
        
        u8 Utf8[4];
        u8 n = utf16_to_utf8(Utf16, Utf8);
        mem_buf_append(U8, Utf8, n);
        
        c += Utf16Size >> 1;
    }
}

files_in_directory_t
platform_get_files_in_directory(range_t Path) {
    files_in_directory_t Result = {0};
    
    WIN32_FIND_DATAW FoundFile;
    
    mem_buffer_t WidePath = {0};
    {
        utf8_str_to_utf16_str(Path, &WidePath);
        // Null-terminate WidePath
        u16 Zero = 0;
        mem_buf_append(&WidePath, &Zero, 2);
        HANDLE FileHandle = FindFirstFileW((u16 *)WidePath.Data, &FoundFile);
        if(FileHandle != INVALID_HANDLE_VALUE) {
            for(b32 Go = true; Go; Go = FindNextFileW(FileHandle, &FoundFile)) {
                file_info_t *Fi = sb_add(Result.Files, 1);
                mem_zero_struct(Fi);
                utf16_c_str_to_utf8_str(FoundFile.cFileName, &Fi->FileName);
                
                Fi->Flags |= (FoundFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_FLAGS_Directory : 0;
                Fi->Flags |= (FoundFile.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? FILE_FLAGS_Hidden : 0;
            }
            
        }
    }
    
    mem_buf_free(&WidePath);
    return Result;
}

b32 
platform_read_file(char *Path, platform_file_data_t *FileData) {
    b32 Success = true;
    // TODO: IMPORTANT! Non-ASCII file paths
    HANDLE FileHandle = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    
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


#include <stdio.h> // TODO: Remove me

void
handle_window_message(MSG *Message, HWND *WindowHandle, input_event_buffer_t *EventBuffer) {
    local_persist input_modifier_t Modifiers; 
    local_persist input_toggles_t Toggles; 
    input_event_t Event = {0};
    switch(Message->message) {
        case WM_CHAR: {
            Event.Type = INPUT_EVENT_Text;
            utf16_to_utf8((u32)Message->wParam, Event.Text.Character);
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
        {
            RECT ClientRect;
            GetClientRect(WindowHandle, &ClientRect);
            s32 BufferWidth = ClientRect.right - ClientRect.left;
            s32 BufferHeight = ClientRect.bottom - ClientRect.top;
            
            resize_framebuffer(&FramebufferInfo, BufferWidth, BufferHeight);
        }
        
        {
            mem_buffer_t U8Path = {0};
            
            wchar_t *WPath = _wgetcwd(NULL, 0);
            utf16_c_str_to_utf8_str(WPath, &U8Path);
            
            k_init(&Shared, (range_t){.Data = U8Path.Data, .Size = U8Path.Used});
            
            free(WPath);
            mem_buf_free(&U8Path);
        }
        
        b8 Running = true;
        while(Running && !WmClose) {
            
            MSG Message = {0};
            while(PeekMessageW(&Message, WindowHandle, 0, 0, PM_REMOVE)) {
                handle_window_message(&Message, &WindowHandle, &Shared.EventBuffer);
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
            }
            
            k_do_editor(&Shared);
            
            HDC Dc = GetDC(WindowHandle);
            StretchDIBits(Dc, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, FramebufferInfo.Fb.Data, &FramebufferInfo.BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            ReleaseDC(WindowHandle, Dc);
        }
    } else {
        // TODO Diagnostics!
    }
    
    return 0;
}