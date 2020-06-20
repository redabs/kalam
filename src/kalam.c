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

#include "layout.c"
#include "render.c"

ctx_t Ctx = {0};

void
fix_cursor_overlap(panel_t *Panel) {
    // Remove any cursors that overlap
    for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
        cursor_t *c0 = &Panel->Cursors[i];
        
        for(s64 j = i + 1; j < sb_count(Panel->Cursors); ++j) {
            cursor_t *c1 = &Panel->Cursors[j];
            
            if(c1->Offset == c0->Offset) {
                for(s64 k = j; k < sb_count(Panel->Cursors) - 1; ++k) {
                    Panel->Cursors[k] = Panel->Cursors[k + 1];
                }
                sb_set_count(Panel->Cursors, sb_count(Panel->Cursors) - 1);
            }
        }
    }
}

void
cursor_move(panel_t *Panel, cursor_t *Cursor, dir_t Dir, s64 StepSize) {
    if(!Panel) { return; }
    buffer_t *Buf = Panel->Buffer;
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
    if(Buf->Text.Used == 0) {
        sb_push(Buf->Lines, (line_t){0});
    } else {
        sb_set_count(Buf->Lines, 0);
        line_t *Line = sb_add(Buf->Lines, 1);
        mem_zero_struct(Line);
        u8 n = 0;
        for(u64 i = 0; i < Buf->Text.Used; i += n) {
            n = utf8_char_width(Buf->Text.Data + i);
            
            if(Buf->Text.Data[i] == '\n') {
                // Push next line and init it
                Line = sb_add(Buf->Lines, 1);
                mem_zero_struct(Line);
                Line->Offset = i + n;
            } else {
                Line->Length += 1;
                Line->Size += n;
            }
        }
    }
}

void
insert_char(panel_t *Panel, u8 *Char) {
    buffer_t *Buf = Panel->Buffer;
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
        cursor_t *Cursor = &Panel->Cursors[CursorIndex];
        
        u8 n = utf8_char_width(Char);
        mem_buf_add(&Buf->Text, n);
        
        for(s64 i = Buf->Text.Used - 1; i >= Cursor->Offset; --i) {
            Buf->Text.Data[i + n] = Buf->Text.Data[i];
        }
        
        for(s64 i = 0; i < n; ++i) {
            Buf->Text.Data[Cursor-> Offset + i] = Char[i];
        }
        
        // Update all the following cursors
        for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
            cursor_t *c = &Panel->Cursors[i];
            if(c->Offset > Cursor->Offset) {
                c->Offset += n;
            }
        }
        
        make_lines(Buf);
        cursor_move(Panel, Cursor, RIGHT, 1);
    }
    fix_cursor_overlap(Panel);
}

void
do_backspace_ex(panel_t *Panel, cursor_t *Cursor) {
    buffer_t *Buf = Panel->Buffer;
    if(Cursor->Offset != 0) {
        s32 n = 1;
        for(u8 *c = Buf->Text.Data + Cursor->Offset; (*(--c) & 0xc0) == 0x80; ++n);
        
        // Update all following cursors
        for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
            cursor_t *c = &Panel->Cursors[i];
            if(c->Offset > Cursor->Offset) {
                c->Offset -= n;
            }
        }
        
        cursor_move(Panel, Cursor, LEFT, 1);
        mem_buf_delete_range(&Buf->Text, Cursor->Offset, n);
        make_lines(Buf);
    }
}

void
do_backspace(panel_t *Panel) {
    buffer_t *Buf = Panel->Buffer;
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
        do_backspace_ex(Panel, &Panel->Cursors[CursorIndex]);
    }
    fix_cursor_overlap(Panel);
}

void
do_char(panel_t *Panel, u8 *Char) {
    if(!Panel) { return; }
    if(*Char < 0x20) {
        switch(*Char) {
            case '\n':
            case '\r': {
                insert_char(Panel, (u8 *)"\n");
            } break;
            
            case '\t': {
                insert_char(Panel, (u8 *)"\t");
            } break;
            
            case '\b': {
                do_backspace(Panel);
            } break;
        }
    } else {
        insert_char(Panel, Char);
    }
}

void
do_delete(panel_t *Panel) {
    buffer_t *Buf = Panel->Buffer;
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
        cursor_t *Cursor = &Panel->Cursors[CursorIndex];
        line_t *Line = &Buf->Lines[Cursor->Line];
        
        if(!(Cursor->ColumnIs == Line->Length && Cursor->Line == (sb_count(Buf->Lines) - 1))) {
            // TODO: This is kind of a hack.
            cursor_move(Panel, Cursor, RIGHT, 1);
            do_backspace_ex(Panel, Cursor);
        }
    }
    fix_cursor_overlap(Panel);
}

u32 
codepoint_under_cursor(buffer_t *Buf, cursor_t *Cursor) {
    u32 Codepoint = 0;
    utf8_to_codepoint(Buf->Text.Data + Cursor->Offset, &Codepoint);
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
line_height(font_t *Font) {
    s32 LineHeight = Font->Ascent - Font->Descent + Font->LineGap; 
    return LineHeight;
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

s32 
cursor_pixel_x_offset(buffer_t *Buffer, font_t *Font, cursor_t *Cursor) {
    s32 x = 0;
    u8 *c = Buffer->Text.Data + Buffer->Lines[Cursor->Line].Offset;
    for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
        u32 Codepoint;
        c = utf8_to_codepoint(c, &Codepoint);
        x += get_x_advance(Font, Codepoint);
    }
    return x;
}

void
draw_cursor(framebuffer_t *Fb, irect_t PanelRect, font_t *Font, panel_t *Panel, cursor_t *Cursor) {
    s32 LineHeight = line_height(Font);
    irect_t Rect = {
        .x = PanelRect.x + cursor_pixel_x_offset(Panel->Buffer, Font, Cursor) - Panel->ScrollX, 
        .y = PanelRect.y + LineHeight * (s32)Cursor->Line - Panel->ScrollY, 
        .w = get_x_advance(Font, codepoint_under_cursor(Panel->Buffer, Cursor)), 
        .h = LineHeight
    };
    u32 Color = (Panel->Mode == MODE_Normal) ? COLOR_STATUS_NORMAL : COLOR_STATUS_INSERT;
    draw_rect(Fb, Rect, Color);
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
load_file(char *Path) {
    buffer_t *Buf = sb_add(Ctx.Buffers, 1);
    mem_zero_struct(Buf);
    platform_file_data_t File;
    ASSERT(platform_read_file(Path, &File));
    
    if(File.Size > 0) {
        mem_buf_add(&Buf->Text, File.Size);
        mem_copy(Buf->Text.Data, File.Data, File.Size);
    } else {
        mem_buf_grow(&Buf->Text, 128);
    }
    
    platform_free_file(&File);
    
    // TODO: Detect file encoding, we assume utf-8 for now
    make_lines(Buf);
}

panel_t *
panel_alloc(panel_ctx_t *PanelCtx) {
    if(PanelCtx->FreeList) {
        panel_t *Result = &PanelCtx->FreeList->Panel;
        PanelCtx->FreeList = PanelCtx->FreeList->Next;
        mem_zero_struct(Result);
        return Result;
    }
    return 0;
}

void
panel_free(panel_ctx_t *PanelCtx, panel_t *Panel) {
    if(!Panel) { return; }
    panel_free_node_t *n = (panel_free_node_t *)Panel;
    n->Next = PanelCtx->FreeList;
    PanelCtx->FreeList = n;
}

void
panel_create(panel_ctx_t *PanelCtx) {
    // no hay nada mas
    if(!PanelCtx->FreeList) {
        return;
    }
    
    if(!PanelCtx->Root) {
        PanelCtx->Root = panel_alloc(PanelCtx);
        PanelCtx->Root->Buffer = &Ctx.Buffers[0];
        PanelCtx->Selected = PanelCtx->Root;
        
        cursor_t *Cursor = sb_add(PanelCtx->Root->Cursors, 1);
        mem_zero_struct(Cursor);
    } else {
        panel_t *Parent = PanelCtx->Selected;
        panel_t *Left = panel_alloc(PanelCtx);
        panel_t *Right = panel_alloc(PanelCtx);
        
        if(!Right || !Left) { 
            panel_free(PanelCtx, Left); 
            panel_free(PanelCtx, Right); 
            return;
        }
        
        *Left = *Parent;
        Parent->Children[0] = Left;
        Parent->Children[1] = Right;
        Left->Parent = Right->Parent = Parent;
        
        Right->Split = Left->Split;
        Right->Buffer = Left->Buffer,
        Right->Mode = MODE_Normal;
        mem_zero_struct(sb_add(Right->Cursors, 1));
        
        Parent->Buffer = 0;
        
        PanelCtx->Selected = Right;
        Right->Parent->LastSelected = 1;
    }
}

s32
panel_child_index(panel_t *Panel) {
    s32 i = (Panel->Parent->Children[0] == Panel) ? 0 : 1; 
    return i;
}

void
panel_kill(panel_ctx_t *PanelCtx, panel_t *Panel) {
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
split_panel_rect(panel_t *Panel, irect_t Rect, irect_t *r0, irect_t *r1) {
    if(Panel->Split == SPLIT_Vertical) {
        s32 Hw = Rect.w / 2;
        s32 RoundedHw = (s32)((f32)Rect.w / 2 + 0.5);
        *r0 = (irect_t){Rect.x, Rect.y, Hw, Rect.h};
        *r1 = (irect_t){Rect.x + Hw, Rect.y, RoundedHw, Rect.h};
    } else {
        s32 Hh = Rect.h / 2;
        s32 RoundedHh = (s32)((f32)Rect.h / 2 + 0.5);
        *r0 = (irect_t){Rect.x, Rect.y, Rect.w, Hh};
        *r1 = (irect_t){Rect.x, Rect.y + Hh, Rect.w, RoundedHh};
    }
}

void
draw_line_number(framebuffer_t *Fb, font_t *Font, irect_t Rect, u32 Color, u64 LineNum) {
    u8 NumStr[64];
    u64 n = 0;
    u8 *c = NumStr + sizeof(NumStr);
    for(u64 i = LineNum; i > 0; i /= 10, n++) {
        --c;
        *c = '0' + (i % 10);
    }
    s32 Width = text_width(Font, c, c + n);
    draw_text_line(Fb, Font, Rect.x + Rect.w - Width, Rect.y, Color, c, c + n);
}

void
draw_selection(framebuffer_t *Fb, panel_t *Panel, cursor_t *Cursor, font_t *Font, irect_t TextRegion) {
    if(Cursor->SelectionOffset <= 0) {
        buffer_t *Buf = Panel->Buffer;
        s64 LineIndexStart = Cursor->Line;
        for(s64 SelectionStart = Cursor->Offset + Cursor->SelectionOffset; Buf->Lines[LineIndexStart].Offset > SelectionStart; --LineIndexStart);
        
        s32 LineHeight = line_height(Font);
        for(s64 i = LineIndexStart; i <= Cursor->Line; ++i) {
            s32 y = TextRegion.y + (s32)i * LineHeight;
            line_t *Line = &Buf->Lines[i];
            
            s64 Start = MAX(Cursor->Offset + Cursor->SelectionOffset, Line->Offset);
            s64 End = MIN(Cursor->Offset, Line->Offset + Line->Size);
            s32 Width = text_width(Font, Buf->Text.Data + Start, Buf->Text.Data + End + utf8_char_width(Buf->Text.Data + End));
            s32 SelectionPixelOffset = (Start == Line->Offset)  ? 0 : text_width(Font, Buf->Text.Data + Line->Offset, Buf->Text.Data + Start);
            irect_t LineRect = {TextRegion.x + SelectionPixelOffset, y - Panel->ScrollY, Width, LineHeight};
            draw_rect(Fb, LineRect, COLOR_SELECTION);
        }
    } else {
        buffer_t *Buf = Panel->Buffer;
        s64 LineIndexEnd = Cursor->Line;
        for(s64 SelectionStart = Cursor->Offset + Cursor->SelectionOffset; 
            (Buf->Lines[LineIndexEnd].Offset + Buf->Lines[LineIndexEnd].Size) < Cursor->Offset + Cursor->SelectionOffset;
            ++LineIndexEnd);
        
        s32 LineHeight = line_height(Font);
        for(s64 i = Cursor->Line; i <= LineIndexEnd; ++i) {
            s32 y = TextRegion.y + (s32)i * LineHeight;
            line_t *Line = &Buf->Lines[i];
            
            s64 Start = MAX(Cursor->Offset, Line->Offset);
            s64 End = MIN(Cursor->Offset + Cursor->SelectionOffset, Line->Offset + Line->Size);
            s32 Width = text_width(Font, Buf->Text.Data + Start, Buf->Text.Data + End + utf8_char_width(Buf->Text.Data + End));
            s32 SelectionPixelOffset = (Start == Line->Offset)  ? 0 : text_width(Font, Buf->Text.Data + Line->Offset, Buf->Text.Data + Start);
            irect_t LineRect = {TextRegion.x + SelectionPixelOffset, y - Panel->ScrollY, Width, LineHeight};
            draw_rect(Fb, LineRect, COLOR_SELECTION);
        }
    }
}

void
panel_draw(framebuffer_t *Fb, panel_t *Panel, font_t *Font, irect_t PanelRect) {
    if(Panel->Buffer) {
        irect_t TextRegion = text_buffer_rect(Panel, Font, PanelRect);
        
        Fb->Clip = PanelRect;
        
        buffer_t *Buf = Panel->Buffer;
        if(Buf) {
            s32 LineHeight = line_height(Font);
            
            // Draw main cursor and line highlight
            {
                cursor_t *Cursor = &Panel->Cursors[0];
                s32 y = TextRegion.y + (s32)Cursor->Line * LineHeight;
                irect_t LineRect = {TextRegion.x, y - Panel->ScrollY, TextRegion.w, LineHeight};
                draw_rect(Fb, LineRect, COLOR_LINE_HIGHLIGHT);
                draw_selection(Fb, Panel, Cursor, Font, TextRegion);
                draw_cursor(Fb, TextRegion, Font, Panel, Cursor);
            }
            
            // Draw the rest of the cursors
            for(s64 CursorIndex = 1; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
                draw_cursor(Fb, TextRegion, Font, Panel, &Panel->Cursors[CursorIndex]);
            }
            
            // Draw lines
            irect_t LineNumberRect = line_number_rect(Panel, Font, PanelRect);
            for(s64 i = 0; i < sb_count(Buf->Lines); ++i) {
                u8 *Start = Buf->Text.Data + Buf->Lines[i].Offset;
                u8 *End = Start + Buf->Lines[i].Size;
                s32 LineY = TextRegion.y + (s32)i * LineHeight;
                s32 Center = LineY + (LineHeight >> 1);
                s32 Baseline = Center + (Font->MHeight >> 1) - Panel->ScrollY;
                
                Fb->Clip = TextRegion;
                draw_text_line(Fb, Font, TextRegion.x - Panel->ScrollX, Baseline, COLOR_TEXT, Start, End);
                Fb->Clip = PanelRect;
                s64 n = ABS(i - Panel->Cursors[0].Line);
                draw_line_number(Fb, Font, (irect_t){LineNumberRect.x, Baseline, LineNumberRect.w, LineHeight}, n == 0 ? COLOR_LINE_NUMBER_CURRENT : COLOR_LINE_NUMBER, n == 0 ? i + 1 : n);
            }
            
            irect_t StatusBar = status_bar_rect(PanelRect);
            draw_rect(Fb, StatusBar, COLOR_STATUS_BAR);
            
            irect_t r0, r1, r2, r3;
            panel_border_rects(PanelRect, &r0, &r1, &r2, &r3);
            
            u32 BorderColor = COLOR_BG;
            if(Ctx.PanelCtx.Selected == Panel) {
                BorderColor = COLOR_PANEL_SELECTED;
            } else if(Panel != Ctx.PanelCtx.Root && Panel->Parent->LastSelected == panel_child_index(Panel)) {
                BorderColor = COLOR_PANEL_LAST_SELECTED;
            } 
            draw_rect(Fb, r0, BorderColor);
            draw_rect(Fb, r1, BorderColor);
            draw_rect(Fb, r2, BorderColor);
            draw_rect(Fb, r3, BorderColor);
            
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
        split_panel_rect(Panel, PanelRect, &r0, &r1);
        panel_draw(Fb, Panel->Children[0], Font, r0);
        panel_draw(Fb, Panel->Children[1], Font, r1);
    }
}

irect_t 
workspace_rect(framebuffer_t *Fb) {
    irect_t Rect = {0, 0, Fb->Width, Fb->Height};
    return Rect;
}

void
draw_panels(framebuffer_t *Fb, font_t *Font) {
    if(Ctx.PanelCtx.Root) {
        irect_t Rect = workspace_rect(Fb);
        panel_draw(Fb, Ctx.PanelCtx.Root, Font, Rect);
    }
}

void
panel_move_selection(panel_ctx_t *PanelCtx, dir_t Dir) {
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
    load_file("test.c");
    //load_file("../test/test.txt");
    
    u32 n = ARRAY_COUNT(Ctx.PanelCtx.Panels);
    for(u32 i = 0; i < n - 1; ++i) {
        Ctx.PanelCtx.Panels[i].Next = &Ctx.PanelCtx.Panels[i + 1];
    }
    Ctx.PanelCtx.Panels[n - 1].Next = 0;
    Ctx.PanelCtx.FreeList = &Ctx.PanelCtx.Panels[0];
    
    panel_create(&Ctx.PanelCtx);
}

void
set_panel_scroll(panel_t *Panel, irect_t Rect) {
    if(!Panel) { return; }
    
    if(Panel->Buffer) {
        s32 LineHeight = line_height(&Ctx.Font);
        cursor_t *Cursor = &Panel->Cursors[0];
        
        irect_t TextRegion = text_buffer_rect(Panel, &Ctx.Font, Rect);
        s32 CursorTop = (s32)Cursor->Line * LineHeight;
        s32 CursorBottom = CursorTop + LineHeight;
        s32 Bottom = Panel->ScrollY + TextRegion.h;
        
        if(CursorBottom > Bottom) {
            Panel->ScrollY += CursorBottom - Bottom;
            
        } else if(CursorTop < Panel->ScrollY) {
            Panel->ScrollY = CursorTop;
        }
        
        s32 CursorLeft = cursor_pixel_x_offset(Panel->Buffer, &Ctx.Font, Cursor);
        s32 CursorWidth = get_x_advance(&Ctx.Font, codepoint_under_cursor(Panel->Buffer, Cursor));
        s32 CursorRight = CursorLeft + CursorWidth;
        s32 Right = Panel->ScrollX + TextRegion.w;
        
        if(CursorRight > Right) {
            Panel->ScrollX += CursorRight - Right;
            
        } else if(CursorLeft < Panel->ScrollX) {
            Panel->ScrollX = CursorLeft;
        }
        
    } else {
        irect_t r0, r1;
        split_panel_rect(Panel, Rect, &r0, &r1);
        set_panel_scroll(Panel->Children[0], r0);
        set_panel_scroll(Panel->Children[1], r1);
    }
}


b32
do_operation(operation_t Op) {
    b32 WasHandled = true;
    switch(Op.Type) {
        case OP_EscapeToNormal: {
            if(Ctx.PanelCtx.Selected) {
                Ctx.PanelCtx.Selected->Mode = MODE_Normal;
            }
        } break;
        
        case OP_MoveCursorWithSelection: {
            if(!Ctx.PanelCtx.Selected) { break; }
            panel_t *Panel = Ctx.PanelCtx.Selected;
            switch(Op.MoveCursorWithSelection.Dir) {
                case LEFT: {
                    for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
                        cursor_t Cursor = Panel->Cursors[i];
                        cursor_move(Panel, &Panel->Cursors[i], LEFT, 1);
                        Panel->Cursors[i].SelectionOffset = Cursor.SelectionOffset + ABS(Cursor.Offset - Panel->Cursors[i].Offset);
                    }
                } break;
                
                case RIGHT: {
                    for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
                        cursor_t Cursor = Panel->Cursors[i];
                        cursor_move(Panel, &Panel->Cursors[i], RIGHT, 1);
                        Panel->Cursors[i].SelectionOffset = Cursor.SelectionOffset - ABS(Cursor.Offset - Panel->Cursors[i].Offset);
                    }
                } break;
                
                case UP: {
                } break;
                
                case DOWN: {
                } break;
            }
        } break;
        
        case OP_Delete: {
            if(Ctx.PanelCtx.Selected) {
                do_delete(Ctx.PanelCtx.Selected);
            }
        } break;
        
        case OP_EnterInsertMode: {
            if(Ctx.PanelCtx.Selected) {
                Ctx.PanelCtx.Selected->Mode = MODE_Insert;
            }
        } break;
        
        case OP_Home: {
            panel_t *Panel = Ctx.PanelCtx.Selected;
            if(Panel) {
                buffer_t *Buf = Panel->Buffer;
                for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
                    cursor_t *c = &Panel->Cursors[i];
                    line_t *l = &Buf->Lines[c->Line];
                    c->Offset = l->Offset;
                    c->ColumnIs = c->ColumnWas = 0;
                }
            }
        } break;
        
        case OP_End: {
            panel_t *Panel = Ctx.PanelCtx.Selected;
            if(Panel) {
                buffer_t *Buf = Panel->Buffer;
                for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
                    cursor_t *c = &Panel->Cursors[i];
                    line_t *l = &Buf->Lines[c->Line];
                    c->Offset = l->Offset + l->Size;
                    c->ColumnIs = c->ColumnWas = l->Length;
                }
            }
        } break;
        
        case OP_MoveCursor: {
            panel_t *Panel = Ctx.PanelCtx.Selected;
            if(Panel) {
                for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
                    cursor_t *Cursor = &Panel->Cursors[i];
                    cursor_move(Panel, Cursor, Op.MoveCursor.Dir, Op.MoveCursor.StepSize); 
                    Cursor->SelectionOffset = 0;
                }
                fix_cursor_overlap(Panel);
            }
        } break;
        
        case OP_ToggleSplitMode: {
            if(Ctx.PanelCtx.Selected) {
                Ctx.PanelCtx.Selected->Split ^= 1;
            }
        } break;
        
        case OP_NewPanel: {
            panel_create(&Ctx.PanelCtx);
        } break;
        
        case OP_KillPanel: {
            panel_kill(&Ctx.PanelCtx, Ctx.PanelCtx.Selected);
        } break;
        
        case OP_MovePanelSelection: {
            panel_move_selection(&Ctx.PanelCtx, Op.MovePanelSelection.Dir); 
        } break;
        
        default: {
            WasHandled = false;
        } break;
    }
    
    return WasHandled;
}

b32
event_mapping_match(key_mapping_t *Map, input_event_t *Event) {
    if(Map->IsKey && Event->Type == INPUT_EVENT_Press) {
        if(Map->Key == Event->Key.KeyCode) {
            if(Map->Modifiers == Event->Modifiers) {
                return true;
            }
        }
        
    } else if(!Map->IsKey && Event->Type == INPUT_EVENT_Text) {
        return (*((u32 *)Map->Character) == *((u32 *)Event->Text.Character));
    }
    
    return false;
}

key_mapping_t *
find_mapping_match(key_mapping_t *Mappings, s64 MappingCount, input_event_t *e) {
    for(s64 i = 0; i < MappingCount; ++i) {
        key_mapping_t *m = &Mappings[i];
        if(event_mapping_match(m, e)) {
            return m;
        }
    }
    
    return 0;
}

void
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    // TODO: Configurable keybindings
    for(s32 EventIndex = 0; EventIndex < Events->Count; ++EventIndex) {
        input_event_t *e = &Events->Events[EventIndex];
        if(e->Device == INPUT_DEVICE_Keyboard) {
            if(e->Type == INPUT_EVENT_Press || e->Type == INPUT_EVENT_Text) {
                b32 EventHandled = false;
                panel_t *Panel = Ctx.PanelCtx.Selected;
                if(Panel) {
                    if(Panel->Mode == MODE_Normal) {
                        key_mapping_t *m = find_mapping_match(NormalMappings, ARRAY_COUNT(NormalMappings), e);
                        if(m) {
                            EventHandled = do_operation(m->Operation);
                        }
                    } else if(Panel->Mode == MODE_Insert) {
                        key_mapping_t *m = find_mapping_match(InsertMappings, ARRAY_COUNT(InsertMappings), e);
                        if(m) {
                            EventHandled = do_operation(m->Operation);
                        } else if(e->Type == INPUT_EVENT_Text) {
                            if(Panel) {
                                // Ignore characters that were sent with the only modifier down being Ctrl (except for Caps Lock and Num Lock).
                                if((e->Modifiers & (INPUT_MOD_Ctrl | INPUT_MOD_Shift | INPUT_MOD_Alt)) != INPUT_MOD_Ctrl) {
                                    do_char(Panel, e->Text.Character);
                                }
                                EventHandled = true;
                            }
                        }
                    }
                }
                if(!EventHandled) {
                    key_mapping_t *m = find_mapping_match(GlobalMappings, ARRAY_COUNT(GlobalMappings), e);
                    if(m) {
                        do_operation(m->Operation);
                    }
                }
                
            }
        }
        
    } Events->Count = 0;
    
    set_panel_scroll(Ctx.PanelCtx.Root, workspace_rect(Shared->Framebuffer));
    
    clear_framebuffer(Shared->Framebuffer, COLOR_BG);
    Shared->Framebuffer->Clip = (irect_t){0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
    
    if(!Ctx.PanelCtx.Root) {
        irect_t Rect = {0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
        iv2_t p = center_text_in_rect(&Ctx.Font, Rect, (u8 *)"Hello there.", 0);
        draw_text_line(Shared->Framebuffer, &Ctx.Font, p.x, p.y, 0xffffffff, (u8 *)"Hello there.", 0);
    }
    
    draw_panels(Shared->Framebuffer, &Ctx.Font);
    
#if 0    
    for(u32 i = 0, x = 0; i < Ctx.Font.SetCount; ++i) {
        bitmap_t *b = &Ctx.Font.GlyphSets[i].Set->Bitmap;
        irect_t Rect = {0, 0, b->w, b->h};
        draw_glyph_bitmap(Shared->Framebuffer, x, Shared->Framebuffer->Height - Rect.h, 0xffffffff, Rect, b);
        x += b->w + 10;
    }
#endif
}
