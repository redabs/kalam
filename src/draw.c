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
text_width(font_t *Font, range_t Text) {
    s32 Result = 0;
    u8 *c = Text.Data;
    u8 *End = Text.Data + Text.Size;
    u32 Codepoint;
    while(c < End) {
        c = utf8_to_codepoint(c, &Codepoint);
        Result += get_x_advance(Font, Codepoint);
    }
    
    return Result;
}

// Returns {x, baseline}
iv2_t 
center_text_in_rect(font_t *Font, irect_t Rect, range_t Text) {
    s32 TextWidth = text_width(Font, Text);
    iv2_t Result = {.x = Rect.x + (Rect.w - TextWidth) / 2, .y = Rect.y + (Rect.h + Font->MHeight) / 2};
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
    range_t s = {.Data = c, .Size = n}; 
    s32 Width = text_width(Font, s);
    draw_text_line(Fb, Font, Rect.x + Rect.w - Width, Rect.y, Color, s);
}

void
draw_cursor(framebuffer_t *Fb, panel_t *Panel, selection_t *Selection, font_t *Font, irect_t TextRegion, u32 Color) {
    s64 LineIndex = offset_to_line_index(Panel->Buffer, Selection->Cursor);
    s32 LineHeight = line_height(Font);
    
    s32 Width = Font->MWidth;
    if(Selection->Cursor < Panel->Buffer->Text.Used) {
        u8 *InBuf = Panel->Buffer->Text.Data + Selection->Cursor;
        Width = text_width(Font, (range_t){.Data = InBuf, .Size = utf8_char_width(InBuf)});
    }
    line_t *Line = Panel->Buffer->Lines + LineIndex;
    s32 PixelOffsetToCursor = text_width(Font, (range_t){.Data = Panel->Buffer->Text.Data + Line->Offset, .Size = Selection->Cursor - Line->Offset});
    
    irect_t Cursor = {.x = TextRegion.x + PixelOffsetToCursor - Panel->ScrollX, .y = TextRegion.y + (s32)(LineIndex * LineHeight - Panel->ScrollY), .w = Width, .h = LineHeight};
    
    draw_rect(Fb, Cursor, Color);
}

void
draw_selection(framebuffer_t *Fb, panel_t *Panel, selection_t *Selection, font_t *Font, irect_t TextRegion) {
    buffer_t *Buf = Panel->Buffer;
    
    u64 SelectionStart = selection_start(Selection);
    u64 SelectionEnd = selection_end(Selection);
    SelectionEnd += utf8_char_width(Buf->Text.Data + SelectionEnd);
    u64 LineIndexStart = offset_to_line_index(Panel->Buffer, SelectionStart);
    u64 LineIndexEnd = offset_to_line_index(Panel->Buffer, SelectionEnd);
    
    s32 LineHeight = line_height(Font);
    for(u64 i = LineIndexStart; i <= LineIndexEnd; ++i) {
        s32 y = TextRegion.y + (s32)i * LineHeight;
        line_t *Line = &Buf->Lines[i];
        
        // Compute the extent of the selection on the current line.
        // The selection might:
        // A. fully include the current line, 
        // [The quick brown fox]
        
        // B. start some non-zero offset into it and include the end, 
        // The [quick brown fox]
        
        // C. include the start of the current line but not the end,
        // [The quick] brown fox
        
        // D. start at some non-zero offset into it and not include the end.
        // The [quick brown] fox
        
        u64 Start = MAX(Line->Offset, SelectionStart);
        u64 Size = MIN(Line->Offset + Line->Size, SelectionEnd) - Start;
        
        s32 Width = text_width(Font, (range_t){.Data = Buf->Text.Data + Start, .Size = Size});
        
        // There is one exception where the selection is allowed to go outside the line and it is on the last "imaginary" character on the 
        // last line in the file. The reason the cursor can go one byte outside like this is so that one can append to the end of the file.
        // Because this one-past-the-end byte could be anything we don't want to include it in the string we pass to text_width, so to handle
        // this we check if the cursor is past the end of the line and we're on the last line and then increase the size my the width of 'M'.
        if(i == (u64)(sb_count(Buf->Lines) - 1) && SelectionEnd > Line->Offset + Line->Size) {
            Width += Font->MWidth;
        } 
        s32 PixelOffsetToSelectionStart = text_width(Font, (range_t){.Data = Buf->Text.Data + Line->Offset, .Size = Start - Line->Offset});
        irect_t LineRect = {.x = TextRegion.x + PixelOffsetToSelectionStart - Panel->ScrollX, .y = y - Panel->ScrollY, .w = Width, .h = LineHeight};
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
            
            selection_group_t *SelGrp;
            // If we're in Select mode and there is a search term and there are selections in the working set then
            // we get the working set selection group instead of the one active in the buffer.
            if(Panel->Mode == MODE_Select && Panel->ModeCtx.Select.SearchTerm.Used != 0 && sb_count(Panel->ModeCtx.Select.SelectionGroup.Selections) != 0) {
                SelGrp = &Panel->ModeCtx.Select.SelectionGroup;
            } else {
                SelGrp = get_selection_group(Panel->Buffer, Panel);
            }
            
            selection_t SelectionMaxIdx = get_selection_max_idx(SelGrp);
            
            // Compute ScrollX and ScrollY
            {
                u64 Li = offset_to_line_index(Panel->Buffer, SelectionMaxIdx.Cursor);
                s32 OffsetY = (s32) Li * LineHeight;
                if(OffsetY >= (Panel->ScrollY + TextRegion.h)) {
                    Panel->ScrollY = OffsetY - TextRegion.h + LineHeight;
                } else if(OffsetY < Panel->ScrollY) {
                    Panel->ScrollY = OffsetY;
                }
                
                line_t *Line = &Panel->Buffer->Lines[Li];
                s32 OffsetX = text_width(&Ctx.Font, (range_t){.Data = Panel->Buffer->Text.Data + Line->Offset, .Size = SelectionMaxIdx.Cursor - Line->Offset});
                if(OffsetX >= (TextRegion.w + Panel->ScrollX)) {
                    Panel->ScrollX = OffsetX - TextRegion.w + Ctx.Font.MWidth;
                } else if(OffsetX < Panel->ScrollX) {
                    Panel->ScrollX = OffsetX;
                }
            }
            
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *s = &SelGrp->Selections[i];
                draw_selection(Fb, Panel, s, &Ctx.Font, TextRegion);
                draw_cursor(Fb, Panel, s, &Ctx.Font, TextRegion, (s->Idx == SelGrp->SelectionIdxTop - 1) ? 0xffbbbbbb : 0xff333333);
            }
            
            // Draw lines
            irect_t LineNumberRect = line_number_rect(Panel, Font, PanelRect);
            for(s64 i = 0; i < sb_count(Buf->Lines); ++i) {
                s32 LineY = TextRegion.y + (s32)i * LineHeight;
                s32 Center = LineY + (LineHeight >> 1);
                s32 Baseline = Center + (Font->MHeight >> 1) - Panel->ScrollY;
                
                Fb->Clip = TextRegion;
                draw_text_line(Fb, Font, TextRegion.x - Panel->ScrollX, Baseline, COLOR_TEXT, (range_t){.Data = Buf->Text.Data + Buf->Lines[i].Offset, .Size = Buf->Lines[i].Size});
                Fb->Clip = PanelRect;
                s64 n = ABS(i - (s64)offset_to_line_index(Buf, SelectionMaxIdx.Cursor));
                draw_line_number(Fb, Font, (irect_t){LineNumberRect.x, Baseline, LineNumberRect.w, LineHeight}, n == 0 ? COLOR_LINE_NUMBER_CURRENT : COLOR_LINE_NUMBER, n == 0 ? i + 1 : n);
            }
            
#if 0
            {
                s32 x = 400; 
                s32 y = 100;
                char Debug[1024];
                sprintf(Debug, "SelectionMaxIdx: %llu", SelGrp->SelectionIdxTop);
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
            
            u32 ModeStrColor = 0xffffffff;
            range_t ModeString = C_STR_AS_RANGE("Invalid mode");
            switch(Panel->Mode) {
                case MODE_Normal: {
                    ModeString = C_STR_AS_RANGE("NORMAL");
                    ModeStrColor = COLOR_STATUS_NORMAL;
                } break;
                
                case MODE_Insert: {
                    ModeString = C_STR_AS_RANGE("INSERT");
                    ModeStrColor = COLOR_STATUS_INSERT;
                } break;
                
                case MODE_Select: {
                    ModeString = mem_buffer_as_range(Panel->ModeCtx.Select.SearchTerm);
                } break;
                
                case MODE_FileSelect: {
                    ModeString = C_STR_AS_RANGE("File Select");
                } break;
            }
            
            iv2_t TextPos = center_text_in_rect(&Ctx.Font, StatusBar, ModeString);
            draw_text_line(Fb, &Ctx.Font, TextPos.x, TextPos.y, ModeStrColor, ModeString);
            draw_text_line(Fb, &Ctx.Font, StatusBar.x, TextPos.y, ModeStrColor, (Panel->Split == SPLIT_Vertical) ? C_STR_AS_RANGE("|") : C_STR_AS_RANGE("_"));
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
    irect_t ClipSave = Fb->Clip;
    if(Ctx.PanelCtx.Root) {
        irect_t Rect = workspace_rect(Fb);
        draw_panel(Fb, Ctx.PanelCtx.Root, Font, Rect);
    }
    Fb->Clip = ClipSave;
}


void
draw_file_menu(framebuffer_t *Fb) {
    irect_t Workspace = workspace_rect(Fb);
    irect_t ClipSave = Fb->Clip;
    
    s32 hw = 250 >> 1;
    s32 LineHeight = line_height(&Ctx.Font);
    irect_t Box = {.x = (Workspace.w >> 1)  - hw, .y = 120, .w = hw * 2, LineHeight * 16};
    draw_rect(Fb, Box, 0xdd2c302d);
    
    s32 y = Box.y;
    s32 HalfLineHeight = LineHeight >> 1;
    s32 HalfMHeight = (Ctx.Font.MHeight >> 1);
    
    range_t CurrentDirText = mem_buffer_as_range(Ctx.WorkingDirectory);
    iv2_t CurrentDirTextPos = center_text_in_rect(&Ctx.Font, (irect_t){.x = Box.x, .y = Box.y - LineHeight, .w = Box.w, .h = LineHeight}, CurrentDirText);
    draw_text_line(Fb, &Ctx.Font, CurrentDirTextPos.x, CurrentDirTextPos.y, 0xffffffff, CurrentDirText);
    
    Fb->Clip = Box;
    for(u64 Offset = 0, i = 0; Offset < Ctx.EnumeratedFiles.Used; ++i) {
        file_select_option_t *Opt = (file_select_option_t *)(&Ctx.EnumeratedFiles.Data[Offset]); 
        range_t Path = {.Data = Opt->String, .Size = Opt->Size};
        if(i == Ctx.SelectedFile.Index) {
            draw_rect(Fb, (irect_t){.x = Box.x, .y = Box.y += LineHeight * (s32)i, .w = Box.w, .h = LineHeight}, 0xff8a8a8a);
        }
        s32 BaseLine = y + HalfLineHeight + HalfMHeight;
        draw_text_line(Fb, &Ctx.Font, Box.x + 5, BaseLine, 0xffffffff, Path);
        Offset += Opt->Size + sizeof(file_select_option_t);
        y += LineHeight;
    }
    
    Fb->Clip = ClipSave;
}