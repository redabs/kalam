#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max
#include <Windowsx.h>

#include "deps/stb_truetype.h"

#include "intrinsics.h"
#include "types.h"
#include "event.h"
#include "platform.h" 
#include "kalam.h"

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
win32_resize_framebuffer(win32_framebuffer_info_t *FbInfo, s32 Width, s32 Height) {
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
win32_window_callback(HWND Window, UINT Message, WPARAM WParameter, LPARAM LParameter) {
    LRESULT Result = 0;
    switch(Message) {
        case WM_CLOSE: {
            WmClose = true;
        } break;
        
        case WM_SIZE: {
            s32 Width = LOWORD(LParameter);
            s32 Height = HIWORD(LParameter);
            win32_resize_framebuffer(&FramebufferInfo, Width, Height);
        } break;
        
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
utf16_to_utf8(u32 Utf16, u8 *Utf8) {
    if(Utf16 <= 0x7f) {
        Utf8[0] = Utf16 & 0x7f; 
        
    } else if(Utf16 <= 0x7ff) {
        Utf8[0] = 0xc0 | ((Utf16 >> 6) & 0x1f);
        Utf8[1] = 0x80 | (Utf16        & 0x3f);
        
    } else if(Utf16 >= 0xe000 && Utf16 <= 0xffff) {
        Utf8[0] = 0xe0 | ((Utf16 >> 12) & 0xf);
        Utf8[1] = 0x80 | ((Utf16 >> 6)  & 0x3f);
        Utf8[2] = 0x80 | ( Utf16        & 0x3f);
        
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
    }
}

void
win32_handle_window_message(MSG *Message, HWND *WindowHandle, input_event_buffer_t *EventBuffer) {
    local_persist input_modifier Modifiers; 
    input_event_t Event = {0};
    switch(Message->message) {
        case WM_KEYDOWN: 
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            b32 WasDown = (b32)(Message->lParam >> 30);
            b32 IsDown = !(Message->lParam >> 31);
            
            Event.Device = INPUT_DEVICE_Keyboard;
            Event.Type = IsDown ? INPUT_EVENT_Press : INPUT_EVENT_Release;
            Event.Modifiers = Modifiers;
            Event.Key.KeyCode = Message->wParam;
            Event.Key.IsRepeatKey = WasDown && IsDown;
            
            if(!Event.Key.IsRepeatKey) {
                Modifiers ^= ((INPUT_MOD_Alt * (Event.Key.KeyCode == VK_MENU)) |
                              (INPUT_MOD_Control * (Event.Key.KeyCode == VK_CONTROL)) |
                              (INPUT_MOD_Shift * (Event.Key.KeyCode == VK_SHIFT)) |
                              (INPUT_MOD_CapsLock * (Event.Key.KeyCode == VK_CAPITAL)) |
                              (INPUT_MOD_NumLock * (Event.Key.KeyCode == VK_NUMLOCK)));
            }
            
            MSG CharMessage;
            if(TranslateMessage(Message)) {
                if(PeekMessageW(&CharMessage, *WindowHandle, 0, 0, PM_REMOVE)) {
                    if(CharMessage.message == WM_CHAR) {
                        utf16_to_utf8((u32)CharMessage.wParam, Event.Key.Character);
                        Event.Key.HasCharacterTranslation = true;
                    } 
                }
            }
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
            Event.Mouse.Position.x = (s32)GET_X_LPARAM(Message->lParam);
            Event.Mouse.Position.y = (s32)GET_Y_LPARAM(Message->lParam);
            
            push_input_event(EventBuffer, &Event);
        } break;
        
        case WM_MOUSEWHEEL: {
            Event.Type = INPUT_EVENT_Scroll;
            Event.Device = INPUT_DEVICE_Mouse;
            Event.Modifiers = Modifiers;
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
    WindowClass.lpfnWndProc = win32_window_callback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.lpszClassName = L"window_class";
    RegisterClassW(&WindowClass);
    
    // Including borders and decorations
    s32 WindowWidth = 900;
    s32 WindowHeight = 900;
    
    HWND WindowHandle = CreateWindowExW(0, WindowClass.lpszClassName, L"kalam", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, WindowWidth, WindowHeight, 0, 0, Instance, 0);
    if(WindowHandle) {
        {
            RECT ClientRect;
            GetClientRect(WindowHandle, &ClientRect);
            s32 BufferWidth = ClientRect.right - ClientRect.left;
            s32 BufferHeight = ClientRect.bottom - ClientRect.top;
            
            win32_resize_framebuffer(&FramebufferInfo, BufferWidth, BufferHeight);
        }
        k_init(&Shared);
        
        b8 Running = true;
        while(Running && !WmClose) {
            
            MSG Message = {0};
            while(PeekMessageW(&Message, WindowHandle, 0, 0, PM_REMOVE)) {
                DispatchMessageW(&Message);
                win32_handle_window_message(&Message, &WindowHandle, &Shared.EventBuffer);
            }
            
            
            k_do_editor(&Shared);
            
            HDC Dc = GetDC(WindowHandle);
            StretchDIBits(Dc, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, FramebufferInfo.Fb.Data, &FramebufferInfo.BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            ReleaseDC(WindowHandle, Dc);
        }
    } else {
        // TODO
    }
    
    return 1;
}