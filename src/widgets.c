b8
ui_begin_tree_node(ui_ctx_t *Ctx, range_t Name) {
    ui_id_t Id = ui_make_id(Ctx, Name);
    
    b8 *Collapsed = (b8 *)ui_get_noodle(Ctx, Id);
    
    irect_t Rect = ui_push_rect(Ctx, text_width(Ctx->Font, Name), Ctx->Font->LineHeight);
    
    ui_interaction_type_t Interaction = ui_update_input_state(Ctx, Rect, Id);
    if(Interaction == UI_INTERACTION_PressAndRelease) {
        *Collapsed = !(*Collapsed);
    }
    ui_draw_text(Ctx, Name, center_text_in_rect(Ctx->Font, Rect, Name), color_u32(0xfffffff));
    
    if(*Collapsed) {
        ui_get_layout(Ctx)->Indent.x += 30;
    }
    
    return (*Collapsed);
}

void
ui_end_tree_node(ui_ctx_t *Ctx) {
    ui_get_layout(Ctx)->Indent.x -= 30;
}

b8
ui_selectable(ui_ctx_t *Ctx, range_t Name, b8 Selected) {
    ui_container_t *Container = Ctx->ActiveContainer;
    
    ui_id_t Id = ui_make_id(Ctx, Name);
    irect_t Rect = ui_push_rect(Ctx, 0, Ctx->Font->LineHeight);
    
    color_t TextColor = color_u32(0xffaaaaaa);
    
    ui_interaction_type_t Interaction = ui_update_input_state(Ctx, Rect, Id);
    if(Ctx->Hot == Id || Ctx->Active == Id || Ctx->Selected == Id) {
        TextColor = color_u32(0xffffffff);
    }
    
    if(Selected) {
        ui_draw_rect(Ctx, Rect, color_u32(0xff3a1825));
        TextColor = color_u32(0xffffffff);
    }
    
    
    iv2_t Baseline = center_text_in_rect(Ctx->Font, Rect, Name);
    Baseline.x = Rect.x + 5;
    ui_draw_text(Ctx, Name, Baseline, TextColor);
    
    return (Interaction == UI_INTERACTION_PressAndRelease);
}

b8
ui_button(ui_ctx_t *Ctx, range_t Name) {
    ui_container_t *Container = Ctx->ActiveContainer;
    ui_id_t Id = ui_make_id(Ctx, Name); 
    irect_t Rect = ui_push_rect(Ctx, 0, Ctx->Font->LineHeight + 8);
    
    color_t Color = color_u32(0xff393a49);
    ui_interaction_type_t Interaction = ui_update_input_state(Ctx, Rect, Id);
    if(Interaction == UI_INTERACTION_Hover) {
        Color = color_u32(0xff5e5e68);
    } else if(Ctx->Active == Id) {
        Color = color_u32(0xff282933);
    }
    
    ui_draw_rect(Ctx, Rect, Color);
    iv2_t Baseline = center_text_in_rect(Ctx->Font, Rect, Name);
    ui_draw_text(Ctx, Name, Baseline, color_u32(0xffffffff));
    
    return (Interaction == UI_INTERACTION_PressAndRelease);
    
}

typedef enum {
    TEXT_BOX_None = 0,
    TEXT_BOX_Modified,
    TEXT_BOX_Submit,
} ui_text_box_interaction_t;

#define UI_TEXT_BOX_CALLBACK(Name) ui_text_box_interaction_t Name(ui_ctx_t *Ctx, mem_buffer_t *TextBuffer)
typedef UI_TEXT_BOX_CALLBACK(ui_text_box_callback_t);

UI_TEXT_BOX_CALLBACK(ui_default_text_box_callback) {
    ui_text_box_interaction_t Interaction = TEXT_BOX_None;
    for(u32 i = 0; i < Ctx->Input->Keys.Count; ++i) {
        key_input_t *Ki = &Ctx->Input->Keys.Items[i];
        if(!Ki->Handled) {
            if(Ki->IsText) {
                if(*Ki->Char < 0x20) {
                    if(*Ki->Char == '\b') {
                        if(TextBuffer->Used > 0) {
                            u8 *c = utf8_move_back_one(TextBuffer->Data + TextBuffer->Used);
                            mem_buf_pop_size(TextBuffer, utf8_char_width(c));
                            Interaction = TEXT_BOX_Modified;
                            Ki->Handled = true;
                        }
                    } else if(*Ki->Char == '\n') {
                        Interaction = TEXT_BOX_Submit;
                        Ki->Handled = true;
                    }
                } else {
                    mem_buf_append(TextBuffer, Ki->Char, utf8_char_width(Ki->Char));
                    Interaction = TEXT_BOX_Modified;
                    Ki->Handled = true;
                }
            } else {
                if(Ki->Key == KEY_Escape) {
                    if(Ctx->Selected == Ctx->TextEditFocus) {
                        Ctx->Selected = 0;
                    }
                    Ctx->TextEditFocus = 0;
                    
                    Ki->Handled = true;
                }
            }
        }
    }
    return Interaction;
}

ui_text_box_interaction_t
ui_text_box_ex(ui_ctx_t *Ctx, mem_buffer_t *TextBuffer, range_t Name, range_t Hint, b32 GrabFocus, ui_text_box_callback_t *Callback) {
    ui_container_t *Container = Ctx->ActiveContainer;
    ui_id_t Id = ui_make_id(Ctx, Name);
    s32 LineHeight = Ctx->Font->LineHeight;
    irect_t Rect = ui_push_rect(Ctx, 0, LineHeight);
    ui_update_input_state(Ctx, Rect, Id);
    if(Ctx->Active == Id || Ctx->Selected == Id || (GrabFocus && (Container->FrameOpened == Ctx->Frame))) {
        Ctx->TextEditFocus = Id;
    }
    
    ui_text_box_interaction_t Interaction = TEXT_BOX_None;
    if(Ctx->TextEditFocus == Id) {
        Interaction = Callback(Ctx, TextBuffer);
    }
    
    ui_draw_rect(Ctx, Rect, color_u32(0xff000000));
    
    range_t TextRange = mem_buf_as_range(*TextBuffer);
    s32 Width = text_width(Ctx->Font, TextRange);
    s32 CenterY = Rect.y + (Rect.h + Ctx->Font->MHeight) / 2;
    if(TextRange.Size > 0) {
        ui_draw_text(Ctx, TextRange, (iv2_t){.x = Rect.x + 5, .y = CenterY}, color_u32(0xffffffff));
    } else {
        if(Hint.Size > 0) {
            ui_draw_text_ex(Ctx, Hint, (iv2_t){.x = Rect.x + 5, .y = CenterY}, color_u32(0xff7f7f7f), false);
        }
    }
    
    if(Ctx->TextEditFocus == Id) {
        ui_draw_rect(Ctx, (irect_t){.x = Rect.x + 5 + Width, .y = Rect.y + 1, .w = 2, .h = LineHeight - 2}, color_u32(0xffffffff));
    }
    return Interaction;
}

ui_text_box_interaction_t
ui_text_box(ui_ctx_t *Ctx, mem_buffer_t *TextBuffer, range_t Name, range_t Hint, b32 GrabFocus) {
    return ui_text_box_ex(Ctx, TextBuffer, Name, Hint, GrabFocus, ui_default_text_box_callback);
}

void
ui_draw_cursor(ui_ctx_t *Ctx, panel_t *Panel, irect_t TextRegion, selection_group_t *SelGrp, selection_t *Selection) {
    s64 LineIdx = offset_to_line_index(Panel->Buffer, Selection->Cursor);
    s32 YPosRelative = (s32)LineIdx * Ctx->Font->LineHeight - Panel->Scroll.y;
    // Skip drawing cursors that are not in view.
    if((YPosRelative + Ctx->Font->LineHeight) < 0 && YPosRelative > TextRegion.h) {
        return;
    }
    
    s32 CursorWidth = Ctx->Font->MWidth;
    if(Selection->Cursor < Panel->Buffer->Text.Used) {
        u8 *CharUnderCursor = Panel->Buffer->Text.Data + Selection->Cursor;
        CursorWidth = text_width(Ctx->Font, (range_t){.Data = CharUnderCursor, .Size = utf8_char_width(CharUnderCursor)});
    }
    line_t *Line = Panel->Buffer->Lines + LineIdx;
    s32 TextWidthToCursor = text_width(Ctx->Font, (range_t){.Data = Panel->Buffer->Text.Data + Line->Offset,.Size = Selection->Cursor - Line->Offset});
    
    irect_t CursorBox = {
        .x = TextRegion.x + TextWidthToCursor - Panel->Scroll.x, .y = TextRegion.y + YPosRelative, 
        .w = CursorWidth, .h = Ctx->Font->LineHeight};
    
    color_t Color = COLOR_CURSOR;
    if(Selection->Idx == (SelGrp->SelectionIdxTop - 1)) {
        Color = COLOR_MAIN_CURSOR;
    }
    
    ui_draw_rect(Ctx, CursorBox, Color);
}

void
ui_draw_selection(ui_ctx_t *Ctx, panel_t *Panel, irect_t TextRegion, selection_t *Selection) {
    buffer_t *Buf = Panel->Buffer;
    
    u64 SelectionStart = selection_start(Selection);
    u64 SelectionEnd = selection_end(Selection);
    if(Buf->Text.Used > 0) {
        SelectionEnd += utf8_char_width(Buf->Text.Data + SelectionEnd);
    }
    
    u64 LineIndexStart = offset_to_line_index(Buf, SelectionStart);
    u64 LineIndexEnd = offset_to_line_index(Buf, SelectionEnd);
    
    for(u64 i = LineIndexStart; i <= LineIndexEnd; ++i) {
        s32 y = TextRegion.y + (s32)i * Ctx->Font->LineHeight;
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
        
        s32 Width = text_width(Ctx->Font, (range_t){.Data = Buf->Text.Data + Start, .Size = Size});
        
        // There is one exception where the selection is allowed to go outside the line and it is on the last "imaginary" character on the 
        // last line in the file. The reason the cursor can go one byte outside like this is so that one can append to the end of the file.
        // Because this one-past-the-end byte could be anything we don't want to include it in the string we pass to text_width, so to handle
        // this we check if the cursor is past the end of the line and we're on the last line and then increase the size by the width of 'M'.
        if(i == (u64)(sb_count(Buf->Lines) - 1) && SelectionEnd > Line->Offset + Line->Size) {
            Width += Ctx->Font->MWidth;
        } 
        
        s32 PixelOffsetToSelectionStart = text_width(Ctx->Font, (range_t){.Data = Buf->Text.Data + Line->Offset, .Size = Start - Line->Offset});
        irect_t LineRect = {
            .x = TextRegion.x + PixelOffsetToSelectionStart - Panel->Scroll.x, .y = y - Panel->Scroll.y,
            .w = Width, .h = Ctx->Font->LineHeight};
        if((LineRect.y + Ctx->Font->LineHeight) > TextRegion.y && LineRect.y < (TextRegion.y + TextRegion.h)) {
            ui_draw_rect(Ctx, LineRect, COLOR_SELECTION);
        }
    }
}

void
ui_draw_panel(ui_ctx_t *Ctx, panel_t *Panel, irect_t Rect) {
    ASSERT(Panel->State == PANEL_STATE_Leaf);
    ASSERT(Panel->Buffer);
    buffer_t *Buf = Panel->Buffer;
    
    ui_push_clip(Ctx, Rect);
    ui_draw_rect(Ctx, Rect, COLOR_BG);
    irect_t TextRegion = Rect;
    
    selection_group_t *SelGrp;
    if(Panel->Mode == MODE_Select && Panel->Select.SearchTerm.Used > 0 && sb_count(Panel->Select.SelectionGroup.Selections) > 0) {
        SelGrp = &Panel->Select.SelectionGroup;
    } else {
        SelGrp = get_selection_group(Panel->Buffer, Panel);
        ASSERT(SelGrp);
    }
    
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Selection = SelGrp->Selections + i;
        ui_draw_selection(Ctx, Panel, TextRegion, Selection);
        ui_draw_cursor(Ctx, Panel, TextRegion, SelGrp, Selection);
    }
    
    // TODO: Can we fast forward to the lines that are in view?
    for(s64 LineIdx = 0; LineIdx < sb_count(Buf->Lines); ++LineIdx) {
        line_t *Line = Buf->Lines + LineIdx;
        iv2_t Baseline = {.x = TextRegion.x, .y = 0};
        s32 LineY = TextRegion.y + (s32)LineIdx * Ctx->Font->LineHeight;
        s32 LineCenter = LineY + Ctx->Font->LineHeight / 2;
        Baseline.y = LineCenter + (Ctx->Font->MHeight / 2) - Panel->Scroll.y;
        
        if((Baseline.y + Ctx->Font->LineHeight) < TextRegion.y) {
            continue;
        }
        
        if(Baseline.y > (TextRegion.y + TextRegion.h)) {
            break;
        }
        
        range_t Text = {.Data = Buf->Text.Data + Line->Offset, .Size = Line->Size};
        ui_draw_text_ex(Ctx, Text, Baseline, COLOR_TEXT, false);
    }
    
    ui_pop_clip(Ctx);
}

void
ui_panels(ui_ctx_t *Ctx, panel_t *Panel, irect_t Rect) {
    switch(Panel->State) {
        case PANEL_STATE_Parent: {
            irect_t r0, r1;
            split_panel_rect(Panel, Rect, &r0, &r1);
            ui_panels(Ctx, Panel->Children[0], r0);
            ui_panels(Ctx, Panel->Children[1], r1);
        } break;
        
        case PANEL_STATE_Leaf: {
            ui_draw_panel(Ctx, Panel, Rect);
        } break;
    }
}