#include "kalam.h"

#include <SDL2/SDL.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

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
            u64 DestIdx = add_index(&Result, (u64)FileStat.st_size);
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

int
main() {
    init_platform_api();

    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return -1;
    }
    
    SDL_Window *Window = SDL_CreateWindow("Kalam", 0, 0, 900, 900, SDL_WINDOW_RESIZABLE);
    if(Window) {
        SDL_Renderer *Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_SOFTWARE);
        if(!Renderer) {
            return -1;
        }

        input_state InputState = {};
        framebuffer Framebuffer = {};
        SDL_Texture *Texture;
        {
            s32 Width = 0, Height = 0;
            ASSERT(SDL_GetRendererOutputSize(Renderer, &Width, &Height) == 0);
            Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STATIC, Width, Height);
            resize_framebuffer(&Framebuffer, Width, Height);
        }

        


        kalam_init();

        b32 Running = true;
        while(Running) {
            reset_input_state(&InputState);

            SDL_Event Event = {};
            while(SDL_PollEvent(&Event)) {
                switch(Event.type) {
                    case SDL_QUIT: {
                        Running = false;
                    } break;
                    
                    case SDL_KEYDOWN:
                    case SDL_KEYUP: {
                        
                    } break;

                    case SDL_WINDOWEVENT: {
                        switch(Event.window.event) {
                            case SDL_WINDOWEVENT_RESIZED:
                            case SDL_WINDOWEVENT_SIZE_CHANGED: {
                                resize_framebuffer(&Framebuffer, Event.window.data1, Event.window.data2);
                                printf("Window resized: %d, %d\n", Event.window.data1, Event.window.data2);
                                SDL_DestroyTexture(Texture);
                                Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STATIC, Framebuffer.Width, Framebuffer.Height);
                            } break;
                        }
                    } break;
                }
            }

            kalam_update_and_render(&InputState, &Framebuffer, 1/60.);

            SDL_UpdateTexture(Texture, 0, Framebuffer.Pixels, Framebuffer.Width * 4);

            SDL_RenderCopy(Renderer, Texture, 0, 0);
            SDL_RenderPresent(Renderer);
        }
    } else {
        fprintf(stderr, "SDL_CreateWindow failed with: %s\n", SDL_GetError());
        return -1;
    }
    
}
