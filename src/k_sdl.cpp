#include "kalam.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL3/SDL.h>

#include "k_memory.cpp"

platform_api gPlatform;

PLATFORM_ALLOC(mem_alloc) {
    void *Result = malloc(Size); // TODO: mmap?
    return Result;
}

PLATFORM_FREE(mem_free) {
    free(Ptr);
}

PLATFORM_READ_FILE(read_file) {
    buffer<u8> Result = {};
    int FileDes = open((const char *)Path.Ptr, O_RDONLY);
    if(FileDes != -1) {
        struct stat FileStat = {};
        int StatRes = fstat(FileDes, &FileStat);
        if(StatRes == 0) {
            u64 DestIdx = add(&Result, (u64)FileStat.st_size);
            read(FileDes, Result.Ptr + DestIdx, Result.Capacity);
        } else {
            fprintf(stderr, "Failed to fstat file %s. errno: %d, \"%s\"\n", Path.Ptr, errno, strerror(errno));
            ASSERT(false);
        }
        close(FileDes);
    } else {
        fprintf(stderr, "Failed to open file %s, errno:%d\n", Path.Ptr, errno);
        ASSERT(false);
    }
    return Result;
}

void
init_platform_api() {
    gPlatform.alloc = mem_alloc;
    gPlatform.free = mem_free;
    gPlatform.read_file = read_file;
}

void
resize_framebuffer(framebuffer *Fb, s32 Width, s32 Height) {
    if(Fb->Pixels) {
        gPlatform.free(Fb->Pixels);
        Fb->Pixels = 0;
    }

    Fb->Width = Width;
    Fb->Height = Height;
    Fb->Pixels = (u32 *)gPlatform.alloc(Fb->Width * Fb->Height * 4);
}

modifier
sdl_get_modifiers() {
    modifier Mods = (modifier)0;
    SDL_Keymod SDLMods = SDL_GetModState();
    if(SDLMods & SDL_KMOD_CTRL) { Mods |= MOD_Ctrl; }
    if((SDLMods & (SDL_KMOD_ALT | SDL_KMOD_GUI))) { Mods |= MOD_Alt; }
    if(SDLMods & SDL_KMOD_SHIFT) { Mods |= MOD_Shift; }
    return Mods;
}

key
sdl_keycode_to_key(SDL_Keycode code) {
    switch(code) {
        case SDLK_UNKNOWN:   { return KEY_Unsupported; }
        case SDLK_BACKSPACE: { return KEY_Backspace; }
        case SDLK_TAB:       { return KEY_Tab; }
        case SDLK_RETURN:    { return KEY_Return; }
        case SDLK_LSHIFT:    { return KEY_Shift; }
        case SDLK_RSHIFT:    { return KEY_Shift; }
        case SDLK_LCTRL:     { return KEY_Ctrl; }
        case SDLK_RCTRL:     { return KEY_Ctrl; }
        case SDLK_LALT:      { return KEY_Alt; }
        case SDLK_RALT:      { return KEY_Alt; }
        case SDLK_CAPSLOCK:  { return KEY_CapsLock; }
        case SDLK_ESCAPE:    { return KEY_Escape; }
        case SDLK_SPACE:     { return KEY_Space; }
        case SDLK_PAGEUP:    { return KEY_PageUp; }
        case SDLK_PAGEDOWN:  { return KEY_PageDown; }
        case SDLK_END:       { return KEY_End; }
        case SDLK_HOME:      { return KEY_Home; }
        case SDLK_LEFT:      { return KEY_Left; }
        case SDLK_UP:        { return KEY_Up; }
        case SDLK_RIGHT:     { return KEY_Right; }
        case SDLK_DOWN:      { return KEY_Down; }
        case SDLK_INSERT:    { return KEY_Insert; }
        case SDLK_0:         { return KEY_0; }
        case SDLK_1:         { return KEY_1; }
        case SDLK_2:         { return KEY_2; }
        case SDLK_3:         { return KEY_3; }
        case SDLK_4:         { return KEY_4; }
        case SDLK_5:         { return KEY_5; }
        case SDLK_6:         { return KEY_6; }
        case SDLK_7:         { return KEY_7; }
        case SDLK_8:         { return KEY_8; }
        case SDLK_9:         { return KEY_9; }
        case SDLK_A:         { return KEY_A; }
        case SDLK_B:         { return KEY_B; }
        case SDLK_C:         { return KEY_C; }
        case SDLK_D:         { return KEY_D; }
        case SDLK_E:         { return KEY_E; }
        case SDLK_F:         { return KEY_F; }
        case SDLK_G:         { return KEY_G; }
        case SDLK_H:         { return KEY_H; }
        case SDLK_I:         { return KEY_I; }
        case SDLK_J:         { return KEY_J; }
        case SDLK_K:         { return KEY_K; }
        case SDLK_L:         { return KEY_L; }
        case SDLK_M:         { return KEY_M; }
        case SDLK_N:         { return KEY_N; }
        case SDLK_O:         { return KEY_O; }
        case SDLK_P:         { return KEY_P; }
        case SDLK_Q:         { return KEY_Q; }
        case SDLK_R:         { return KEY_R; }
        case SDLK_S:         { return KEY_S; }
        case SDLK_T:         { return KEY_T; }
        case SDLK_U:         { return KEY_U; }
        case SDLK_V:         { return KEY_V; }
        case SDLK_W:         { return KEY_W; }
        case SDLK_X:         { return KEY_X; }
        case SDLK_Y:         { return KEY_Y; }
        case SDLK_Z:         { return KEY_Z; }
        case SDLK_DELETE:    { return KEY_Delete; }
        default:             { return KEY_Unsupported; }
    }
}

mouse
sdl_button_to_button(u8 SdlButton) {
    mouse Button = 0;
    switch(SdlButton) {
        case SDL_BUTTON_RIGHT: Button = MOUSE_Right; break;
        case SDL_BUTTON_LEFT: Button = MOUSE_Left; break;
        case SDL_BUTTON_MIDDLE: Button = MOUSE_Middle; break;
        case SDL_BUTTON_X1: Button = MOUSE_Backward; break;
        case SDL_BUTTON_X2: Button = MOUSE_Forward; break;
    }
    return Button;
}


int
main() {
    init_platform_api();

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *Window = SDL_CreateWindow("Kalam", 900, 900, SDL_WINDOW_RESIZABLE);
    if(Window) {
        SDL_Renderer *Renderer = SDL_CreateRenderer(Window, NULL);
        if(!Renderer) {
            return -1;
        }

        input_state InputState = {};
        framebuffer Framebuffer = {};
        SDL_Texture *Texture;
        {
            s32 Width = 0, Height = 0;
            ASSERT(SDL_GetCurrentRenderOutputSize(Renderer, &Width, &Height));
            Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STATIC, Width, Height);
            if(Texture == NULL) {
                fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
                return -1;
            }
            resize_framebuffer(&Framebuffer, Width, Height);
        }

        kalam_init();

        ASSERT(SDL_StartTextInput(Window));

        b32 Running = true;
        while(Running) {
            reset_input_state(&InputState);

            SDL_Event Event = {};
            while(SDL_PollEvent(&Event)) {
                switch(Event.type) {
                    case SDL_EVENT_QUIT: {
                        Running = false;
                    } break;

                    case SDL_EVENT_TEXT_INPUT: {
                        u8 n = 1;
                        for(u8 *c = (u8*)Event.text.text; *c; c += n) {
                            n = utf8_char_size(*Event.text.text);
                            key_event Key = {0};
                            Key.IsText = true;
                            Key.Modifiers = sdl_get_modifiers();
                            switch(n) {
                                case 4: Key.Char[3] = c[3];
                                case 3: Key.Char[2] = c[2];
                                case 2: Key.Char[1] = c[1];
                                case 1: Key.Char[0] = c[0];
                            }

                            if(InputState.EventCount < KEY_EVENT_MAX) {
                                InputState.Events[InputState.EventCount++] = Key;
                            }
                        }
                    } break;

                    case SDL_EVENT_MOUSE_WHEEL: {
                        InputState.Scroll += Event.wheel.y * (Event.wheel.direction == SDL_MOUSEWHEEL_NORMAL ? 1 : -1);
                        InputState.ScrollModifiers = sdl_get_modifiers();
                    } break;

                    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                        mouse Button = sdl_button_to_button(Event.button.button);
                        InputState.MousePress |= Button;
                        InputState.MouseDown |= Button;
                    } break;

                    case SDL_EVENT_MOUSE_BUTTON_UP: {
                        mouse Button = sdl_button_to_button(Event.button.button);
                        InputState.MouseDown &= (~Button);
                    } break;

                    case SDL_EVENT_KEY_DOWN: {
                        key_event Key = {0};
                        Key.Key = sdl_keycode_to_key(Event.key.key);
                        Key.Modifiers = sdl_get_modifiers();

                        if(InputState.EventCount < KEY_EVENT_MAX) {
                            InputState.Events[InputState.EventCount++] = Key;
                        }
                    } break;

                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    case SDL_EVENT_WINDOW_RESIZED: {
                        resize_framebuffer(&Framebuffer, Event.window.data1, Event.window.data2);
                        printf("Window resized: %d, %d\n", Event.window.data1, Event.window.data2);
                        SDL_DestroyTexture(Texture);
                        Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_BGRX32, SDL_TEXTUREACCESS_STATIC, Framebuffer.Width, Framebuffer.Height);
                    } break;
                }
            }

            kalam_update_and_render(&InputState, &Framebuffer, 1/60.);

            if(!SDL_UpdateTexture(Texture, 0, Framebuffer.Pixels, Framebuffer.Width * 4)) {
                fprintf(stderr, "SDL_UpdateTexture: Failed to update texture with framebuffer pixels: %s\n", SDL_GetError());
                return -1;
            }

            if(!SDL_RenderTexture(Renderer, Texture, NULL, NULL)) {
                fprintf(stderr, "SDL_RenderTexture: Failed to render texture to render target: %s\n", SDL_GetError());
                return -1;
            }
            if(!SDL_RenderPresent(Renderer)) {
                fprintf(stderr, "SDL_RenderPreset: Failed to flip screen: %s\n", SDL_GetError());
                return -1;
            }
        }
    } else {
        fprintf(stderr, "SDL_CreateWindow failed with: %s\n", SDL_GetError());
        return -1;
    }

}
