#include "event.c"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max
#include <Windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM

#include "intrinsics.h"
#include "types.h"
#include "event.h"

#include "kalam.h"

b8 WmClose = false;

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
            // TODO(Redab): Resize backbuffer
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
                Modifiers ^= (
                              (INPUT_MOD_Alt * (Event.Key.KeyCode == VK_MENU)) |
                              (INPUT_MOD_Control * (Event.Key.KeyCode == VK_CONTROL)) |
                              (INPUT_MOD_Shift * (Event.Key.KeyCode == VK_SHIFT)) |
                              (INPUT_MOD_CapsLock * (Event.Key.KeyCode == VK_CAPITAL)) |
                              (INPUT_MOD_NumLock * (Event.Key.KeyCode == VK_NUMLOCK))
                              );
            }
            
            // NOTE(Redab): TranslateMessage should've been called before for us to get the WM_CHAR.
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
    BOOL bla = IsWindowUnicode(WindowHandle);
    
    RECT ClientRect;
    GetClientRect(WindowHandle, &ClientRect);
    s32 DrawBufferWidth = ClientRect.right - ClientRect.left;
    s32 DrawBufferHeight = ClientRect.bottom - ClientRect.top;
    
    input_event_buffer_t EventBuffer = {0};
    
    if(WindowHandle) {
        b8 Running = true;
        while(Running && !WmClose) {
            
            MSG Message = {0};
            while(PeekMessage(&Message, WindowHandle, 0, 0, PM_REMOVE)) {
                DispatchMessage(&Message);
                win32_handle_window_message(&Message, &WindowHandle, &EventBuffer);
            }
            
            for(s32 i = 0; i < EventBuffer.Count; ++i) {
                input_event_t *e = EventBuffer.Events + i;
                if(e->Type == INPUT_EVENT_Press && 
                   e->Device == INPUT_DEVICE_Keyboard &&
                   e->Key.HasCharacterTranslation) {
                    char c[2] = {e->Key.Character, 0};
                    OutputDebugString(c);
                }
            }
            
            EventBuffer.Count = 0;
        }
    } else {
        // TODO
    }
    
    return 1;
}