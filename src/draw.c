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
    buffer_t *Buf = Panel->Buffer;
    
    s64 Start = MIN(Cursor->Offset, Cursor->Offset + Cursor->SelectionOffset);
    s64 End = MAX(Cursor->Offset, Cursor->Offset + Cursor->SelectionOffset);
    
    s64 LineIndexStart = Cursor->Line;
    s64 LineIndexEnd = Cursor->Line;
    
    if(Cursor->SelectionOffset <= 0) {
        while(Buf->Lines[LineIndexStart].Offset > Start) {
            --LineIndexStart;
        }
        
    } else {
        while(Buf->Lines[LineIndexEnd + 1].Offset <= End) {
            ++LineIndexEnd;
        }
        End += utf8_char_width(Buf->Text.Data + End);
    }
    
    s32 LineHeight = line_height(Font);
    for(s64 i = LineIndexStart; i <= LineIndexEnd; ++i) {
        s32 y = TextRegion.y + (s32)i * LineHeight;
        line_t *Line = &Buf->Lines[i];
        
        u8 *SelectionStartOnCurrentLine = Buf->Text.Data + MAX(Line->Offset, Start);
        u8 *SelectionEndOnCurrentLine = Buf->Text.Data + MIN(Line->Offset + Line->Size, End);
        
        s32 Width = text_width(Font, SelectionStartOnCurrentLine, SelectionEndOnCurrentLine);
        
        irect_t LineRect = {TextRegion.x + text_width(Font, Buf->Text.Data + Line->Offset, SelectionStartOnCurrentLine), y - Panel->ScrollY, Width, LineHeight};
        draw_rect(Fb, LineRect, COLOR_SELECTION);
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
                draw_selection(Fb, Panel, &Panel->Cursors[CursorIndex], Font, TextRegion);
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
