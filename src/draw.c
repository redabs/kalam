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
draw_line_number(framebuffer_t *Fb, font_t *Font, irect_t Rect, color_t Color, u64 LineNum) {
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
draw_cursor(framebuffer_t *Fb, panel_t *Panel, selection_t *Selection, font_t *Font, irect_t TextRegion, color_t Color) {
    s64 LineIndex = offset_to_line_index(Panel->Buffer, Selection->Cursor);
    s32 LineHeight = Font->LineHeight;
    
    s32 Width = Font->MWidth;
    if(Selection->Cursor < Panel->Buffer->Text.Used) {
        u8 *InBuf = Panel->Buffer->Text.Data + Selection->Cursor;
        Width = text_width(Font, (range_t){.Data = InBuf, .Size = utf8_char_width(InBuf)});
    }
    line_t *Line = Panel->Buffer->Lines + LineIndex;
    s32 PixelOffsetToCursor = text_width(Font, (range_t){.Data = Panel->Buffer->Text.Data + Line->Offset, .Size = Selection->Cursor - Line->Offset});
    
    irect_t Cursor = {.x = TextRegion.x + PixelOffsetToCursor - Panel->Scroll.x, .y = TextRegion.y + (s32)(LineIndex * LineHeight - Panel->Scroll.y), .w = Width, .h = LineHeight};
    
    draw_rect(Fb, Cursor, Color);
}

void
draw_selection(framebuffer_t *Fb, panel_t *Panel, selection_t *Selection, font_t *Font, irect_t TextRegion) {
    buffer_t *Buf = Panel->Buffer;
    
    u64 SelectionStart = selection_start(Selection);
    u64 SelectionEnd = selection_end(Selection);
    if(Buf->Text.Used > 0) {
        SelectionEnd += utf8_char_width(Buf->Text.Data + SelectionEnd);
    }
    u64 LineIndexStart = offset_to_line_index(Panel->Buffer, SelectionStart);
    u64 LineIndexEnd = offset_to_line_index(Panel->Buffer, SelectionEnd);
    
    s32 LineHeight = Font->LineHeight;
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
        // this we check if the cursor is past the end of the line and we're on the last line and then increase the size by the width of 'M'.
        if(i == (u64)(sb_count(Buf->Lines) - 1) && SelectionEnd > Line->Offset + Line->Size) {
            Width += Font->MWidth;
        } 
        s32 PixelOffsetToSelectionStart = text_width(Font, (range_t){.Data = Buf->Text.Data + Line->Offset, .Size = Start - Line->Offset});
        irect_t LineRect = {.x = TextRegion.x + PixelOffsetToSelectionStart - Panel->Scroll.x, .y = y - Panel->Scroll.y, .w = Width, .h = LineHeight};
        draw_rect(Fb, LineRect, COLOR_SELECTION);
    }
}

// For now
color_t
token_color(token_t Token) {
    u32 Color = COLOR_TEXT.Argb;
    switch(Token.Type) { 
        case TOKEN_CharLiteral:
        case TOKEN_StringLiteral: {
            Color = 0xffffff00;
        } break; 
        
        case TOKEN_CppDirective: {
            Color = 0xff56845e;
        } break;
        
        case TOKEN_IncludePath: {
            Color = 0xff8455ff;
        } break;
        
        case TOKEN_Keyword: {
            Color = 0xffee9999;
        } break;
        
        case TOKEN_Identifier: {
        } break;
        
        case TOKEN_HexadecimalLiteral:
        case TOKEN_BinaryLiteral:
        case TOKEN_OctalLiteral:
        case TOKEN_FloatLiteral: {
            Color = 0xff6666aa;
        } break;
        
        case TOKEN_DecimalLiteral: {
            Color = 0xffffffff;
        } break;
        
        case TOKEN_Dot:
        case TOKEN_GreaterThan:
        case TOKEN_Subtraction:
        case TOKEN_And:
        case TOKEN_LogicalNot:
        case TOKEN_Xor:
        case TOKEN_BitwiseNot:
        case TOKEN_Assignment:
        case TOKEN_Semicolon:
        case TOKEN_Comma:
        case TOKEN_Bracket: {
            Color = 0xff809fb0;
        } break; 
        
        case TOKEN_SingleLineComment:
        case TOKEN_MultiLineComment: {
            Color = 0xff1f508f;
        } break;
        
    }
    return color_u32(Color);
}

void
draw_panel(framebuffer_t *Fb, panel_ctx_t *PanelCtx, panel_t *Panel, font_t *Font, irect_t PanelRect) {
    if(Panel->State == PANEL_STATE_Leaf) {
        irect_t TextRegion = text_buffer_rect(Panel, Font, PanelRect);
        
        Fb->Clip = PanelRect;
        
        buffer_t *Buf = Panel->Buffer;
        s32 LineHeight = Font->LineHeight;
        
        selection_group_t *SelGrp;
        // If we're in Select mode and there is a search term and there are selections in the working set then
        // we get the working set selection group instead of the one active in the buffer.
        if(Panel->Mode == MODE_Select && Panel->Select.SearchTerm.Used != 0 && sb_count(Panel->Select.SelectionGroup.Selections) != 0) {
            SelGrp = &Panel->Select.SelectionGroup;
        } else {
            SelGrp = get_selection_group(Panel->Buffer, Panel);
        }
        
        selection_t SelectionMaxIdx = get_selection_max_idx(SelGrp);
        
        // Compute Scroll
        if(sb_count(Panel->Buffer->Lines) > 0) {
            u64 Li = offset_to_line_index(Panel->Buffer, SelectionMaxIdx.Cursor);
            s32 OffsetY = (s32) Li * LineHeight;
            if(OffsetY >= (Panel->Scroll.y + TextRegion.h - LineHeight)) {
                Panel->Scroll.y = OffsetY - TextRegion.h + LineHeight;
            } else if(OffsetY < Panel->Scroll.y) {
                Panel->Scroll.y = OffsetY;
            }
            
            line_t *Line = &Panel->Buffer->Lines[Li];
            s32 OffsetX = text_width(Font, (range_t){.Data = Panel->Buffer->Text.Data + Line->Offset, .Size = SelectionMaxIdx.Cursor - Line->Offset});
            if(OffsetX >= (TextRegion.w + Panel->Scroll.x)) {
                Panel->Scroll.x = OffsetX - TextRegion.w + Font->MWidth;
            } else if(OffsetX < Panel->Scroll.x) {
                Panel->Scroll.x = OffsetX;
            }
        }
        
        for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
            selection_t *s = &SelGrp->Selections[i];
            draw_selection(Fb, Panel, s, Font, TextRegion);
            draw_cursor(Fb, Panel, s, Font, TextRegion, (s->Idx == SelGrp->SelectionIdxTop - 1) ? color_u32(0xffbbbbbb) : color_u32(0xff333333));
        }
        
        {
            token_t Token = {0};
            b8 HasTokens = next_token(Buf, Token, &Token);
            
            // Draw lines
            irect_t LineNumberRect = line_number_rect(Panel, Font, PanelRect);
            for(s64 i = 0; i < sb_count(Buf->Lines); ++i) {
                s32 LineY = TextRegion.y + (s32)i * LineHeight;
                s32 Center = LineY + (LineHeight >> 1);
                s32 Baseline = Center + (Font->MHeight >> 1) - Panel->Scroll.y;
                line_t *Line = &Buf->Lines[i];
                
                Fb->Clip = TextRegion;
                if(HasTokens) {
                    u64 Offset = Line->Offset;
                    while(HasTokens && Offset < (Line->Offset + Line->Size)) {
                        // Do we need a new token?
                        if(Offset >= (Token.Offset + Token.Size)) {
                            HasTokens = next_token(Buf, Token, &Token);
                            // TODO: Now we're skipping characters that aren't tokenized and fall between tokens. This is fine if we don't want
                            // to draw blank characters but simply advance the cursor, but it's a problem if the tokenizer incorrectly skips characters.
                            Offset = Token.Offset;
                        } else {
                            u64 End = MIN(Token.Offset + Token.Size, Line->Offset + Line->Size); // Don't write past this line if token spans multiple lines
                            range_t Text = {.Data = Buf->Text.Data + Offset, .Size = End - Offset};
                            
                            color_t Color = token_color(Token);
                            s32 StartX = text_width(Font, (range_t){.Data = Buf->Text.Data + Line->Offset, .Size = Offset - Line->Offset});
                            
                            draw_text_line(Fb, Font, TextRegion.x - Panel->Scroll.x + StartX, Baseline, Color, Text);
                            Offset = End;
                        }
                    }
                }
                
                Fb->Clip = PanelRect;
                s64 n = ABS(i - (s64)offset_to_line_index(Buf, SelectionMaxIdx.Cursor));
                draw_line_number(Fb, Font, (irect_t){.x = LineNumberRect.x, .y = Baseline, .w = LineNumberRect.w, .h = LineHeight}, n == 0 ? COLOR_LINE_NUMBER_CURRENT : COLOR_LINE_NUMBER, n == 0 ? i + 1 : n);
            }
        }
        
        irect_t StatusBar = status_bar_rect(PanelRect);
        draw_rect(Fb, StatusBar, COLOR_STATUS_BAR);
        
        irect_t r0, r1, r2, r3;
        panel_border_rects(PanelRect, &r0, &r1, &r2, &r3);
        
        color_t BorderColor = COLOR_BG;
        if(PanelCtx->Selected == Panel) {
            BorderColor = COLOR_PANEL_SELECTED;
        } else if(Panel != PanelCtx->Root && Panel->Parent->LastSelectedChild == panel_child_index(Panel)) {
            BorderColor = COLOR_PANEL_LAST_SELECTED;
        } 
        draw_rect(Fb, r0, BorderColor);
        draw_rect(Fb, r1, BorderColor);
        draw_rect(Fb, r2, BorderColor);
        draw_rect(Fb, r3, BorderColor);
        
        color_t ModeStrColor = color_u32(0xffffffff);
        mem_buffer_t ModeString = {0};
        switch(Panel->Mode) {
            case MODE_Normal: {
                mem_buf_append_range(&ModeString, C_STR_AS_RANGE("NORMAL"));
                ModeStrColor = COLOR_STATUS_NORMAL;
            } break;
            
            case MODE_Insert: {
                mem_buf_append_range(&ModeString, C_STR_AS_RANGE("INSERT"));
                ModeStrColor = COLOR_STATUS_INSERT;
            } break;
            
            case MODE_Select: {
                mem_buf_append_range(&ModeString, C_STR_AS_RANGE("Select:"));
                range_t r = mem_buf_as_range(Panel->Select.SearchTerm);
                mem_buf_append_range(&ModeString, r);
            } break;
            
            default: {
                mem_buf_append_range(&ModeString, C_STR_AS_RANGE("Invalid mode"));
            } break;
        }
        
        iv2_t TextPos = center_text_in_rect(Font, StatusBar, mem_buf_as_range(ModeString));
        draw_text_line(Fb, Font, TextPos.x, TextPos.y, ModeStrColor, mem_buf_as_range(ModeString));
        draw_text_line(Fb, Font, StatusBar.x, TextPos.y, ModeStrColor, (Panel->Split == SPLIT_Vertical) ? C_STR_AS_RANGE("|") : C_STR_AS_RANGE("_"));
    } else {
        irect_t r0, r1;
        split_panel_rect(Panel, PanelRect, &r0, &r1);
        draw_panel(Fb, PanelCtx, Panel->Children[0], Font, r0);
        draw_panel(Fb, PanelCtx, Panel->Children[1], Font, r1);
    }
}

irect_t 
workspace_rect(framebuffer_t *Fb) {
    irect_t Rect = {0, 0, Fb->Width, Fb->Height};
    return Rect;
}

void
draw_panels(framebuffer_t *Fb, panel_ctx_t *PanelCtx, font_t *Font, irect_t Rect) {
    irect_t ClipSave = Fb->Clip;
    if(PanelCtx->Root) {
        draw_panel(Fb, PanelCtx, PanelCtx->Root, Font, Rect);
    }
    Fb->Clip = ClipSave;
}