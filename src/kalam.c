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
cursor_move(buffer_t *Buf, dir Dir, s64 StepSize) {
    int LineEndChars = 0; // TODO: CRLF, LF, LFCR, .....
    s64 Column = 0;
    switch(Dir) {
        case DOWN: 
        case UP: {
            s64 LineCount = mem_buf_count(&Buf->Lines, line_t);
            if((Dir == UP && Buf->Cursor.Line > 0) ||
               (Dir == DOWN && Buf->Cursor.Line < LineCount)) {
                s64 LineNum = CLAMP(0, LineCount - 1, Buf->Cursor.Line + ((Dir == DOWN) ? StepSize : -StepSize));
                line_t *Line = (line_t *)Buf->Lines.Data + LineNum;
                Column = MIN(Buf->Cursor.ColumnWas, Line->ColumnCount - 1);
                
                Buf->Cursor.Line = LineNum;
                Buf->Cursor.ColumnIs = Column;
                Buf->Cursor.ColumnWas = Buf->Cursor.ColumnIs;
                Buf->Cursor.ByteOffset = Line->ByteOffset;
                for(s64 i = 0; i < Buf->Cursor.ColumnIs; ++i) {
                    Buf->Cursor.ByteOffset += utf8_char_width(Buf->Data + Buf->Cursor.ByteOffset);
                }
            }
        } break;
        
        case RIGHT: {
            s64 LineNum = Buf->Cursor.Line;
            line_t *Line = (line_t *)Buf->Lines.Data + LineNum;
            Column = Buf->Cursor.ColumnIs;
            s64 LineCount = mem_buf_count(&Buf->Lines, line_t);
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column + 1 >= Line->ColumnCount) {
                    if(LineNum + 1 >= LineCount) {
                        Column = Line->ColumnCount;
                        break;
                    }
                    LineNum++;
                    Line++;
                    Column = 0;
                } else {
                    Column++;
                }
            }
            
            Buf->Cursor.Line = LineNum;
            Buf->Cursor.ColumnIs = Column;
            Buf->Cursor.ColumnWas = Buf->Cursor.ColumnIs;
            Buf->Cursor.ByteOffset = Line->ByteOffset;
            for(s64 i = 0; i < Buf->Cursor.ColumnIs; ++i) {
                Buf->Cursor.ByteOffset += utf8_char_width(Buf->Data + Buf->Cursor.ByteOffset);
            }
        } break;
        
        case LEFT: {
            s64 LineNum = Buf->Cursor.Line;
            line_t *Line = (line_t *)Buf->Lines.Data + LineNum;
            Column = Buf->Cursor.ColumnIs;
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column - 1 < 0) {
                    if(LineNum - 1 < 0) {
                        Column = 0;
                        break;
                    }
                    LineNum--;
                    Line--;
                    Column = Line->ColumnCount - 1;
                } else {
                    Column--;
                }
            }
            
            Buf->Cursor.Line = LineNum;
            Buf->Cursor.ColumnIs = Column;
            Buf->Cursor.ColumnWas = Buf->Cursor.ColumnIs;
            Buf->Cursor.ByteOffset = Line->ByteOffset;
            for(s64 i = 0; i < Buf->Cursor.ColumnIs; ++i) {
                Buf->Cursor.ByteOffset += utf8_char_width(Buf->Data + Buf->Cursor.ByteOffset);
            }
        } break;
        
    }
}

void
do_char(u8 *Char) {
    u8 n = utf8_char_width(Char);
    // TODO: keep lines up to date
    //cursor_move(b, RIGHT);
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
        
        if(Dest->Data[i] == '\n' || (i + n) >= Dest->Used) {
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
            s32 StepSize = (e->Modifiers & INPUT_MOD_Control) ? 5 : 1;
            switch(e->Key.KeyCode) {
                case KEY_Left: {
                    cursor_move(&Ctx.Buffer, LEFT, StepSize);
                } break;
                case KEY_Right: {
                    cursor_move(&Ctx.Buffer, RIGHT, StepSize);
                } break;
                case KEY_Up: {
                    cursor_move(&Ctx.Buffer, UP, StepSize);
                } break;
                case KEY_Down: {
                    cursor_move(&Ctx.Buffer, DOWN, StepSize);
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