#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "memory.h"
#include "types.h"
#include "event.h"
#include "platform.h"

#include "kalam.h"

#include "render.c"

ctx_t Ctx = {0};

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
            if((Dir == UP && Buf->Cursor.Line > 0) ||
               (Dir == DOWN && Buf->Cursor.Line < (sb_count(Buf->Lines) - 1))) {
                s64 LineNum = CLAMP(0, sb_count(Buf->Lines) - 1, Buf->Cursor.Line + ((Dir == DOWN) ? StepSize : -StepSize));
                Column = MIN(Buf->Cursor.ColumnWas, Buf->Lines[LineNum].ColumnCount - 1);
                
                Buf->Cursor.Line = LineNum;
                Buf->Cursor.ColumnIs = Column;
                Buf->Cursor.ByteOffset = Buf->Lines[LineNum].ByteOffset;
                for(s64 i = 0; i < Buf->Cursor.ColumnIs; ++i) {
                    Buf->Cursor.ByteOffset += utf8_char_width(Buf->Data + Buf->Cursor.ByteOffset);
                }
            }
        } break;
        
        case RIGHT: {
            s64 LineNum = Buf->Cursor.Line;
            Column = Buf->Cursor.ColumnIs;
            s64 LineCount = sb_count(Buf->Lines);
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column + 1 >= Buf->Lines[LineNum].ColumnCount) {
                    if(LineNum + 1 >= LineCount) {
                        Column = Buf->Lines[LineNum].ColumnCount;
                        break;
                    }
                    LineNum++;
                    Column = 0;
                } else {
                    Column++;
                }
            }
            
            Buf->Cursor.Line = LineNum;
            Buf->Cursor.ColumnIs = Column;
            Buf->Cursor.ColumnWas = Buf->Cursor.ColumnIs;
            Buf->Cursor.ByteOffset = Buf->Lines[LineNum].ByteOffset;
            for(s64 i = 0; i < Buf->Cursor.ColumnIs; ++i) {
                Buf->Cursor.ByteOffset += utf8_char_width(Buf->Data + Buf->Cursor.ByteOffset);
            }
        } break;
        
        case LEFT: {
            s64 LineNum = Buf->Cursor.Line;
            Column = Buf->Cursor.ColumnIs;
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column - 1 < 0) {
                    if(LineNum - 1 < 0) {
                        Column = 0;
                        break;
                    }
                    LineNum--;
                    Column = Buf->Lines[LineNum].ColumnCount - 1;
                } else {
                    Column--;
                }
            }
            
            Buf->Cursor.Line = LineNum;
            Buf->Cursor.ColumnIs = Column;
            Buf->Cursor.ColumnWas = Buf->Cursor.ColumnIs;
            Buf->Cursor.ByteOffset = Buf->Lines[LineNum].ByteOffset;
            for(s64 i = 0; i < Buf->Cursor.ColumnIs; ++i) {
                Buf->Cursor.ByteOffset += utf8_char_width(Buf->Data + Buf->Cursor.ByteOffset);
            }
        } break;
        
    }
}

void
do_char(buffer_t *Buf, u8 *Char) {
    u8 n = utf8_char_width(Char);
    sb_add(Buf->Data, n);
    
    for(s64 i = sb_count(Buf->Data) - 1; i >= Buf->Cursor.ByteOffset; --i) {
        Buf->Data[i + n] = Buf->Data[i];
    }
    
    for(s64 i = 0; i < n; ++i) {
        Buf->Data[Buf->Cursor.ByteOffset] = Char[i];
    }
    
    // Update following lines
    for(s64 i = Buf->Cursor.Line + 1; i < sb_count(Buf->Lines); ++i) {
        Buf->Lines[i].ByteOffset += n;
    }
    
    Buf->Lines[Buf->Cursor.Line].Size += n;
    Buf->Lines[Buf->Cursor.Line].ColumnCount += 1;
    
    // TODO: CRLF 
    if(*Char == '\n') {
        sb_add(Buf->Lines, 1);
        for(s64 i = sb_count(Buf->Lines) - 1; i > Buf->Cursor.Line; --i) {
            Buf->Lines[i] = Buf->Lines[i - 1];
        }
        
        line_t *NewLine = &Buf->Lines[Buf->Cursor.Line + 1];
        NewLine->ByteOffset = Buf->Cursor.ByteOffset + n;
        NewLine->Size = Buf->Lines[Buf->Cursor.Line].Size - (NewLine->ByteOffset - Buf->Lines[Buf->Cursor.Line].ByteOffset);
        NewLine->ColumnCount = Buf->Lines[Buf->Cursor.Line].ColumnCount - (Buf->Cursor.ColumnIs + 1);
        
        // Split current line
        Buf->Lines[Buf->Cursor.Line].Size -= NewLine->Size;
        Buf->Lines[Buf->Cursor.Line].ColumnCount -= NewLine->ColumnCount;
        
    } 
    
    cursor_move(Buf, RIGHT, 1);
    
}

void
draw_buffer(framebuffer_t *Fb, font_t *Font, buffer_t *Buf) {
    s32 LineHeight = Font->Ascent - Font->Descent; 
    
    irect_t Cursor = {.x = Font->MWidth * (s32)Buf->Cursor.ColumnIs, .y = LineHeight * (s32)Buf->Cursor.Line, .w = Font->MWidth, .h = LineHeight};
    draw_rect(Fb, Cursor, 0xff117766);
    
    for(s64 i = 0; i < sb_count(Buf->Lines); ++i) {
        u8 *Start = Buf->Data + Buf->Lines[i].ByteOffset;
        u8 *End = Start + Buf->Lines[i].Size;
        s32 LineY = (s32)i * LineHeight;
        s32 Center = LineY + (LineHeight >> 1);
        s32 Baseline = Center + (Font->MHeight >> 1);
        
        draw_text_line(Fb, Font, 0, Baseline, Start, End);
    }
}

font_t
load_ttf(char *Path, f32 Size) {
    font_t Font = {0};
    Font.Size = Size;
    
    ASSERT(platform_read_file(Path, &Font.File));
    
    s32 Status = stbtt_InitFont(&Font.StbInfo, Font.File.Data, 0);
    
    stbtt_GetFontVMetrics(&Font.StbInfo, &Font.Ascent, &Font.Descent, &Font.LineGap);
    f32 Scale = stbtt_ScaleForMappingEmToPixels(&Font.StbInfo, Size);
    Font.Ascent = (s32)(Font.Ascent * Scale);
    Font.Descent = (s32)(Font.Descent * Scale);
    Font.LineGap = (s32)(Font.LineGap * Scale);
    
    glyph_set_t *Set;
    if(get_glyph_set(&Font, 'M', &Set)) {
        stbtt_bakedchar *g = &Set->Glyphs['M'];
        Font.MHeight = (s32)(g->y1 - g->y0 + 0.5);
        Font.MWidth = (s32)(g->x1 - g->x0 + 0.5);
    }
    
    return Font;
}

void
load_file(char *Path, buffer_t *Buf) {
    mem_zero_struct(Buf);
    {
        // TODO: Think a little harder about how files are loaded
        platform_file_data_t File;
        ASSERT(platform_read_file(Path, &File));
        
        sb_add(Buf->Data, File.Size);
        mem_copy(Buf->Data, File.Data, File.Size);
        
        platform_free_file(&File);
        
    }
    
    // TODO: Detect file encoding, we assume utf-8 for now
    line_t Line = {0};
    u8 n = 0;
    for(s64 i = 0; i < sb_count(Buf->Data); i += n) {
        n = utf8_char_width(Buf->Data + i);
        Line.Size += n;
        Line.ColumnCount += 1;
        
        if(Buf->Data[i] == '\n' || (i + n) >= sb_count(Buf->Data)) {
            line_t *l = sb_add(Buf->Lines, 1);
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
    //load_file("../test/test.txt", &Ctx.Buffer);
}

panel_t *
panel_alloc() {
    // TODO: free list with a set number of panels, you dont need more than like 16 anyway.
    panel_t *Result = malloc(sizeof(panel_t));
    mem_zero_struct(Result);
    return Result;
}

void
panel_create(panel_ctx *PanelCtx) {
    if(!PanelCtx->Root) {
        PanelCtx->Root = panel_alloc();
        PanelCtx->Root->Buffer = &Ctx.Buffer;
        PanelCtx->Selected = PanelCtx->Root;
    } else {
        panel_t *Parent = PanelCtx->Selected;
        panel_t *Left = panel_alloc();
        panel_t *Right = panel_alloc();
        
        Left->Parent = Right->Parent = Parent;
        Parent->Children[0] = Left;
        Parent->Children[1] = Right;
        
        Left->Buffer = Right->Buffer = Parent->Buffer;
        Parent->Buffer = 0;
        
        PanelCtx->Selected = Left;
    }
}

void
panel_kill(panel_ctx *PanelCtx, panel_t *Panel) {
    if(Panel->Parent) {
        panel_t *Parent = Panel->Parent;
        panel_t *OtherChild = Parent->Children[0] == Panel ? Parent->Children[1] : Parent->Children[0];
        if(Parent->Parent) {
            panel_t **p = Parent->Parent->Children[0] == Parent ? &Parent->Parent->Children[0] : &Parent->Parent->Children[1];
            *p = OtherChild;
            OtherChild->Parent = Parent->Parent;
        } else {
            OtherChild->Parent = 0;
            PanelCtx->Root = OtherChild;
        }
        free(Parent);
        free(Panel);
        PanelCtx->Selected = OtherChild;
    } else {
        free(Panel);
        PanelCtx->Root = 0;
        PanelCtx->Selected = 0;
    }
}


void
panel_draw(framebuffer_t *Fb, panel_t *Panel, irect_t Rect) {
    if(Panel->Buffer) {
        u32 c = 0xff000000 | (u32)((u64)(Panel) * 0x127382 ^ 0x138293);
        draw_rect(Fb, Rect, c);
        if(Ctx.PanelCtx.Selected == Panel) {
            irect_t r;
            if(Panel->Split == SPLIT_Vertical) {
                r = (irect_t){Rect.x + Rect.w - 4, Rect.y, 4, Rect.h};
            } else {
                r = (irect_t){Rect.x, Rect.y + Rect.h - 4, Rect.w, 4};
            }
            draw_rect(Fb, r, 0xffffffff);
        }
    } else {
        irect_t r0, r1;
        if(Panel->Split == SPLIT_Vertical) {
            s32 Hw = Rect.w / 2;
            s32 RoundedHw = (s32)((f32)Rect.w / 2 + 0.5);
            r0 = (irect_t){Rect.x, Rect.y, Hw, Rect.h};
            r1 = (irect_t){Rect.x + Hw, Rect.y, RoundedHw, Rect.h};
        } else {
            s32 Hh = Rect.h / 2;
            s32 RoundedHh = (s32)((f32)Rect.h / 2 + 0.5);
            r0 = (irect_t){Rect.x, Rect.y, Rect.w, Hh};
            r1 = (irect_t){Rect.x, Rect.y + Hh, Rect.w, RoundedHh};
        }
        panel_draw(Fb, Panel->Children[0], r0);
        panel_draw(Fb, Panel->Children[1], r1);
    }
}

void
draw_panels(framebuffer_t *Fb) {
    if(Ctx.PanelCtx.Root) {
        irect_t Rect = {0, 0, Fb->Width, Fb->Height};
        panel_draw(Fb, Ctx.PanelCtx.Root, Rect);
    }
}

s32
panel_child_index(panel_t *Panel) {
    s32 i = (Panel->Parent->Children[0] == Panel) ? 0 : 1; 
    return i;
}

void
panel_move_selected(panel_ctx *PanelCtx, dir Dir) {
    // Panel->Children[0] is always left or above 
    panel_t *Panel = PanelCtx->Selected;
    if(PanelCtx->Root == Panel) {
        return;
    }
    
    if(Dir == LEFT || Dir == RIGHT) {
        s32 Idx = panel_child_index(Panel);
        panel_t *p = Panel;
        b32 Found = false;
        while(p != PanelCtx->Root) {
            s32 n = (Dir == LEFT) ? 1 : 0;
            if(panel_child_index(p) == n && p->Parent->Split == SPLIT_Vertical) {
                p = p->Parent->Children[n ^ 1];
                Found = true;
                break;
            }
            p = p->Parent;
        }
        
        if(Found) {
            while(!p->Buffer) {
                p = p->Children[p->Split == SPLIT_Vertical ? 1 : 0];
            }
            PanelCtx->Selected = p;
        }
        
    } else if(Dir == UP || Dir == DOWN) {
        s32 Idx = panel_child_index(Panel);
        panel_t *p = Panel;
        b32 Found = false;
        while(p != PanelCtx->Root) {
            s32 n = (Dir == UP) ? 1 : 0;
            if(panel_child_index(p) == n && p->Parent->Split == SPLIT_Horizontal) {
                p = p->Parent->Children[n ^ 1];
                Found = true;
                break;
            }
            p = p->Parent;
        }
        
        if(Found) {
            while(!p->Buffer) {
                p = p->Children[Idx];
            }
            PanelCtx->Selected = p;
        }
        
    }
}

void
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    for(s32 i = 0; i < Events->Count; ++i) {
        input_event_t *e = &Events->Events[i];
        if(e->Type == INPUT_EVENT_Press && e->Device == INPUT_DEVICE_Keyboard) {
            if(Ctx.Mode == MODE_Normal) {
                
            } else if(Ctx.Mode == MODE_Insert) {
                
                s32 StepSize = (e->Modifiers & INPUT_MOD_Ctrl) ? 5 : 1;
                switch(e->Key.KeyCode) {
                    case KEY_Left: { cursor_move(&Ctx.Buffer, LEFT, StepSize); } continue;
                    case KEY_Right: { cursor_move(&Ctx.Buffer, RIGHT, StepSize); } continue;
                    case KEY_Up: { cursor_move(&Ctx.Buffer, UP, StepSize); } continue;
                    case KEY_Down: { cursor_move(&Ctx.Buffer, DOWN, StepSize); } continue;
                    default: {
                        if(e->Key.HasCharacterTranslation) {
                            do_char(&Ctx.Buffer, e->Key.Character[0] == '\r' ? (u8 *)"\n" : e->Key.Character);
                        }
                    } continue;
                }
            }
            
            // Global
            if(e->Key.KeyCode == KEY_N && e->Modifiers & INPUT_MOD_Ctrl) {
                panel_create(&Ctx.PanelCtx);
            } else if(e->Key.KeyCode == KEY_X && e->Modifiers & INPUT_MOD_Ctrl) { 
                if(Ctx.PanelCtx.Selected) {
                    Ctx.PanelCtx.Selected->Split ^= 1;
                }
            } else if(e->Key.KeyCode == KEY_W && e->Modifiers & INPUT_MOD_Ctrl) { 
                if(Ctx.PanelCtx.Selected) {
                    panel_kill(&Ctx.PanelCtx, Ctx.PanelCtx.Selected);
                }
            } else if(e->Key.KeyCode == KEY_Left && e->Modifiers & INPUT_MOD_Ctrl) {
                panel_move_selected(&Ctx.PanelCtx, LEFT);
            } else if(e->Key.KeyCode == KEY_Right && e->Modifiers & INPUT_MOD_Ctrl) {
                panel_move_selected(&Ctx.PanelCtx, RIGHT);
            } else if(e->Key.KeyCode == KEY_Up && e->Modifiers & INPUT_MOD_Ctrl) {
                panel_move_selected(&Ctx.PanelCtx, UP);
            } else if(e->Key.KeyCode == KEY_Down && e->Modifiers & INPUT_MOD_Ctrl) {
                panel_move_selected(&Ctx.PanelCtx, DOWN);
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
    
    draw_panels(Shared->Framebuffer);
}
