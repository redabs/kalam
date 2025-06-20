#include "kalam.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max

#include <stdio.h>

#include "k_memory.cpp"

bool gWindowWasClosed = false;
BITMAPINFO gBitmapInfo = {};
framebuffer gFramebuffer = {};
platform_api gPlatform = {};

PLATFORM_ALLOC(win32_allocate_memory) {
    void *Result = 0;
    Result = VirtualAlloc(0, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return Result;
}

PLATFORM_FREE(win32_free_memory) {
    if(Ptr == 0) { return; }
    BOOL Ret = VirtualFree(Ptr, 0, MEM_RELEASE);
    DWORD Error = GetLastError();
    ASSERT(Ret != 0);
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

PLATFORM_READ_FILE(win32_read_file) {
    HANDLE FileHandle = INVALID_HANDLE_VALUE;

    {
        u64 WidePathBufferLength = (u64)MultiByteToWideChar(CP_UTF8, 0, (char *)Path.Ptr, (int)Path.Count, 0, 0);
        wchar_t *WidePath = (wchar_t *) gPlatform.alloc(WidePathBufferLength * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, (char *)Path.Ptr, (int)Path.Count, WidePath, (int)WidePathBufferLength);
        FileHandle = CreateFile(WidePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        gPlatform.free(WidePath);
    }

    buffer<u8> Data = {};
    if(FileHandle == INVALID_HANDLE_VALUE) {
        return Data;
    } 

    {
        LARGE_INTEGER FileSize = {};
        if(GetFileSizeEx(FileHandle, &FileSize) == 0) {
            // GetFileSizeEx Failure
            CloseHandle(FileHandle);
            return Data;
        }
        add(&Data, FileSize.QuadPart);
    }

    DWORD BytesRead = 0;

    // TODO: Handle chonkier files

    BOOL ReadSuccess = ReadFile(FileHandle, Data.Ptr, (DWORD)Data.Count, &BytesRead, NULL);
    CloseHandle(FileHandle);

    if(!ReadSuccess) {
        destroy(&Data);
        return Data;
    }

    return Data;
}

PLATFORM_WRITE_FILE(win32_write_file) {
    HANDLE FileHandle = INVALID_HANDLE_VALUE;

    {
        u64 WidePathBufferLength = (u64)MultiByteToWideChar(CP_UTF8, 0, (char *)Path.Ptr, (int)Path.Count, 0, 0);
        wchar_t *WidePath = (wchar_t *) gPlatform.alloc(WidePathBufferLength * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, (char *)Path.Ptr, (int)Path.Count, WidePath, (int)WidePathBufferLength);
        FileHandle = CreateFile(WidePath, GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        gPlatform.free(WidePath);
    }

    if(FileHandle != INVALID_HANDLE_VALUE) {
        DWORD Error = GetLastError();
        ASSERT(FileHandle != INVALID_HANDLE_VALUE);
    }


    DWORD BytesWritten = 0;
    BOOL WriteSuccess = WriteFile(FileHandle, Data.Ptr, (DWORD)Data.Count, &BytesWritten, 0);
    if(WriteSuccess) {
        CloseHandle(FileHandle);
    } else {
        DWORD Error = GetLastError();
        ASSERT(WriteSuccess);
    }

    ASSERT(BytesWritten == Data.Count);

}

void
init_platform_api() {
    gPlatform.alloc = win32_allocate_memory;
    gPlatform.free = win32_free_memory;
    gPlatform.read_file = win32_read_file;
    gPlatform.write_file = win32_write_file;
}

inline u8
utf16_to_utf8(u32 Utf16, u8 Utf8[4]) {
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

void
win32_resize_framebuffer(framebuffer *Fb, BITMAPINFO *Bi, s32 Width, s32 Height) {
    if(Fb->Pixels) {
        VirtualFree(Fb->Pixels, 0, MEM_RELEASE);
    }
    Fb->Width = Width;
    Fb->Height = Height;
    
    Bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    Bi->bmiHeader.biWidth = Width;
    Bi->bmiHeader.biHeight = -Height;
    Bi->bmiHeader.biPlanes = 1;
    Bi->bmiHeader.biBitCount = 32;
    Bi->bmiHeader.biCompression = BI_RGB;
    
    Fb->Pixels = (u32 *)VirtualAlloc(0, Width * Height * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ASSERT(Fb->Pixels);
}

LRESULT CALLBACK
win32_window_callback(HWND Window, UINT Message, WPARAM WParameter, LPARAM LParameter) {
    LRESULT Result = 0;
    switch(Message) {
        case WM_CLOSE: {
            gWindowWasClosed = true;
        } break;
        
        case WM_SIZE: {
            // SIZE_MINIMIZED messages give us Width = 0 and Height = 0, ignore those.
            if(WParameter != SIZE_MINIMIZED) {
                s32 Width = LOWORD(LParameter);
                s32 Height = HIWORD(LParameter);
                win32_resize_framebuffer(&gFramebuffer, &gBitmapInfo, Width, Height);
            }
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

modifier 
win32_get_modifiers() {
    modifier Mods = (modifier)0;
    if(GetKeyState(VK_CONTROL) & 0x8000) { Mods |= MOD_Ctrl; }
    if(GetKeyState(VK_MENU) & 0x8000) { Mods |= MOD_Alt; }
    if(GetKeyState(VK_SHIFT) & 0x8000) { Mods |= MOD_Shift; }
    return Mods;
}

void
win32_handle_window_message(MSG *Message, HWND *WindowHandle, input_state *InputState) {
    switch(Message->message) {
        case WM_CHAR: {
            // TODO: Surrogate pairs don't come packed into wParam as one 32-bit value, they are separated into two 16-bit WM_CHARs messages.
            key_event Key = {};
            Key.IsText = true;
            Key.Modifiers = win32_get_modifiers();
            
            utf16_to_utf8((u32)Message->wParam, Key.Char);
            
            if(Key.Char[0] < 0x1f) {
                return;
            }
            
            if(InputState->EventCount < KEY_EVENT_MAX) {
                InputState->Events[InputState->EventCount++] = Key;
            }
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
            key_event Key = {0};
            Key.Key = Message->wParam < KEY_Max ? (key)Message->wParam : KEY_Invalid;
            
            // Special case where our enum values don't match win32's virtual key-codes.
            // Mainly because our enum values are ASCII.
            if(Message->wParam == VK_DELETE) {
                Key.Key = KEY_Delete;
            }
            
            Key.Modifiers = win32_get_modifiers();
            
            if(InputState->EventCount < KEY_EVENT_MAX) {
                InputState->Events[InputState->EventCount++] = Key;
            }
        } break;
        
        case WM_RBUTTONDOWN: { 
            InputState->MousePress |= MOUSE_Right; 
            InputState->MouseDown  |= MOUSE_Right; 
        } break;
        
        case WM_LBUTTONDOWN: { 
            InputState->MousePress |= MOUSE_Left; 
            InputState->MouseDown  |= MOUSE_Left;
        } break;
        
        case WM_RBUTTONUP: { InputState->MouseDown &= (~MOUSE_Right); } break;
        case WM_LBUTTONUP: { InputState->MouseDown &= (~MOUSE_Left);  } break;
        
        case WM_XBUTTONDOWN: { 
            auto Forward = (GET_XBUTTON_WPARAM(Message->wParam) == 1);
            InputState->MousePress |=  Forward ? MOUSE_Forward : MOUSE_Backward;
        } break;
        
        case WM_XBUTTONUP: { 
            auto Forward = GET_XBUTTON_WPARAM(Message->wParam) == 1;
            InputState->MouseDown &= ~(Forward ? MOUSE_Forward : MOUSE_Backward);
        } break;
        
        case WM_MOUSEWHEEL: {
            s32 Delta = GET_WHEEL_DELTA_WPARAM(Message->wParam);
            InputState->Scroll += Delta / WHEEL_DELTA;
        } break;
        
    }
}

int
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCommand) {
    WNDCLASSW WindowClass = {0};
    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc = win32_window_callback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.lpszClassName = L"window_class";
    RegisterClassW(&WindowClass);
    
    HWND WindowHandle = CreateWindowExW(0, WindowClass.lpszClassName, L"kalam", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 820, 380, 900, 900, 0, 0, Instance, 0);
    if(WindowHandle) {
        b32 IsUnicode = IsWindowUnicode(WindowHandle);
        ASSERT(IsUnicode);
        
        {
            RECT ClientRect;
            GetClientRect(WindowHandle, &ClientRect);
            s32 Width = (s32)(ClientRect.right - ClientRect.left);
            s32 Height = (s32)(ClientRect.bottom - ClientRect.top);
            win32_resize_framebuffer(&gFramebuffer, &gBitmapInfo, Width, Height);
        }
        
        init_platform_api();
        
        LARGE_INTEGER CounterStart, CounterEnd;
        QueryPerformanceCounter(&CounterStart);
        
        s64 Frequency;
        {
            LARGE_INTEGER F;
            QueryPerformanceFrequency(&F);
            Frequency = F.QuadPart;
        }
        
        f64 TargetFps = 30;
        f64 FrameBudget = (1. / TargetFps);
        f64 FrameDt = FrameBudget;  // Inital value that isn't 0
        f64 TotalFrameTimeElapsed = FrameBudget;
        f64 SecondTimer = 0;
        u32 Fps = 0;
        
        input_state InputState = {};
        kalam_ctx KalamCtx = {};
        
        kalam_init();
        bool Running = true;
        while(Running && !gWindowWasClosed) {
            reset_input_state(&InputState);
            
            MSG Message = {0};
            while(PeekMessageW(&Message, WindowHandle, 0, 0, PM_REMOVE)) {
                win32_handle_window_message(&Message, &WindowHandle, &InputState);
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
            }
            
            InputState.ScrollModifiers = win32_get_modifiers();
            
            POINT CursorPoint;
            GetCursorPos(&CursorPoint);
            ScreenToClient(WindowHandle, &CursorPoint);
            InputState.LastMousePos = InputState.MousePos;
            InputState.MousePos = {CursorPoint.x, CursorPoint.y};
            
            kalam_update_and_render(&InputState, &gFramebuffer, (f32)TotalFrameTimeElapsed);
            
            HDC Dc = GetDC(WindowHandle);
            StretchDIBits(Dc, 0, 0, gFramebuffer.Width, gFramebuffer.Height, 0, 0, gFramebuffer.Width, gFramebuffer.Height, gFramebuffer.Pixels, &gBitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            ReleaseDC(WindowHandle, Dc);
            
            
            QueryPerformanceCounter(&CounterEnd);
            s64 CounterElapsed = CounterEnd.QuadPart - CounterStart.QuadPart;
            FrameDt = (f64)CounterElapsed / Frequency;
            
            // Sleep for remainder of frame budget
            f64 SleepTime = 0;
            if(FrameDt < FrameBudget) {
                SleepTime = FrameBudget - FrameDt;
                Sleep((DWORD)(SleepTime * 1000));
            }
            
            TotalFrameTimeElapsed = FrameDt + SleepTime;
            
            static u32 FpsDisplayed = 0;
            SecondTimer += TotalFrameTimeElapsed;
            Fps++;
            
            char WindowTitle[256];
            sprintf_s(WindowTitle, "Kalam | FPS %d | Dt %0.2fms | Total frame time %0.2fms | x:%d, y:%d", FpsDisplayed, FrameDt*1000, TotalFrameTimeElapsed * 1000, InputState.MousePos.x, InputState.MousePos.y);
            SetWindowTextA(WindowHandle, WindowTitle);
            
            if(SecondTimer >= 1.) {
                FpsDisplayed = Fps;
                Fps = 0;
                SecondTimer = 0;
            }
            
            // Query performance counter after sleep to not include this frame's sleep in the next frame's timings.
            QueryPerformanceCounter(&CounterStart);
        }
    } else {
        // TODO: Diagnostics
        DWORD Error = GetLastError();
        char Buf[1024];
        snprintf(Buf, ARRAY_COUNT(Buf), "CreateWindowExW failed with error: %d\n See https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes", (s32)Error);
        OutputDebugStringA(Buf);
    }
    
}
