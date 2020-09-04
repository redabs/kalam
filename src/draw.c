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
draw_cursor(framebuffer_t *Fb, panel_t *Panel, selection_t *Selection, font_t *Font, irect_t TextRegion, u32 Color) {
    s64 LineIndex = offset_to_line_index(Panel->Buffer, Selection->Cursor);
    s32 LineHeight = line_height(Font);
    
    s32 Width = Font->MWidth;
    if(Selection->Cursor < Panel->Buffer->Text.Used) {
        u8 *InBuf = Panel->Buffer->Text.Data + Selection->Cursor;
        Width = text_width(Font, InBuf, InBuf + utf8_char_width(InBuf));
    }
    line_t *Line = Panel->Buffer->Lines + LineIndex;
    u8 *LineInBuffOff = Panel->Buffer->Text.Data + Line->Offset;
    s32 x = text_width(Font, LineInBuffOff, Panel->Buffer->Text.Data + Selection->Cursor);
    
    irect_t Cursor = {.x = TextRegion.x + x, .y = TextRegion.y + (s32)(LineIndex * LineHeight - Panel->ScrollY), .w = Width, .h = LineHeight};
    
    draw_rect(Fb, Cursor, Color);
}

void
draw_selection(framebuffer_t *Fb, panel_t *Panel, selection_t *Selection, font_t *Font, irect_t TextRegion) {
    buffer_t *Buf = Panel->Buffer;
    
    s64 Start = selection_start(Selection);
    s64 End = selection_end(Selection);
    End += utf8_char_width(Buf->Text.Data + End);
    s64 LineIndexStart = offset_to_line_index(Panel->Buffer, Start);
    s64 LineIndexEnd = offset_to_line_index(Panel->Buffer, End);
    
    s32 LineHeight = line_height(Font);
    for(s64 i = LineIndexStart; i <= LineIndexEnd; ++i) {
        s32 y = TextRegion.y + (s32)i * LineHeight;
        line_t *Line = &Buf->Lines[i];
        
        u8 *SelectionStartOnCurrentLine = Buf->Text.Data + MAX(Line->Offset, Start);
        u8 *SelectionEndOnCurrentLine = Buf->Text.Data + MIN(Line->Offset + Line->Size, End);
        
        s32 Width = text_width(Font, SelectionStartOnCurrentLine, SelectionEndOnCurrentLine);
        
        // There is one exception where the selection is allowed to go outside the line and it is on the last "imaginary" character on the 
        // last line in the file. The reason the cursor can go one byte outside like this is so that one can append to the end of the file.
        // Because this one-past-the-end byte could be anything we don't want to include it in the string we pass to text_width, so to handle
        // this we check if the cursor is past the end of the line and we're on the last line and then increase the size my the width of 'M'.
        if(i == (sb_count(Buf->Lines) - 1) && End > Line->Offset + Line->Size) {
            Width += Font->MWidth;
        } 
        
        irect_t LineRect = {TextRegion.x + text_width(Font, Buf->Text.Data + Line->Offset, SelectionStartOnCurrentLine), y - Panel->ScrollY, Width, LineHeight};
        draw_rect(Fb, LineRect, COLOR_SELECTION);
    }
}

#include <stdio.h> // TODO: Remove

void
draw_panel(framebuffer_t *Fb, panel_t *Panel, font_t *Font, irect_t PanelRect) {
    if(Panel->Buffer) {
        irect_t TextRegion = text_buffer_rect(Panel, Font, PanelRect);
        
        Fb->Clip = PanelRect;
        
        buffer_t *Buf = Panel->Buffer;
        if(Buf) {
            s32 LineHeight = line_height(Font);
            
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *s = &SelGrp->Selections[i];
                draw_selection(Fb, Panel, s, &Ctx.Font, TextRegion);
                draw_cursor(Fb, Panel, s, &Ctx.Font, TextRegion, (s->Idx == SelGrp->SelectionIdxTop - 1) ? 0xffbbbbbb : 0xff333333);
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
                s64 n = ABS(i - offset_to_line_index(Buf, get_selection_max_idx(Panel).Cursor));
                draw_line_number(Fb, Font, (irect_t){LineNumberRect.x, Baseline, LineNumberRect.w, LineHeight}, n == 0 ? COLOR_LINE_NUMBER_CURRENT : COLOR_LINE_NUMBER, n == 0 ? i + 1 : n);
            }
            
#if 0            
            {
                selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
                s32 x = 400; 
                s32 y = 100;
                char Debug[1024];
                sprintf(Debug, "SelectionTopIdx: %llu", SelGrp->SelectionIdxTop);
                draw_text_line(Fb, &Ctx.Font, x, y - 10, 0xffffffff, (u8*)Debug, 0);
                for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                    s32 LineY = y + (s32)i * LineHeight;
                    s32 Center = LineY + (LineHeight >> 1);
                    s32 Baseline = Center + (Font->MHeight >> 1);
                    char *c = Debug;
                    selection_t *s = &SelGrp->Selections[i];
                    c += sprintf(c, "Idx: %lld, Anchor %lld, Cursor %lld, ColumnIs %lld, ColumnWas %lld", s->Idx, s->Anchor, s->Cursor, s->ColumnIs, s->ColumnWas);
                    draw_rect(Fb, (irect_t){x, LineY, text_width(&Ctx.Font, (u8*)Debug, 0), LineHeight}, 0xff000000);
                    draw_text_line(Fb, &Ctx.Font, x, Baseline, 0xffffffff, (u8*)Debug, 0);
                }
            }
#endif
            
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
        draw_panel(Fb, Panel->Children[0], Font, r0);
        draw_panel(Fb, Panel->Children[1], Font, r1);
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
        draw_panel(Fb, Ctx.PanelCtx.Root, Font, Rect);
    }
}