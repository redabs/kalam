#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

#include "intrinsics.h"
#include "types.h"
#include "event.h"
#include "platform.h"
#include "kalam.h"

#include "render.c"
#include "font.c"

ctx_t Ctx = {0};

u8
utf8_char_width(u8 *Char) {
    switch(*Char & 0xf0) {
        case 0xf0: { return 4; } break;
        case 0xe0: { return 3; } break;
        case 0xd0:
        case 0xc0: { return 2; } break;
        default:   { return 1; } break;
    }
}

void
k_init(platform_shared_t *Shared) {
    Ctx.Font = load_ttf("fonts/consola.ttf", 15);
    
}

typedef enum {
    UP    = 1,
    DOWN  = 1 << 1,
    LEFT  = 1 << 2,
    RIGHT = 1 << 3
} dir;

void
cursor_move(text_buffer_t *Buf, dir Dir) {
    if(Dir & (UP | LEFT) && Buf->Cursor.ByteOff == 0 || 
       Dir & (RIGHT | DOWN) && Buf->Cursor.ByteOff >= Buf->Size) {
        return;
    }
    
    // TODO:
    switch(Dir) {
        case UP: {
        } break;
        
        case DOWN: {
        } break;
        
        case LEFT: {
        } break;
        
        case RIGHT: {
        } break;
    }
    
}

void
do_char(u8 *Char) {
    u8 n = utf8_char_width(Char);
    
    // TODO: tab, backspace 
    text_buffer_t *b = &Ctx.Buffer;
    if((b->Size + n) > b->Capacity) {
        u64 NewCapacity = b->Capacity + KILOBYTES(4);
        b->Data = realloc(b->Data, NewCapacity);
        b->Capacity = NewCapacity;
    }
    
    // Move all bytes after cursor forward n bytes
    for(s64 i = b->Size - 1; i >= b->Cursor.ByteOff; i--) {
        b->Data[i + n] = b->Data[i];
    }
    
    for(u8 i = 0; i < n; i++) {
        b->Data[b->Cursor.ByteOff + i] = Char[i];
    }
    b->Size += n;
    
    cursor_move(b, RIGHT);
}


#if 0
void
move_back(text_buffer_t *Buffer) { 
    if(Buffer->Cursor.ByteOff == 0) {
        return;
    }
    
    // Scan one character back
    u8 *c = Buffer->Data + Buffer->Cursor.ByteOff - 1;
    u64 i = Buffer->Cursor.ByteOff - 1;
    b32 Found = false;
    
    while(i >= 0) {
        if((*c & 0xc0) != 0x80) {
            Found = true;
            break;
        }
        --c;
        --i;
    }
    Buffer->Cursor.ByteOff = i;
    
    u32 Codepoint;
    utf8_to_codepoint(c, &Codepoint);
    if(Codepoint == '\n') {
        --Buffer->Cursor.Line;
    } else {
    }
}

void
move_forward(text_buffer_t *Buffer) {
    if(Buffer->Cursor.ByteOff < Buffer->Size) {
        Buffer->Cursor.ByteOff += utf8_char_width(Buffer->Data + Buffer->Cursor.ByteOff);
        ++Buffer->Cursor.CharOff;
    }
}
#endif

void
draw_buffer(framebuffer_t *Fb, font_t *Font, text_buffer_t *Buf) {
    s32 LineHeight = Font->Ascent - Font->Descent; 
    s32 LineIndex = 0;
    u8 *c = Buf->Data;
    u8 *End = Buf->Data + Buf->Size;
    s32 CursorX = 0;
    while(c < End) {
        s32 LineY = LineIndex * LineHeight;
        s32 Center = LineY + (LineHeight >> 1);
        s32 Baseline = Center + (Font->MHeight >> 1);
        
        u32 Codepoint;
        c = utf8_to_codepoint(c, &Codepoint);
        glyph_set_t *Set;
        if(get_glyph_set(Font, Codepoint, &Set)) {
            stbtt_bakedchar *g = &Set->Glyphs[Codepoint];
            irect_t gRect = {g->x0, g->y0, g->x1 - g->x0, g->y1 - g->y0};
            s32 yOff = (s32)(g->yoff + 0.5);
            draw_glyph_bitmap(Fb, CursorX, Baseline + yOff, gRect, &Set->Bitmap);
            
            if(Codepoint == '\n') {
                ++LineIndex;
                CursorX = 0;
            } else {
                CursorX += (s32)(g->xadvance + 0.5);
            }
        }
        
    }
    
}

void
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    for(s32 i = 0; i < Events->Count; ++i) {
        input_event_t *e = &Events->Events[i];
        if(e->Type == INPUT_EVENT_Press && e->Device == INPUT_DEVICE_Keyboard) {
            switch(e->Key.KeyCode) {
                case KEY_Left: {
                    cursor_move(&Ctx.Buffer, LEFT);
                } break;
                case KEY_Right: {
                    cursor_move(&Ctx.Buffer, RIGHT);
                } break;
                case KEY_Up: {
                    cursor_move(&Ctx.Buffer, UP);
                } break;
                case KEY_Down: {
                    cursor_move(&Ctx.Buffer, DOWN);
                } break;
                
                default: {
                    if(e->Key.HasCharacterTranslation) {
                        do_char(e->Key.Character);
                    }
                } break;
            }
            
        } else if(e->Type == INPUT_EVENT_Scroll) {
            
        }
    } Events->Count = 0;
    
    clear_framebuffer(Shared->Framebuffer, 0xff110f58);
#if 0
    {
        font_t *Font = &Ctx.Font;
        s32 LineHeight = Font->Ascent - Font->Descent; 
        s32 FbWidth = Shared->Framebuffer->Width;
        for(s32 i = 0; i < 25; ++i) {
            s32 LineY = i * LineHeight;
            s32 Center = LineY + LineHeight / 2;
            //draw_rect(Shared->Framebuffer, (irect_t){0, LineY, FbWidth, LineHeight}, (i&1) ? 0xffaaaaaa : 0xff444444);
            //draw_rect(Shared->Framebuffer, (irect_t){0, Center - 1, FbWidth, 2}, 0xff333333);
            
            s32 CursorX = 0;
            s32 Baseline = Center + Font->MHeight / 2; //LineY + LineHeight;
            
            char *TestString = "The quick brown fucker yeeted the lazy zoomer";
            for(char *c = TestString; *c; ++c) {
                glyph_set_t *Set;
                s32 Advance = 32;
                if(get_glyph_set(Font, *c, &Set)) {
                    stbtt_bakedchar *g = &Set->Glyphs[*c];
                    bitmap_t *b = &Set->Bitmap;
                    
                    irect_t gRect = {.x = g->x0, .y = g->y0, .w = g->x1 - g->x0, .h = g->y1 - g->y0};
                    s32 yOff = (s32)(g->yoff + 0.5);
                    draw_glyph_bitmap(Shared->Framebuffer, CursorX, Baseline + yOff - 1, gRect, b);
                    
                    Advance = (s32)(g->xadvance + 0.5);
                }
                CursorX += Advance;
            }
        }
    }
#endif
    draw_buffer(Shared->Framebuffer, &Ctx.Font, &Ctx.Buffer);
    
    for(u32 i = 0, x = 0; i < Ctx.Font.SetCount; ++i) {
        bitmap_t *b = &Ctx.Font.GlyphSets[i].Set->Bitmap;
        irect_t Rect = {0, 0, b->w, b->h};
        draw_glyph_bitmap(Shared->Framebuffer, x, Shared->Framebuffer->Height - Rect.h, Rect, b);
        x += b->w + 10;
    }
}