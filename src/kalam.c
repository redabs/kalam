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
    
    // TODO: \r\n line endings
    switch(Dir) {
        case UP: {
        } break;
        
        case DOWN: {
        } break;
        
        case LEFT: {
            // Move one character back
            u8 *c = Buf->Data + Buf->Cursor.ByteOff - 1;
            u64 ByteOff = Buf->Cursor.ByteOff - 1;
            while(ByteOff > 0 &&  (*c & 0xc0) == 0x80)  {
                --c;
                --ByteOff;
            }
            Buf->Cursor.ByteOff = ByteOff;
            
            if(Buf->Cursor.ColumnIs - 1 < 0) {
                // Find out what column we ended up on when we jumped up one line.
                --Buf->Cursor.Line;
                s64 Column = 0;
                s64 i = Buf->Cursor.ByteOff - 1;
                while(Buf->Data[i] != '\n' && i > 0) {
                    --i;
                    ++Column;
                }
                
                Buf->Cursor.ColumnIs = Column;
                Buf->Cursor.ColumnWas = Column;
            } else {
                --Buf->Cursor.ColumnIs;
                --Buf->Cursor.ColumnWas;
            }
            
        } break;
        
        case RIGHT: {
            u8 *c = Buf->Data + Buf->Cursor.ByteOff;
            if(*c == '\n') {
                ++Buf->Cursor.Line;
                Buf->Cursor.ColumnIs = 0;
                Buf->Cursor.ColumnWas = 0;
            } else {
                ++Buf->Cursor.ColumnIs;
                ++Buf->Cursor.ColumnWas;
            }
            u8 n = utf8_char_width(c);
            Buf->Cursor.ByteOff += n;;
            
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
    {
        u32 Codepoint = 'M';
        s32 x = Font->MWidth * (s32)Buf->Cursor.ColumnIs;
        s32 y = LineHeight * (s32)Buf->Cursor.Line;
        draw_rect(Fb, (irect_t){.x = x, .y = y, .w = Font->MWidth, .h = LineHeight}, 0xff117766);
    }
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
                        do_char(e->Key.Character[0] == '\r' ? (u8 *)"\n" : e->Key.Character);
                    }
                } break;
            }
            
        } else if(e->Type == INPUT_EVENT_Scroll) {
            
        }
    } Events->Count = 0;
    
    clear_framebuffer(Shared->Framebuffer, 0xff110f58);
    
    draw_buffer(Shared->Framebuffer, &Ctx.Font, &Ctx.Buffer);
    
    for(u32 i = 0, x = 0; i < Ctx.Font.SetCount; ++i) {
        bitmap_t *b = &Ctx.Font.GlyphSets[i].Set->Bitmap;
        irect_t Rect = {0, 0, b->w, b->h};
        draw_glyph_bitmap(Shared->Framebuffer, x, Shared->Framebuffer->Height - Rect.h, Rect, b);
        x += b->w + 10;
    }
}


