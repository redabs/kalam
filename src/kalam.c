#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

#include "intrinsics.h"
#include "memory.h"
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

typedef enum {
    UP    = 1,
    DOWN  = 1 << 1,
    LEFT  = 1 << 2,
    RIGHT = 1 << 3
} dir;

void
cursor_move(buffer_t *Buf, dir Dir) {
    switch(Dir) {
        case UP: {
            if(Buf->Cursor.Line > 0) {
                line_t *PreviousLine = (line_t *)Buf->Lines.Data + Buf->Cursor.Line - 1;
                Buf->Cursor.ByteOffset = PreviousLine->ByteOffset;
                Buf->Cursor.ColumnIs = MIN(Buf->Cursor.ColumnWas, PreviousLine->ColumnCount - 1);
                Buf->Cursor.Line -= 1;
            }
        } break;
        
        case DOWN: {
            if((Buf->Cursor.Line + 1) < Buf->Lines.Used) {
                line_t *NextLine = (line_t *)Buf->Lines.Data + Buf->Cursor.Line + 1;
                Buf->Cursor.ByteOffset = NextLine->ByteOffset;
                Buf->Cursor.ColumnIs = MIN(Buf->Cursor.ColumnWas, NextLine->ColumnCount - 1);
                Buf->Cursor.Line += 1;
            }
        } break;
        
        case LEFT: {
            if(Buf->Cursor.ByteOffset > 0) {
                if(Buf->Cursor.ColumnIs == 0) {
                    line_t *PreviousLine = (line_t *)Buf->Lines.Data + Buf->Cursor.Line - 1;
                    Buf->Cursor.ByteOffset = PreviousLine->ByteOffset + PreviousLine->Size - 1;
                    Buf->Cursor.ColumnIs = PreviousLine->ColumnCount - 1;
                    Buf->Cursor.ColumnWas = PreviousLine->ColumnCount - 1;
                    Buf->Cursor.Line -= 1;
                } else {
                    Buf->Cursor.ColumnIs -= 1;
                    Buf->Cursor.ColumnWas -= 1;
                    // Scan one character back
                    while(Buf->Cursor.ByteOffset > 0 && (Buf->Data[Buf->Cursor.ByteOffset] & 0xc0) == 0x80) {
                        Buf->Cursor.ByteOffset -= 1;
                    }
                }
            }
            
        } break;
        
        case RIGHT: {
            if(Buf->Cursor.ByteOffset < Buf->Used) {
                line_t *CurrentLine = (line_t *)Buf->Lines.Data + Buf->Cursor.Line;
                if((Buf->Cursor.ColumnIs + 1) > (CurrentLine->ColumnCount - 1)) {
                    line_t *NextLine = (line_t *)Buf->Lines.Data + Buf->Cursor.Line + 1;
                    Buf->Cursor.ByteOffset = NextLine->ByteOffset;
                    Buf->Cursor.ColumnIs = 0;
                    Buf->Cursor.ColumnWas = 0;
                    Buf->Cursor.Line += 1;
                } else {
                    Buf->Cursor.ByteOffset += utf8_char_width(Buf->Data + Buf->Cursor.ByteOffset);
                    Buf->Cursor.ColumnIs += 1;
                    Buf->Cursor.ColumnWas += 1;
                }
            }
        } break;
    }
}

void
do_char(u8 *Char) {
#if 0
    u8 n = utf8_char_width(Char);
    
    // TODO: tab, backspace 
    buffer_t *b = &Ctx.Buffer;
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
#endif
}

void
draw_buffer(framebuffer_t *Fb, font_t *Font, buffer_t *Buf) {
    s32 LineHeight = Font->Ascent - Font->Descent; 
    {
        u32 Codepoint = 'M';
        s32 x = Font->MWidth * (s32)Buf->Cursor.ColumnIs;
        s32 y = LineHeight * (s32)Buf->Cursor.Line;
        draw_rect(Fb, (irect_t){.x = x, .y = y, .w = Font->MWidth, .h = LineHeight}, 0xff117766);
    }
    s32 LineIndex = 0;
    u8 *c = Buf->Data;
    u8 *End = Buf->Data + Buf->Used;
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
load_file(char *Path, buffer_t *Dest) {
    mem_zero_struct(Dest);
    {
        // TODO: Think a little harder about how files are loaded
        platform_file_data_t File;
        ASSERT(platform_read_file(Path, &File));
        
        u64 Capacity = (File.Size + 4096) & ~(4096 - 1);
        Dest->Data = malloc(Capacity);
        Dest->Used = File.Size;
        Dest->Capacity = Capacity;
        mem_copy(Dest->Data, File.Data, File.Size);
        
        platform_free_file(&File);
        
    }
    
    // TODO: Detect file encoding, we assume utf-8 for now
    line_t Line = {0};
    u8 n = 0;
    for(u64 i = 0; i < Dest->Used; i += n) {
        n = utf8_char_width(Dest->Data + i);
        Line.Size += n;
        Line.ColumnCount += 1;
        
        if(Dest->Data[i] == '\n') {
            line_t *l = mem_buf_push_struct(&Dest->Lines, line_t);
            *l = Line;
            mem_zero_struct(&Line);
            Line.ByteOffset = i + n;
        }
    }
}

void
k_init(platform_shared_t *Shared) {
    Ctx.Font = load_ttf("fonts/consola.ttf", 15);
    load_file("test.c", &Ctx.Buffer);
    
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


