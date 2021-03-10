void
ui_init(ui_ctx_t *Ctx, font_t *Font) {
    Ctx->Pool.Table = table_create();
    for(u32 i = 0; i < UI_NOODLE_MAX; ++i) {
        ui_noodle_t *n = Ctx->Pool.Noodles + i;
        n->Next = n + 1;
    }
    Ctx->Pool.Noodles[UI_NOODLE_MAX - 1].Next = 0;
    Ctx->Pool.Free = Ctx->Pool.Noodles;
    
    Ctx->Font = Font;
}

ui_cmd_t *

ui_push_draw_cmd(ui_ctx_t *Ctx, ui_cmd_t Cmd) {
    ASSERT((Ctx->CmdBuf.Used + Cmd.Size) < UI_COMMAND_BUFFER_SIZE);
    mem_copy(Ctx->CmdBuf.Data + Ctx->CmdBuf.Used, &Cmd, Cmd.Size);
    
    ui_cmd_t *Result = (ui_cmd_t *)(Ctx->CmdBuf.Data + Ctx->CmdBuf.Used);
    Ctx->CmdBuf.Used += Cmd.Size; 
    
    return Result;
}

void
ui_push_clip(ui_ctx_t *Ctx, irect_t Rect) {
    ui_cmd_t Cmd = {0};
    Cmd.Type = UI_CMD_Clip;
    Cmd.Size = sizeof(ui_cmd_t);
    
    if(Ctx->LastClip) {
        Cmd.Clip.Rect = rect_overlap(Rect, Ctx->LastClip->Rect);
    } else {
        Cmd.Clip.Rect = Rect;
    }
    
    Cmd.Clip.Prev = Ctx->LastClip;
    Ctx->LastClip = &(ui_push_draw_cmd(Ctx, Cmd)->Clip);
}

void
ui_pop_clip(ui_ctx_t *Ctx) {
    ui_cmd_t Cmd = {0};
    Cmd.Type = UI_CMD_Clip;
    Cmd.Size = sizeof(ui_cmd_t);
    if(Ctx->LastClip->Prev) {
        Cmd.Clip.Rect = Ctx->LastClip->Prev->Rect;
    } 
    Cmd.Clip.Prev = 0;
    
    Ctx->LastClip = Ctx->LastClip->Prev;
    ui_push_draw_cmd(Ctx, Cmd);
}

void
ui_draw_rect(ui_ctx_t *Ctx, irect_t Rect, color_t Color) {
    ui_cmd_t Cmd = {.Size = sizeof(ui_cmd_t), .Type = UI_CMD_Rect, .Rect.Rect = Rect, .Rect.Color = Color};
    ui_push_draw_cmd(Ctx, Cmd);
}

// Internal text commands store a copy of the text that is to be draw in the command itself.
// External text commands (Internal == false) stores a copy of the Text.Data pointer. Meaning
// the pointer has to still point to valid memory when drawing!
void
ui_draw_text_ex(ui_ctx_t *Ctx, range_t Text, iv2_t Baseline, color_t Color, b32 Internal) {
    u64 CmdSize = sizeof(ui_cmd_t) + (Internal ? Text.Size : 0);
    ui_cmd_t *Cmd = ui_push_draw_cmd(Ctx, (ui_cmd_t){.Size = CmdSize, .Type = UI_CMD_Text});
    Cmd->Text.Size = Text.Size;
    Cmd->Text.Baseline = Baseline;
    Cmd->Text.Color = Color;
    if(Internal) {
        Cmd->Text.Text = (u8 *)&Cmd->Text.Text + sizeof(Cmd->Text.Text); 
    } else {
        Cmd->Text.Text = Text.Data;
    }
    mem_copy(Cmd->Text.Text, Text.Data, Text.Size);
}

void
ui_draw_text(ui_ctx_t *Ctx, range_t Text, iv2_t Baseline, color_t Color) {
    ui_draw_text_ex(Ctx, Text, Baseline, Color, true);
}

void
ui_begin(ui_ctx_t *Ctx) {
    Ctx->CmdBuf.Used = 0;
    Ctx->LastClip = 0;
    Ctx->Hot = 0;
}

ui_id_t 
ui_hash_ex(ui_id_t Id, range_t Data) {
    return fnv1a_32_ex((u32)Id, Data);
}

ui_id_t
ui_hash(range_t Data) {
    ui_id_t Id = fnv1a_32(Data);
    return Id;
}

void
ui_end(ui_ctx_t *Ctx) {
    ASSERT(Ctx->ActiveContainer == 0);
    ASSERT(Ctx->LastClip == 0);
    Ctx->Frame += 1;
    
    if(!(Ctx->Input->MouseDown & MOUSE_Left)) {
        Ctx->Active = 0;
    }
    
    Ctx->Input->MousePress = 0;
    Ctx->Input->Scroll = 0;
}

ui_id_t
ui_make_id(ui_ctx_t *Ctx, range_t Name) {
    ui_id_t Id;
    if(Ctx->ActiveContainer) {
        Id = ui_hash_ex(Ctx->ActiveContainer->Id, Name);
    } else {
        Id = ui_hash(Name);
    }
    
    return Id;
}

ui_container_t *
ui_find_container(ui_ctx_t *Ctx, range_t Name) {
    ui_id_t Id = ui_hash(Name);
    for(u64 i = 0; i < UI_CONTAINER_MAX; ++i) {
        ui_container_t *c = &Ctx->Containers[i];
        if(c->Id == Id && (c->Flags & UI_CNT_Open)) {
            return c;
        }
    }
    return 0;
}

void
ui_put_container_on_top(ui_ctx_t *Ctx, ui_container_t *Container) {
    if(!(Container->Flags & UI_CNT_Open)) { 
        return; 
    }
    
    // There already is a pointer to it in the ContainersByDepth array.
    for(u64 i = 0; i < Ctx->OpenContainerCount; ++i) {
        if(Ctx->ContainerStack[i] == Container) {
            for(u64 j = 1; j < i + 1; ++j) {
                Ctx->ContainerStack[j - 1] = Ctx->ContainerStack[j];
            }
            Ctx->ContainerStack[0] = Container;
            break;
        }
    }
}

ui_container_t *
ui_container_under_point(ui_ctx_t *Ctx, iv2_t Point) {
    for(u32 i = 0; i < Ctx->OpenContainerCount; ++i) {
        ui_container_t *c = Ctx->ContainerStack[i];
        ASSERT(c->Flags & UI_CNT_Open);
        if(is_point_inside_rect(c->Rect, Point)) {
            return c;
        }
    }
    
    return 0;
}

ui_container_t *
ui_allocate_container(ui_ctx_t *Ctx) {
    if(Ctx->OpenContainerCount >= UI_CONTAINER_MAX) {
        return 0;
    }
    
    ui_container_t *Container = 0;
    for(u32 i = 0; i < UI_CONTAINER_MAX; ++i) {
        ui_container_t *c = &Ctx->Containers[i];
        if(!(c->Flags & UI_CNT_Open)) {
            Container = c;
            break;
        }
    }
    
    ASSERT(Container);
    mem_zero_struct(Container);
    return Container;
}

ui_container_t *
ui_open_container(ui_ctx_t *Ctx, range_t Name, b8 IsPopUp) {
    ui_container_t *Container = ui_find_container(Ctx, Name);
    if(Container) {
        if(Container->Flags & UI_CNT_Open) {
            return Container;
        }
    } else {
        ASSERT(Ctx->OpenContainerCount < UI_CONTAINER_MAX);
        Container = ui_allocate_container(Ctx);
        Container->Id = ui_hash(Name);
    }
    
    Container->FrameOpened = Ctx->Frame;
    Container->Flags |= IsPopUp ? UI_CNT_PopUp : 0;
    Container->Flags |= UI_CNT_Open;
    
    // Put at the top of container stack
    for(u32 i = Ctx->OpenContainerCount; i > 0 ; --i) {
        Ctx->ContainerStack[i] = Ctx->ContainerStack[i - 1];
    }
    Ctx->ContainerStack[0] = Container;
    Ctx->OpenContainerCount += 1;
    
    Ctx->Focus = Container->Id;
    
    return Container;
}

void
ui_close_container(ui_ctx_t *Ctx, ui_container_t *Container) {
    Container->Flags &= ~UI_CNT_Open;
    for(u32 i = 0; i < Ctx->OpenContainerCount; ++i) {
        if(Ctx->ContainerStack[i] == Container) {
            for(u32 j = i; j < Ctx->OpenContainerCount - 1; ++j) {
                Ctx->ContainerStack[j] = Ctx->ContainerStack[j + 1];
            }
            break;
        }
    }
    Ctx->OpenContainerCount -= 1;
    
    if(Ctx->Focus == Container->Id) {
        Ctx->Focus = (Ctx->OpenContainerCount > 0) ? Ctx->ContainerStack[0]->Id : 0;
    }
}

ui_interaction_type_t
ui_update_input_state(ui_ctx_t *Ctx, irect_t Rect, ui_id_t Id) {
    ui_interaction_type_t Result = UI_INTERACTION_None;
    ui_container_t *Container = Ctx->ActiveContainer;
    if(!Container) {
        return Result;
    }
    
    irect_t RelativeToContainer = {
        .x = Rect.x - Container->Rect.x - Container->Scroll.x,
        .y = Rect.y - Container->Rect.y - Container->Scroll.y,
        .w = Rect.w, .h = Rect.h};
    ui_control_t Ctrl = {.Rect = RelativeToContainer, .Id = Id};
    MEM_STACK_PUSH(Container->ControlStack, Ctrl);
    
    if(!Ctx->Active) {
        ui_container_t *CntUnderMouse = ui_container_under_point(Ctx, Ctx->Input->MousePos);
        if(CntUnderMouse == Container && is_point_inside_rect(Rect, Ctx->Input->MousePos)) {
            Ctx->Hot = Id;
            Result = UI_INTERACTION_Hover;
        }
    }
    
    if(Ctx->Selected == Id) {
        Result = UI_INTERACTION_Hover;
        for(u32 i = 0; i < Ctx->Input->Keys.Count; ++i) {
            key_input_t *Ki = Ctx->Input->Keys.Items + i;
            if(!Ki->Handled && Ki->IsText && Ki->Char[0] == '\n') {
                Result = UI_INTERACTION_PressAndRelease;
            }
        }
    }
    
    if(Ctx->Active == Id) {
        if(!(Ctx->Input->MouseDown & MOUSE_Left)) {
            if(is_point_inside_rect(Rect, Ctx->Input->MousePos)) {
                Result = UI_INTERACTION_PressAndRelease;
            }
            Ctx->Active = 0;
        } 
    } else if(Ctx->Hot == Id) {
        if(Ctx->Input->MousePress & MOUSE_Left) {
            if(is_point_inside_rect(Rect, Ctx->Input->MousePos)) {
                Ctx->Hot = 0;
                Ctx->Selected = 0;
                Ctx->Active = Id;
                Result = UI_INTERACTION_Press;
            }
        } 
    }
    
    return Result;
}

ui_layout_t *
ui_get_layout(ui_ctx_t *Ctx) {
    ASSERT(Ctx->LayoutStack.Count > 0);
    return &Ctx->LayoutStack.Items[Ctx->LayoutStack.Count - 1];
}

void
ui_next_row(ui_layout_t *Layout) {
    Layout->Cursor.x = Layout->Rect.x;
    Layout->Rect.h += Layout->RowHeight;
    Layout->Cursor.y += Layout->RowHeight;
    Layout->RowHeight = 0;
}

// if Width|Height <= 0 then Rect.w|h = (parent's layout width|height) + (Width|height)
irect_t
ui_push_rect(ui_ctx_t *Ctx, s32 Width, s32 Height) {
    ui_container_t *Container = Ctx->ActiveContainer;
    ui_layout_t *Layout = ui_get_layout(Ctx);
    if(Width <= 0) {
        s32 RemainingWidth = Layout->Rect.w - (Layout->Cursor.x - Layout->Rect.x + Layout->Indent.x);
        s32 RemainingSlots = Layout->ItemsPerRow - Layout->ItemCount;
        s32 W = RemainingWidth / MAX(RemainingSlots, 1);
        Width = MAX(W + Width, 0);
    }
    
    if(Height <= 0) {
        // Layouts are expanded vertically to fit their content. 
        s32 H;
        // Use parent's height
        if(Ctx->LayoutStack.Count > 1) {
            H = Ctx->LayoutStack.Items[Ctx->LayoutStack.Count - 2].Rect.h;
        } else {
            H = Container->Rect.h;
        }
        Height = H + Height;
    }
    // The cursor does not move in Y until we move to the next row.
    // Therefore we need to keep track of the tallest control rect 
    // on the current row such that we can advance in Y appropriately.
    Layout->RowHeight = MAX(Layout->RowHeight, Height);
    
    irect_t Result = {
        .x = Layout->Cursor.x + Container->Rect.x + Container->Scroll.x + Layout->Indent.x,
        .y = Layout->Cursor.y + Container->Rect.y + Container->Scroll.y + Layout->Indent.y,
        .w = Width, .h = Height};
    
    Layout->Cursor.x += Width;
    
    Layout->ItemCount += 1;
    if(Layout->ItemCount == Layout->ItemsPerRow) {
        ui_next_row(Layout);
        Layout->ItemCount = 0;
    }
    
    return Result;
}

void
ui_push_layout(ui_ctx_t *Ctx, s32 Width) {
    ui_layout_t ActiveLayout = *ui_get_layout(Ctx);
    irect_t Rect = ui_push_rect(Ctx, Width, 0);
    Rect.x -= Ctx->ActiveContainer->Rect.x;
    Rect.y -= Ctx->ActiveContainer->Rect.y;
    *ui_get_layout(Ctx) = ActiveLayout;
    
    ui_layout_t NewLayout = {.Rect = Rect, .Cursor = {.x = Rect.x, .y = Rect.y}, .ItemsPerRow = 1};
    MEM_STACK_PUSH(Ctx->LayoutStack, NewLayout);
}

ui_layout_t
ui_pop_layout(ui_ctx_t *Ctx) {
    ui_layout_t Layout = MEM_STACK_POP(Ctx->LayoutStack);
    ui_push_rect(Ctx, Layout.Rect.w, Layout.Rect.h);
    return Layout;
}

ui_container_t *
ui_begin_container(ui_ctx_t *Ctx, irect_t Rect, range_t Name) {
    ui_container_t *Container = ui_find_container(Ctx, Name);
    if(!Container || !(Container->Flags & UI_CNT_Open)) {
        return 0;
    }
    
    ASSERT(!Ctx->ActiveContainer); 
    
    if(Container->Flags & UI_CNT_PopUp) {
        if(Ctx->Input->MousePress & MOUSE_Left) {
            ui_container_t *PressedContainer = ui_container_under_point(Ctx, Ctx->Input->MousePos);
            if(PressedContainer != Container) {
                ui_close_container(Ctx, Container);
                return 0;
            }
        }
    }
    
    Container->Rect = Rect;
    ui_layout_t Layout = {.Rect.w = Rect.w, .Rect.h = Rect.h, .ItemsPerRow = 1};
    MEM_STACK_PUSH(Ctx->LayoutStack, Layout);
    
    Container->Prev = Ctx->ActiveContainer;
    Ctx->ActiveContainer = Container;
    
    return Container;
}

void
ui_row(ui_ctx_t *Ctx, u32 Count) {
    ui_layout_t *Layout = ui_get_layout(Ctx);
    if(Layout->ItemCount > 0) {
        ui_next_row(Layout);
    }
    Layout->ItemCount = 0;
    Layout->ItemsPerRow = Count;
}

void
ui_begin_column(ui_ctx_t *Ctx, s32 Width) {
    ui_push_layout(Ctx, Width);
    ui_layout_t *Layout = ui_get_layout(Ctx);
    ui_container_t *Cnt = Ctx->ActiveContainer;
    ui_push_clip(Ctx, (irect_t){Cnt->Rect.x + Layout->Rect.x, Cnt->Rect.y + Layout->Rect.y, Layout->Rect.w, Cnt->Rect.h});
}

void
ui_end_column(ui_ctx_t *Ctx) {
    ui_pop_clip(Ctx);
    ui_pop_layout(Ctx);
}

void
ui_end_container(ui_ctx_t *Ctx) {
    MEM_STACK_POP(Ctx->LayoutStack);
    ui_container_t *Container = Ctx->ActiveContainer;
    ASSERT(Container);
    
    if(Ctx->Focus == Container->Id && Container->ControlStack.Count > 0) {
        for(u32 i = 0; i < Ctx->Input->Keys.Count; ++i) {
            key_input_t *Ki = &Ctx->Input->Keys.Items[i];
            b8 Forward = (Ki->Key == KEY_Tab || Ki->Key == KEY_Down);
            b8 Backward = (Ki->Key == KEY_Tab && Ki->Mods == MOD_Shift) || (Ki->Key == KEY_Up);
            if(!Ki->Handled && !Ki->IsText && (Backward || Forward)) {
                if(!Ctx->Selected) {
                    Ctx->Selected = Container->ControlStack.Items[0].Id;
                    Container->ControlSelected = 0;
                } else {
                    if(Backward) {
                        Container->ControlSelected += (Container->ControlSelected == 0) ? (u32)Container->ControlStack.Count - 1 : -1;
                    } else {
                        Container->ControlSelected = (Container->ControlSelected + 1) % Container->ControlStack.Count;
                    }
                    
                    Ctx->TextEditFocus = 0;
                    Ctx->Selected = Container->ControlStack.Items[Container->ControlSelected].Id;
                    
                    // Scroll the control into view
                    ui_control_t *Ctrl = Container->ControlStack.Items + Container->ControlSelected;
                    s32 CtrlBottom = (Ctrl->Rect.y + Ctrl->Rect.h);
                    s32 CntBottom = (Container->Rect.h - Container->Scroll.y);
                    if(CtrlBottom > CntBottom) {
                        Container->Scroll.y -= (CtrlBottom - CntBottom);
                    } else if(Ctrl->Rect.y < (-Container->Scroll.y)) {
                        Container->Scroll.y = -Ctrl->Rect.y;
                    }
                }
            }
        }
    }
    
    Container->ControlStack.Count = 0;
    Ctx->ActiveContainer = 0;
}

void *
ui_get_noodle(ui_ctx_t *Ctx, ui_id_t Id) {
    ui_noodle_t *Noodle = 0;
    ASSERT(sizeof(ui_noodle_t *) == sizeof(u64)); // Table entry values are u64
    
    range_t Key;
    Key.Data = (u8*)&Id;
    Key.Size = sizeof(Id);
    
    if(!table_get(&Ctx->Pool.Table, Key, (u64 *)&Noodle)) {
        if(Ctx->Pool.Free) {
            Noodle = Ctx->Pool.Free;
            Ctx->Pool.Free = Noodle->Next;
            mem_zero_struct(Noodle);
            
            table_add(&Ctx->Pool.Table, Key, (u64)Noodle);
        } 
    }
    
    ASSERT(Noodle);
    return Noodle;
}

void
ui_free_noodle(ui_ctx_t *Ctx, ui_id_t Id) {
    ui_noodle_t *Noodle;
    
    range_t Key;
    Key.Data = (u8*)&Id;
    Key.Size = sizeof(Id);
    
    ASSERT(table_get(&Ctx->Pool.Table, Key, (u64 *)&Noodle));
    Noodle->Next = Ctx->Pool.Free;
    Ctx->Pool.Free = Noodle;
    table_remove(&Ctx->Pool.Table, Key);
}
