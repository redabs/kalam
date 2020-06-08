#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "memory.h"
#include "types.h"
#include "event.h"
#include "platform.h"

#include "custom.h"

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
cursor_move(buffer_t *Buf, cursor_t *Cursor, dir Dir, s64 StepSize) {
    int LineEndChars = 0; // TODO: CRLF, LF, LFCR, .....
    s64 Column = 0;
    switch(Dir) {
        case DOWN: 
        case UP: {
            if((Dir == UP && Cursor->Line > 0) ||
               (Dir == DOWN && Cursor->Line < (sb_count(Buf->Lines) - 1))) {
                s64 LineNum = CLAMP(0, sb_count(Buf->Lines) - 1, Cursor->Line + ((Dir == DOWN) ? StepSize : -StepSize));
                Column = MIN(Cursor->ColumnWas, Buf->Lines[LineNum].Length);
                
                Cursor->Line = LineNum;
                Cursor->ColumnIs = Column;
                Cursor->Offset = Buf->Lines[LineNum].Offset;
                for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
                    Cursor->Offset += utf8_char_width(Buf->Text.Data + Cursor->Offset);
                }
            }
        } break;
        
        case RIGHT: {
            s64 LineNum = Cursor->Line;
            Column = Cursor->ColumnIs;
            s64 LineCount = sb_count(Buf->Lines);
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column + 1 > Buf->Lines[LineNum].Length) {
                    if(LineNum + 1 >= LineCount) {
                        Column = Buf->Lines[LineNum].Length;
                        break;
                    }
                    LineNum++;
                    Column = 0;
                } else {
                    Column++;
                }
            }
            
            Cursor->Line = LineNum;
            Cursor->ColumnIs = Column;
            Cursor->ColumnWas = Cursor->ColumnIs;
            Cursor->Offset = Buf->Lines[LineNum].Offset;
            for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
                Cursor->Offset += utf8_char_width(Buf->Text.Data + Cursor->Offset);
            }
        } break;
        
        case LEFT: {
            s64 LineNum = Cursor->Line;
            Column = Cursor->ColumnIs;
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column - 1 < 0) {
                    if(LineNum - 1 < 0) {
                        Column = 0;
                        break;
                    }
                    LineNum--;
                    Column = Buf->Lines[LineNum].Length;
                } else {
                    Column--;
                }
            }
            
            Cursor->Line = LineNum;
            Cursor->ColumnIs = Column;
            Cursor->ColumnWas = Cursor->ColumnIs;
            Cursor->Offset = Buf->Lines[LineNum].Offset;
            for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
                Cursor->Offset += utf8_char_width(Buf->Text.Data + Cursor->Offset);
            }
        } break;
    }
}

void
make_lines(buffer_t *Buf) {
    sb_set_count(Buf->Lines, 0);
    line_t Line = {0};
    u8 n = 0;
    for(u64 i = 0; i < Buf->Text.Used; i += n) {
        n = utf8_char_width(Buf->Text.Data + i);
        
        if(Buf->Text.Data[i] == '\n') {
            sb_push(Buf->Lines, Line);
            
            // Reset for next line
            mem_zero_struct(&Line);
            Line.Offset = i + n;
        } else {
            Line.Length += 1;
            Line.Size += n;
            if((i + n) >= Buf->Text.Used) {
                // Last character in buffer without trailing newline character
                sb_push(Buf->Lines, Line);
                break;
            }
        }
    }
}

void
do_char(buffer_t *Buf, u8 *Char) {
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Buf->Cursors); ++CursorIndex) {
        cursor_t *Cursor = &Buf->Cursors[CursorIndex];
        
        u8 n = utf8_char_width(Char);
        mem_buf_add(&Buf->Text, n);
        
        for(s64 i = Buf->Text.Used - 1; i >= Cursor->Offset; --i) {
            Buf->Text.Data[i + n] = Buf->Text.Data[i];
        }
        
        for(s64 i = 0; i < n; ++i) {
            Buf->Text.Data[Cursor-> Offset + i] = Char[i];
        }
        
        // Update all the following cursors
        for(s64 i = 0; i < sb_count(Buf->Cursors); ++i) {
            cursor_t *c = &Buf->Cursors[i];
            if(c->Offset > Cursor->Offset) {
                c->Offset += n;
            }
        }
        
        make_lines(Buf);
        cursor_move(Buf, Cursor, RIGHT, 1);
    }
}

void
do_backspace_ex(buffer_t *Buf, cursor_t *Cursor) {
    if(Cursor->Offset != 0) {
        s32 n = 1;
        for(u8 *c = Buf->Text.Data + Cursor->Offset; (*(--c) & 0xc0) == 0x80; ++n);
        
        // Update all following cursors
        for(s64 i = 0; i < sb_count(Buf->Cursors); ++i) {
            cursor_t *c = &Buf->Cursors[i];
            if(c->Offset > Cursor->Offset) {
                c->Offset -= n;
            }
        }
        
        cursor_move(Buf, Cursor, LEFT, 1);
        mem_buf_delete_range(&Buf->Text, Cursor->Offset, n);
        make_lines(Buf);
    }
}

void
do_backspace(buffer_t *Buf) {
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Buf->Cursors); ++CursorIndex) {
        do_backspace_ex(Buf, &Buf->Cursors[CursorIndex]);
    }
}

void
do_delete(buffer_t *Buf) {
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Buf->Cursors); ++CursorIndex) {
        cursor_t *Cursor = &Buf->Cursors[CursorIndex];
        line_t *Line = &Buf->Lines[Cursor->Line];
        
        if(!(Cursor->ColumnIs == Line->Length && Cursor->Line == (sb_count(Buf->Lines) - 1))) {
            // TODO: This is kind of a hack.
            cursor_move(Buf, Cursor, RIGHT, 1);
            do_backspace_ex(Buf, Cursor);
        }
    }
}

u32 
codepoint_under_cursor(buffer_t *Buf, cursor_t *Cursor) {
    u8 *c = Buf->Text.Data + Buf->Lines[Cursor->Line].Offset;
    for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
        c += utf8_char_width(c);
    }
    u32 Codepoint = 0;
    utf8_to_codepoint(c, &Codepoint);
    return Codepoint;
}

stbtt_bakedchar *
get_stbtt_bakedchar(font_t *Font, u32 Codepoint) {
    glyph_set_t *Set;
    if(get_glyph_set(Font, Codepoint, &Set)) {
        return &Set->Glyphs[Codepoint % 256];
    }
    return 0;
}

s32
get_x_advance(font_t *Font, u32 Codepoint) {
    s32 Result = Font->MWidth;
    if(Codepoint == '\t') {
        Result = Font->SpaceWidth * TAB_WIDTH;
    } else {
        stbtt_bakedchar *g = get_stbtt_bakedchar(Font, Codepoint);
        if(g) {
            Result = (s32)(g->xadvance + 0.5);
        }
    }
    return Result;
}

void
draw_cursor(framebuffer_t *Fb, irect_t PanelRect, font_t *Font, buffer_t *Buf, cursor_t *Cursor) {
    s32 LineHeight = Font->Ascent - Font->Descent + Font->LineGap; 
    irect_t Rect = {.x = PanelRect.x, .y = PanelRect.y + LineHeight * (s32)Cursor->Line, .w = get_x_advance(Font, codepoint_under_cursor(Buf, Cursor)), .h = LineHeight};
    
    u8 *c = Buf->Text.Data + Buf->Lines[Cursor->Line].Offset;
    for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
        u32 Codepoint;
        c = utf8_to_codepoint(c, &Codepoint);
        Rect.x += get_x_advance(Font, Codepoint);
    }
    
    draw_rect(Fb, Rect, 0xffddee66);
}

s32
text_width(font_t *Font, u8 *Start, u8 *End) {
    if(!End) {
        End = Start;
        while(*End) { ++End; }
    }
    
    s32 Result = 0;
    u8 *c = Start;
    u32 Codepoint;
    while(c < End) {
        c = utf8_to_codepoint(c, &Codepoint);
        Result += get_x_advance(Font, Codepoint);
    }
    
    return Result;
}

// Returns {x, baseline}
iv2_t 
center_text_in_rect(font_t *Font, irect_t Rect, u8 *Start, u8 *End) {
    if(!End) {
        End = Start;
        while(*End) { ++End; }
    }
    
    s32 TextWidth = text_width(Font, Start, End);
    iv2_t Result;
    Result.x = Rect.x + (Rect.w - TextWidth) / 2;
    Result.y = Rect.y + (Rect.h + Font->MHeight) / 2;
    return Result;
}

font_t
load_ttf(char *Path, f32 Size) {
    font_t Font = {0};
    Font.Size = Size;
    
    ASSERT(platform_read_file(Path, &Font.File));
    
    s32 Status = stbtt_InitFont(&Font.StbInfo, Font.File.Data, 0);
    
    stbtt_GetFontVMetrics(&Font.StbInfo, &Font.Ascent, &Font.Descent, &Font.LineGap);
    f32 Scale = stbtt_ScaleForMappingEmToPixels(&Font.StbInfo, Size);
    Font.Ascent = (s32)(Font.Ascent * Scale + 0.5);
    Font.Descent = (s32)(Font.Descent * Scale + 0.5);
    Font.LineGap = (s32)(Font.LineGap * Scale + 0.5);
    
    stbtt_bakedchar *g = get_stbtt_bakedchar(&Font, 'M');
    if(g) {
        Font.MHeight = (s32)(g->y1 - g->y0 + 0.5);
        Font.MWidth = (s32)(g->x1 - g->x0 + 0.5);
    }
    
    Font.SpaceWidth = get_x_advance(&Font, ' ');
    
    return Font;
}

void
load_file(char *Path, buffer_t *Buf) {
    mem_zero_struct(Buf);
    // TODO: Think a little harder about how files are loaded
    platform_file_data_t File;
    ASSERT(platform_read_file(Path, &File));
    
    mem_buf_add(&Buf->Text, File.Size);
    mem_copy(Buf->Text.Data, File.Data, File.Size);
    
    platform_free_file(&File);
    
    // TODO: Detect file encoding, we assume utf-8 for now
    make_lines(Buf);
    
    cursor_t *Cursor = sb_add(Buf->Cursors, 1);
    mem_zero_struct(Cursor);
}

panel_t *
panel_alloc(panel_ctx *PanelCtx) {
    if(PanelCtx->FreeList) {
        panel_t *Result = &PanelCtx->FreeList->Panel;
        PanelCtx->FreeList = PanelCtx->FreeList->Next;
        return Result;
    }
    return 0;
}

void
panel_free(panel_ctx *PanelCtx, panel_t *Panel) {
    if(!Panel) { return; }
    panel_free_node_t *n = (panel_free_node_t *)Panel;
    n->Next = PanelCtx->FreeList;
    PanelCtx->FreeList = n;
}

void
panel_create(panel_ctx *PanelCtx) {
    // no hay nada mas
    if(!PanelCtx->FreeList) {
        return;
    }
    
    if(!PanelCtx->Root) {
        PanelCtx->Root = panel_alloc(PanelCtx);
        PanelCtx->Root->Buffer = &Ctx.Buffer;
        PanelCtx->Selected = PanelCtx->Root;
    } else {
        panel_t *Parent = PanelCtx->Selected;
        panel_t *Left = panel_alloc(PanelCtx);
        panel_t *Right = panel_alloc(PanelCtx);
        
        if(!Right) { 
            panel_free(PanelCtx, Left); 
            return;
        }
        
        *Left = *Right = *Parent;
        Left->Parent = Right->Parent = Parent;
        Parent->Children[0] = Left;
        Parent->Children[1] = Right;
        
        Parent->Buffer = 0;
        
        PanelCtx->Selected = Left;
    }
}

s32
panel_child_index(panel_t *Panel) {
    s32 i = (Panel->Parent->Children[0] == Panel) ? 0 : 1; 
    return i;
}

void
panel_kill(panel_ctx *PanelCtx, panel_t *Panel) {
    if(Panel->Parent) {
        s32 Idx = panel_child_index(Panel);
        panel_t *Sibling = Panel->Parent->Children[Idx ^ 1];
        
        // Copy sibling to parent as panels are leaf nodes so parent can't have only one valid child.
        Sibling->Parent = Panel->Parent->Parent;
        if(Sibling->Children[0]) {
            Sibling->Children[0]->Parent = Panel->Parent;
        }
        if(Sibling->Children[1]) {
            Sibling->Children[1]->Parent = Panel->Parent;
        }
        
        *Panel->Parent = *Sibling;
        
        panel_t *p = Panel->Parent;
        while(!p->Buffer) {
            p = p->Children[p->LastSelected];
        }
        PanelCtx->Selected = p;
        
        panel_free(PanelCtx, Sibling);
        panel_free(PanelCtx, Panel);
    } else {
        PanelCtx->Selected = Panel->Parent;
        panel_free(PanelCtx, Panel);
        PanelCtx->Root = 0;
    }
}

void
panel_draw(framebuffer_t *Fb, panel_t *Panel, font_t *Font, irect_t Rect) {
    if(Panel->Buffer) {
        irect_t PanelRect = Rect;
        s32 Padding = 1;
        Rect = (irect_t) {Rect.x + Padding, Rect.y + Padding, Rect.w - Padding * 2, Rect.h - Padding * 2};
        
        Fb->Clip = PanelRect;
        
        buffer_t *Buf = Panel->Buffer;
        if(Buf) {
            s32 LineHeight = Font->Ascent - Font->Descent + Font->LineGap; 
            
            {
                cursor_t *Cursor = &Buf->Cursors[0];
                s32 y = Rect.y + (s32)Cursor->Line * LineHeight;
                irect_t LineRect = {Rect.x, y, Rect.w, LineHeight};
                draw_rect(Fb, LineRect, 0xff1f4068);
                draw_cursor(Fb, Rect, Font, Buf, Cursor);
            }
            
            for(s64 CursorIndex = 1; CursorIndex < sb_count(Buf->Cursors); ++CursorIndex) {
                draw_cursor(Fb, Rect, Font, Buf, &Buf->Cursors[CursorIndex]);
            }
            
            for(s64 i = 0; i < sb_count(Buf->Lines); ++i) {
                u8 *Start = Buf->Text.Data + Buf->Lines[i].Offset;
                u8 *End = Start + Buf->Lines[i].Size;
                s32 LineY = Rect.y + (s32)i * LineHeight;
                s32 Center = LineY + (LineHeight >> 1);
                s32 Baseline = Center + (Font->MHeight >> 1);
                
                draw_text_line(Fb, Font, Rect.x, Baseline, 0xff90b080, Start, End);
            }
            
            irect_t StatusBar = {Rect.x, Rect.y + Rect.h - 20, Rect.w, 20};
            
            u32 StatusColor = COLOR_STATUS_BAR;
            draw_rect(Fb, StatusBar, StatusColor);
            
            irect_t r0 = {PanelRect.x, PanelRect.y, Padding, PanelRect.h};
            irect_t r1 = {PanelRect.x + PanelRect.w - Padding, PanelRect.y, Padding, PanelRect.h};
            irect_t r2 = {PanelRect.x, PanelRect.y, PanelRect.w, Padding};
            irect_t r3 = {PanelRect.x, PanelRect.y + PanelRect.h - Padding, PanelRect.w, Padding};
            u32 Border = COLOR_BG;
            if(Ctx.PanelCtx.Selected == Panel) {
                Border = COLOR_PANEL_SELECTED;
            } else if(Panel != Ctx.PanelCtx.Root && Panel->Parent->LastSelected == panel_child_index(Panel)) {
                Border = COLOR_PANEL_LAST_SELECTED;
            } 
            
            draw_rect(Fb, r0, Border);
            draw_rect(Fb, r1, Border);
            draw_rect(Fb, r2, Border);
            draw_rect(Fb, r3, Border);
            
            u8 *ModeStr = (u8 *)"Invalid mode";
            u32 ModeStrColor = 0xffffffff;
            switch(Panel->Mode) {
                case MODE_Normal: {
                    ModeStr = (u8 *)"NORMAL";
                    ModeStrColor = COLOR_STATUS_NORMAL;
                } break;
                
                case MODE_Insert: {
                    ModeStr = (u8 *)"INSERT";
                    ModeStrColor = COLOR_STATUS_INSERT;
                } break;
            }
            
            iv2_t TextPos = center_text_in_rect(&Ctx.Font, StatusBar, ModeStr, 0);
            draw_text_line(Fb, &Ctx.Font, TextPos.x, TextPos.y, ModeStrColor, ModeStr, 0);
            draw_text_line(Fb, &Ctx.Font, StatusBar.x, TextPos.y, ModeStrColor, (Panel->Split == SPLIT_Vertical) ? (u8*)"|" : (u8*)"_", 0);
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
        panel_draw(Fb, Panel->Children[0], Font, r0);
        panel_draw(Fb, Panel->Children[1], Font, r1);
    }
}

void
draw_panels(framebuffer_t *Fb, font_t *Font) {
    if(Ctx.PanelCtx.Root) {
        irect_t Rect = {0, 0, Fb->Width, Fb->Height};
        panel_draw(Fb, Ctx.PanelCtx.Root, Font, Rect);
    }
}

void
panel_move_selected(panel_ctx *PanelCtx, dir Dir) {
    // Panel->Children[0] is always left or above 
    panel_t *Panel = PanelCtx->Selected;
    if(PanelCtx->Root == Panel) {
        return;
    }
    
    if(Dir == LEFT || Dir == RIGHT) {
        s32 Idx = 0;
        panel_t *p = Panel;
        b32 Found = false;
        while(p != PanelCtx->Root) {
            u8 n = (Dir == LEFT) ? 1 : 0;
            Idx = panel_child_index(p);
            if(Idx == n && p->Parent->Split == SPLIT_Vertical) {
                p = p->Parent->Children[n ^ 1];
                p->Parent->LastSelected = n ^ 1;
                Found = true;
                break;
            }
            p = p->Parent;
        }
        
        if(Found) {
            while(!p->Buffer) {
                p = p->Children[p->LastSelected];
            }
            
            panel_t *Par = p->Parent;
            // Only move to the last selected node if both children are leaves (i.e. they have buffers attached)
            if(Par != Panel->Parent && Par->Children[0]->Buffer && Par->Children[1]->Buffer) {
                PanelCtx->Selected = p->Parent->Children[p->Parent->LastSelected];
            } else {
                PanelCtx->Selected = p;
                p->Parent->LastSelected = (u8)panel_child_index(p);
            }
        }
        
    } else if(Dir == UP || Dir == DOWN) {
        s32 Idx = 0;
        panel_t *p = Panel;
        b32 Found = false;
        while(p != PanelCtx->Root) {
            u8 n = (Dir == UP) ? 1 : 0;
            Idx = panel_child_index(p);
            if(Idx == n && p->Parent->Split == SPLIT_Horizontal) {
                p = p->Parent->Children[n ^ 1];
                p->Parent->LastSelected = n ^ 1;
                Found = true;
                break;
            }
            p = p->Parent;
        }
        
        if(Found) {
            while(!p->Buffer) {
                p = p->Children[p->LastSelected];
            }
            
            panel_t *Par = p->Parent;
            // Only move to the last selected node if both children are leaves (i.e. they have buffers attached)
            if(Par != Panel->Parent && Par->Children[0]->Buffer && Par->Children[1]->Buffer) {
                PanelCtx->Selected = p->Parent->Children[p->Parent->LastSelected];
            } else {
                PanelCtx->Selected = p;
                p->Parent->LastSelected = (u8)panel_child_index(p);
            }
        }
        
    }
}

void
k_init(platform_shared_t *Shared) {
    Ctx.Font = load_ttf("fonts/consola.ttf", 15);
    load_file("test.c", &Ctx.Buffer);
    //load_file("../test/test.txt", &Ctx.Buffer);
    
    u32 n = ARRAY_COUNT(Ctx.PanelCtx.Panels);
    for(u32 i = 0; i < n - 1; ++i) {
        Ctx.PanelCtx.Panels[i].Next = &Ctx.PanelCtx.Panels[i + 1];
    }
    Ctx.PanelCtx.Panels[n - 1].Next = 0;
    Ctx.PanelCtx.FreeList = &Ctx.PanelCtx.Panels[0];
}

b32
do_normal_keybinds(panel_t *Panel, input_event_t *e) {
    buffer_t *Buf = Panel->Buffer;
    b32 DidHandle = true;
    if(e->Modifiers & INPUT_MOD_Ctrl) {
        switch(e->Key.KeyCode) {
            case KEY_X: { 
                if(Ctx.PanelCtx.Selected) {
                    Ctx.PanelCtx.Selected->Split ^= 1;
                }
            } break;
            
            case KEY_W: {
                if(Ctx.PanelCtx.Selected) {
                    panel_kill(&Ctx.PanelCtx, Ctx.PanelCtx.Selected);
                }
            } break;
            
            case KEY_Left: { panel_move_selected(&Ctx.PanelCtx, LEFT); } break;
            case KEY_Right: { panel_move_selected(&Ctx.PanelCtx, RIGHT); } break;
            case KEY_Up: { panel_move_selected(&Ctx.PanelCtx, UP); } break;
            case KEY_Down: { panel_move_selected(&Ctx.PanelCtx, DOWN); } break;
            
            default: { DidHandle = false; } break;
        }
    } else {
        switch(e->Key.KeyCode) {
            case KEY_I: { Panel->Mode = MODE_Insert; } break;
            default: { DidHandle = false; } break;
        }
    }
    return DidHandle;
}

b32
do_insert_keybinds(panel_t *Panel, input_event_t *e) {
    buffer_t *Buf = Panel->Buffer;
    b32 DidHandle = true;
    switch(e->Key.KeyCode) {
        case KEY_Escape: {
            Panel->Mode = MODE_Normal;
        } break;
        
        case KEY_Backspace: {
            do_backspace(Buf);
        } break;
        
        default: {
            // Only handle character input if Ctrl is not the only modifier (among the non-toggled modifiers, i.e. not Caps, Num-lock) that is currently active for the event.
            // The intent here is to filter out control characters; we only want actual readable characters.
            u64 ModCond = (e->Modifiers & (INPUT_MOD_Ctrl | INPUT_MOD_Shift | INPUT_MOD_Alt)) != INPUT_MOD_Ctrl;
            if(e->Key.HasCharacterTranslation && ModCond) {
                do_char(Buf, e->Key.Character[0] == '\r' ? (u8 *)"\n" : e->Key.Character);
            } else {
                DidHandle = false;
            }
        }
    }
    
    return DidHandle;
}

b32
do_global_keybinds(buffer_t *Buf, input_event_t *e) {
    b32 DidHandle = true;
    s32 StepSize = (e->Modifiers & INPUT_MOD_Ctrl) ? 5 : 1;
    switch(e->Key.KeyCode) {
        case KEY_Home: {
            for(s64 i = 0; i < sb_count(Buf->Cursors); ++i) {
                cursor_t *c = &Buf->Cursors[i];
                line_t *l = &Buf->Lines[c->Line];
                c->Offset = l->Offset;
                c->ColumnIs = c->ColumnWas = 0;
            }
        } break;
        
        case KEY_N: {
            if(e->Modifiers & INPUT_MOD_Ctrl && !e->Key.IsRepeatKey) {
                panel_create(&Ctx.PanelCtx); 
            }
        } break;
        
        case KEY_End: {
            for(s64 i = 0; i < sb_count(Buf->Cursors); ++i) {
                cursor_t *c = &Buf->Cursors[i];
                line_t *l = &Buf->Lines[c->Line];
                c->Offset = l->Offset + l->Size;
                c->ColumnIs = c->ColumnWas = l->Length;
            }
        } break;
        
        case KEY_Delete: {
            do_delete(Buf);
        } break;
        
        case KEY_Left:
        case KEY_Right:
        case KEY_Up:
        case KEY_Down: { 
            dir Map[] = {
                [KEY_Left] = LEFT, 
                [KEY_Right] = RIGHT, 
                [KEY_Up] = UP, 
                [KEY_Down] = DOWN, 
            };
            
            dir Dir = Map[e->Key.KeyCode];
            if(e->Modifiers & INPUT_MOD_Shift && (Dir == UP || Dir == DOWN)) {
                sb_push(Buf->Cursors, Buf->Cursors[0]);
                cursor_move(Buf, &Buf->Cursors[0], Dir, StepSize); 
            } else {
                for(s64 i = 0; i < sb_count(Buf->Cursors); ++i) {
                    cursor_move(Buf, &Buf->Cursors[i], Dir, StepSize); 
                }
            }
            
        }
        default: { DidHandle = false; };
    }
    
    return DidHandle;
}

void
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    // TODO: Configurable keybindings
    for(s32 EventIndex = 0; EventIndex < Events->Count; ++EventIndex) {
        input_event_t *e = &Events->Events[EventIndex];
        b32 b = false;
        if(e->Type == INPUT_EVENT_Press && e->Device == INPUT_DEVICE_Keyboard) {
            if(Ctx.PanelCtx.Selected) {
                if(Ctx.PanelCtx.Selected->Mode == MODE_Normal) b = do_normal_keybinds(Ctx.PanelCtx.Selected, e);
                else if(Ctx.PanelCtx.Selected->Mode == MODE_Insert) b = do_insert_keybinds(Ctx.PanelCtx.Selected, e);
            }
            
            if(!b || !Ctx.PanelCtx.Selected) {
                b = do_global_keybinds(&Ctx.Buffer, e);
            }
        } 
        
    } Events->Count = 0;
    
    
    // Remove any cursors that overlap
    for(s64 i = 0; i < sb_count(Ctx.Buffer.Cursors); ++i) {
        cursor_t *c0 = &Ctx.Buffer.Cursors[i];
        
        for(s64 j = i + 1; j < sb_count(Ctx.Buffer.Cursors); ++j) {
            cursor_t *c1 = &Ctx.Buffer.Cursors[j];
            
            if(c1->Offset == c0->Offset) {
                for(s64 k = j; k < sb_count(Ctx.Buffer.Cursors) - 1; ++k) {
                    Ctx.Buffer.Cursors[k] = Ctx.Buffer.Cursors[k + 1];
                }
                sb_set_count(Ctx.Buffer.Cursors, sb_count(Ctx.Buffer.Cursors) - 1);
            }
        }
    }
    
    
    clear_framebuffer(Shared->Framebuffer, COLOR_BG);
    Shared->Framebuffer->Clip = (irect_t){0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
    
    if(!Ctx.PanelCtx.Root) {
        irect_t Rect = {0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
        iv2_t p = center_text_in_rect(&Ctx.Font, Rect, (u8 *)"Hello there.", 0);
        draw_text_line(Shared->Framebuffer, &Ctx.Font, p.x, p.y, 0xffffffff, (u8 *)"Hello there.", 0);
    }
    
    draw_panels(Shared->Framebuffer,  &Ctx.Font);
    
#if 0    
    for(u32 i = 0, x = 0; i < Ctx.Font.SetCount; ++i) {
        bitmap_t *b = &Ctx.Font.GlyphSets[i].Set->Bitmap;
        irect_t Rect = {0, 0, b->w, b->h};
        draw_glyph_bitmap(Shared->Framebuffer, x, Shared->Framebuffer->Height - Rect.h, 0xffffffff, Rect, b);
        x += b->w + 10;
    }
#endif
}
