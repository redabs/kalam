#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max
#include <Windowsx.h>

#include "deps/stb_truetype.h"

#include "intrinsics.h"
#include "types.h"
#include "event.h"
#include "renderer.h"
#include "platform.h" 
#include "font.h"

b8 WmClose = false;

typedef struct {
    framebuffer_t Fb;
    BITMAPINFO BitmapInfo;
} win32_framebuffer_info_t;

win32_framebuffer_info_t FramebufferInfo = {.Fb.BytesPerPixel = 4, .BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER)};

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
    // TODO: IMPORTANT! Non-ASCII file paths
    HANDLE FileHandle = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle == INVALID_HANDLE_VALUE) {
        // TODO
        return false;
    }
    
    s64 FileSize = 0;
    if(!win32_get_file_size(FileHandle, &FileSize)) {
        // TODO
        return false;
    } else if(FileSize > GIGABYTES(1)) {
        // TODO: Handle large files. Map only parts of files bigger than a certain size?
        CloseHandle(FileHandle);
        return false;
    }
    
    FileData->Data = VirtualAlloc(0, FileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    DWORD SizeRead = 0;
    if(!ReadFile(FileHandle, FileData->Data, (DWORD)FileSize, &SizeRead, 0)) { 
        VirtualFree(FileData->Data, 0, MEM_RELEASE);
        CloseHandle(FileHandle);
        return false;
    }
    FileData->Size = (s64)SizeRead;
    
    return true;
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
    
    FbInfo->BitmapInfo.bmiHeader.biWidth = Width;
    FbInfo->BitmapInfo.bmiHeader.biHeight = -Height;
    FbInfo->BitmapInfo.bmiHeader.biPlanes = 1;
    FbInfo->BitmapInfo.bmiHeader.biBitCount = 32;
    FbInfo->BitmapInfo.bmiHeader.biCompression = BI_RGB;
    
    FbInfo->Fb.BytesPerPixel = 4;
    FbInfo->Fb.Data = VirtualAlloc(0, Width * Height * FbInfo->Fb.BytesPerPixel, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
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
            Result = DefWindowProc(Window, Message, WParameter, LParameter);
        } break;
    }
    
    return Result;
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
                if(PeekMessage(&CharMessage, *WindowHandle, 0, 0, PM_REMOVE)) {
                    if(CharMessage.message == WM_CHAR) {
                        Event.Key.Character = CharMessage.wParam;
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
    WNDCLASSA WindowClass = {0};
    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc = win32_window_callback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.lpszClassName = "window_class";
    RegisterClass(&WindowClass);
    
    // Including borders and decorations
    s32 WindowWidth = 900;
    s32 WindowHeight = 900;
    
    HWND WindowHandle = CreateWindowEx(0, WindowClass.lpszClassName, "kalam", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, WindowWidth, WindowHeight, 0, 0, Instance, 0);
    if(WindowHandle) {
        {
            RECT ClientRect;
            GetClientRect(WindowHandle, &ClientRect);
            s32 BufferWidth = ClientRect.right - ClientRect.left;
            s32 BufferHeight = ClientRect.bottom - ClientRect.top;
            
            win32_resize_framebuffer(&FramebufferInfo, BufferWidth, BufferHeight);
        }
        
        font_t Font = f_load_ttf("fonts/consola.ttf", 16);
        
        input_event_buffer_t EventBuffer = {0};
        b8 Running = true;
        while(Running && !WmClose) {
            
            MSG Message = {0};
            while(PeekMessage(&Message, WindowHandle, 0, 0, PM_REMOVE)) {
                DispatchMessage(&Message);
                win32_handle_window_message(&Message, &WindowHandle, &EventBuffer);
            }
            
            r_draw_rect(&FramebufferInfo.Fb, (irect_t){.x=10, .y=100, .w=80, .h=100}, 0xffaabbcc);
#if 0
            r_draw_rect_bitmap(&FramebufferInfo.Fb, 0, 0, (irect_t){.x = 0, .y = 0, .w = Font.Bitmap.w, .h = Font.Bitmap.h}, &Font.Bitmap);
            s32 CursorX = 50;
            for(int i = 0; i < 256; ++i) {
                s32 Baseline = 300;
                s32 x0 = Font.BakedChars[i].x0;
                s32 x1 = Font.BakedChars[i].x1;
                s32 y0 = Font.BakedChars[i].y0;
                s32 y1 = Font.BakedChars[i].y1;
                irect_t Rect = {.x = x0, .y = y0, .w = x1 - x0, .h = y1 - y0};
                r_draw_rect_bitmap(&FramebufferInfo.Fb, CursorX + Font.BakedChars[i].xoff, Baseline + Font.BakedChars[i].yoff, Rect, &Font.Bitmap);
                CursorX += Font.BakedChars[i].xadvance;
            }
#endif
            
            HDC Dc = GetDC(WindowHandle);
            StretchDIBits(Dc, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, 0, 0, FramebufferInfo.Fb.Width, FramebufferInfo.Fb.Height, FramebufferInfo.Fb.Data, &FramebufferInfo.BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            ReleaseDC(WindowHandle, Dc);
            
            EventBuffer.Count = 0;
        }
    } else {
        // TODO
    }
    
    return 1;
}