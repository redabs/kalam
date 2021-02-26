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
    irect_t Rect = ui_push_rect(Ctx, -1, Ctx->Font->LineHeight);
    
    color_t Color = color_u32(0xdd2c302d);
    
    ui_interaction_type_t Interaction = ui_update_input_state(Ctx, Rect, Id);
    if(Interaction == UI_INTERACTION_Hover || Selected) {
        Color = color_u32(0xff8a8a8a);
    } else if(Ctx->Active == Id) {
        Color = color_u32(0xffffffff);
    }
    
    ui_draw_rect(Ctx, Rect, Color);
    iv2_t Baseline = center_text_in_rect(Ctx->Font, Rect, Name);
    Baseline.x = Rect.x + 5;
    ui_draw_text(Ctx, Name, Baseline, color_u32(0xffffffff));
    
    return (Interaction == UI_INTERACTION_PressAndRelease);
}

b8
ui_button(ui_ctx_t *Ctx, range_t Name) {
    ui_container_t *Container = Ctx->ActiveContainer;
    ui_id_t Id = ui_make_id(Ctx, Name); 
    irect_t Rect = ui_push_rect(Ctx, -1, Ctx->Font->LineHeight + 8);
    
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
    for(u32 i = 0; i < Ctx->KeyInputCount; ++i) {
        ui_key_input_t *Ki = &Ctx->KeyInput[i];
        if(!Ki->Handled && Ki->IsText) {
            u8 *Char = Ki->Text;
            if(*Char < 0x20) {
                if(*Char == '\b') {
                    if(TextBuffer->Used > 0) {
                        u8 *c = utf8_move_back_one(TextBuffer->Data + TextBuffer->Used);
                        mem_buf_pop_size(TextBuffer, utf8_char_width(c));
                        Interaction = TEXT_BOX_Modified;
                        Ki->Handled = true;
                    }
                } else if(*Char == '\n') {
                    Interaction = TEXT_BOX_Submit;
                    Ki->Handled = true;
                }
            } else {
                mem_buf_append(TextBuffer, Char, utf8_char_width(Char));
                Interaction = TEXT_BOX_Modified;
                Ki->Handled = true;
            }
        }
    }
    
    return Interaction;
}

ui_text_box_interaction_t
ui_text_box_ex(ui_ctx_t *Ctx, mem_buffer_t *TextBuffer, range_t Name, b32 GrabFocus, ui_text_box_callback_t *Callback) {
    ui_container_t *Container = Ctx->ActiveContainer;
    ui_id_t Id = ui_make_id(Ctx, Name);
    s32 LineHeight = Ctx->Font->LineHeight;
    irect_t Rect = ui_push_rect(Ctx, -1, LineHeight);
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
    ui_draw_text(Ctx, TextRange, (iv2_t){.x = Rect.x + 5, .y = CenterY}, color_u32(0xffffffff));
    
    if(Ctx->TextEditFocus == Id) {
        ui_draw_rect(Ctx, (irect_t){.x = Rect.x + 5 + Width, .y = Rect.y + 1, .w = 2, .h = LineHeight - 2}, color_u32(0xffffffff));
    }
    return Interaction;
}

ui_text_box_interaction_t
ui_text_box(ui_ctx_t *Ctx, mem_buffer_t *TextBuffer, range_t Name, b32 GrabFocus) {
    return ui_text_box_ex(Ctx, TextBuffer, Name, GrabFocus, ui_default_text_box_callback);
}