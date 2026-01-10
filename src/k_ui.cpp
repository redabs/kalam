
ui_cmd *
ui_push_draw_cmd(ui_ctx *Ctx, ui_cmd Cmd) {
    ASSERT((Ctx->CmdUsed + Cmd.Size) < UI_COMMAND_BUFFER_SIZE);

    ui_cmd *Result = (ui_cmd *)(Ctx->CmdBuf + Ctx->CmdUsed);
    copy(Result, &Cmd, Cmd.Size);

    Ctx->CmdUsed += Cmd.Size;
    return Result;
}

ui_cmd *
ui_add_draw_cmd(ui_ctx *Ctx, u64 Size) {
    ASSERT((Ctx->CmdUsed + Size) < UI_COMMAND_BUFFER_SIZE);

    ui_cmd *Result = (ui_cmd *)(Ctx->CmdBuf + Ctx->CmdUsed);
    zero(Result, Size);
    Result->Size = Size;

    Ctx->CmdUsed += Size;
    return Result;
}

void
ui_push_clip(ui_ctx *Ctx, irect Rect) {
    ui_cmd Cmd = {};
    Cmd.Type = UI_CMD_Clip;
    Cmd.Size = sizeof(ui_cmd);

    if(Ctx->LastClip) {
        Cmd.Clip.Rect = rect_overlap(Rect, Ctx->LastClip->Rect);
    } else {
        Cmd.Clip.Rect = Rect;
    }

    Cmd.Clip.Prev = Ctx->LastClip;
    Ctx->LastClip = &(ui_push_draw_cmd(Ctx, Cmd)->Clip);
}

void
ui_pop_clip(ui_ctx *Ctx) {
    ui_cmd Cmd = {0};
    Cmd.Type = UI_CMD_Clip;
    Cmd.Size = sizeof(ui_cmd);
    if(Ctx->LastClip->Prev) {
        Cmd.Clip.Rect = Ctx->LastClip->Prev->Rect;
    }
    Cmd.Clip.Prev = 0;

    Ctx->LastClip = Ctx->LastClip->Prev;
    ui_push_draw_cmd(Ctx, Cmd);
}

void
ui_draw_rect(ui_ctx *Ctx, irect Rect, color Color) {
    ui_cmd Cmd = {};
    Cmd.Size = sizeof(ui_cmd);
    Cmd.Type = UI_CMD_Rect;
    Cmd.Rect.Rect = Rect;
    Cmd.Rect.Color = Color;

    ui_push_draw_cmd(Ctx, Cmd);
}

// Internal text commands store a copy of the text that is to be draw in the command itself.
// External text commands (Internal == false) stores a copy of the Text.Data pointer. Meaning
// the pointer has to still point to valid memory when drawing!
void
ui_draw_text_ex(ui_ctx *Ctx, view<u8> Text, iv2 Baseline, color Color, b32 Internal) {
    u64 CmdSize = sizeof(ui_cmd) + (Internal ? Text.Count : 0);
    ui_cmd *Cmd = ui_add_draw_cmd(Ctx, CmdSize);
    Cmd->Type = UI_CMD_Text;
    Cmd->Text.Size = Text.Count;
    Cmd->Text.Baseline = Baseline;
    Cmd->Text.Color = Color;
    if(Internal) {
        // Internal text is stored right after the text pointer
        Cmd->Text.Text = (u8 *)&Cmd->Text.Text + sizeof(Cmd->Text.Text);
        copy(Cmd->Text.Text, Text.Ptr, Text.Count);
    } else {
        Cmd->Text.Text = Text.Ptr;
    }
}

void
ui_draw_text(ui_ctx *Ctx, view<u8> Text, iv2 Baseline, color Color) {
    ui_draw_text_ex(Ctx, Text, Baseline, Color, true);
}

void
ui_begin(ui_ctx *Ctx) {
    Ctx->CmdUsed = 0;
    Ctx->LastClip = 0;
    Ctx->Hot = 0;
}

u64
ui_hash_ex(u64 Id, view<u8> Data) {
    return fnv1a_32_ex((u32)Id, Data.Ptr, Data.Count);
}

u64
ui_hash(view<u8> Data) {
    u64 Id = fnv1a_32(Data.Ptr, Data.Count);
    return Id;
}

void
ui_end(ui_ctx *Ctx) {
    ASSERT(Ctx->ActiveContainer == 0);
    ASSERT(Ctx->LastClip == 0);
    Ctx->Frame += 1;

    Ctx->Input->MousePress = (mouse)0;
    Ctx->Input->Scroll = 0;
}

u64
ui_make_id(ui_ctx *Ctx, view<u8> Name) {
    u64 Id;
    if(Ctx->ActiveContainer) {
        Id = ui_hash_ex(Ctx->ActiveContainer->Id, Name);
    } else {
        Id = ui_hash(Name);
    }

    return Id;
}

ui_container *
ui_find_container(ui_ctx *Ctx, view<u8> Name) {
    u64 Id = ui_hash(Name);
    for(u64 i = 0; i < UI_CONTAINER_MAX; ++i) {
        ui_container *c = &Ctx->Containers[i];
        if(c->Id == Id && (c->Flags & UI_CNT_Open)) {
            return c;
        }
    }
    return 0;
}

void
ui_put_container_on_top(ui_ctx *Ctx, ui_container *Container) {
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

ui_container *
ui_container_under_point(ui_ctx *Ctx, iv2 Point) {
    for(u32 i = 0; i < Ctx->OpenContainerCount; ++i) {
        ui_container *c = Ctx->ContainerStack[i];
        ASSERT(c->Flags & UI_CNT_Open);
        if(is_point_inside_rect(c->Rect, Point)) {
            return c;
        }
    }

    return 0;
}

ui_container *
ui_allocate_container(ui_ctx *Ctx) {
    if(Ctx->OpenContainerCount >= UI_CONTAINER_MAX) {
        return 0;
    }

    ui_container *Container = 0;
    for(u32 i = 0; i < UI_CONTAINER_MAX; ++i) {
        ui_container *c = &Ctx->Containers[i];
        if(!(c->Flags & UI_CNT_Open)) {
            Container = c;
            break;
        }
    }

    ASSERT(Container);
    zero_struct(Container);
    return Container;
}

ui_container *
ui_open_container(ui_ctx *Ctx, view<u8> Name, b8 IsPopUp) {
    ui_container *Container = ui_find_container(Ctx, Name);
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
    Container->Flags |= (IsPopUp ? UI_CNT_PopUp : 0);
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
ui_close_container(ui_ctx *Ctx, ui_container *Container) {
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

ui_interaction_type
ui_update_input_state(ui_ctx *Ctx, irect Rect, u64 Id) {
    ui_interaction_type Result = UI_INTERACTION_None;
    ui_container *Container = Ctx->ActiveContainer;
    if(!Container) {
        return Result;
    }

    irect RelativeToContainer = {};
    RelativeToContainer.x = Rect.x - Container->Rect.x - Container->Scroll.x;
    RelativeToContainer.y = Rect.y - Container->Rect.y - Container->Scroll.y;
    RelativeToContainer.w = Rect.w;
    RelativeToContainer.h = Rect.h;
    ui_control Ctrl = {};
    Ctrl.Rect = RelativeToContainer;
    Ctrl.Id = Id;

    ASSERT(Container->ControlCount + 1 < UI_CONTROL_MAX);
    if(Container->ControlCount + 1 < UI_CONTROL_MAX) {
        Container->Controls[Container->ControlCount++] = Ctrl;
    }

    if(!Ctx->Active) {
        ui_container *CntUnderMouse = ui_container_under_point(Ctx, Ctx->Input->MousePos);
        if(CntUnderMouse == Container && is_point_inside_rect(Rect, Ctx->Input->MousePos)) {
            Ctx->Hot = Id;
            Result = UI_INTERACTION_Hover;
        }
    }

    if(Ctx->Selected == Id) {
        Result = UI_INTERACTION_Hover;
        for(u32 i = 0; i < Ctx->Input->EventCount; ++i) {
            key_event *Ki = Ctx->Input->Events + i;
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

ui_layout *
ui_get_layout(ui_ctx *Ctx) {
    ASSERT(Ctx->LayoutCount > 0);
    return &Ctx->Layouts[Ctx->LayoutCount - 1];
}

void
ui_next_row(ui_layout *Layout) {
    Layout->Cursor.x = Layout->Rect.x;
    Layout->Rect.h += Layout->RowHeight;
    Layout->Cursor.y += Layout->RowHeight;
    Layout->RowHeight = 0;
}

// Returns the cursor position for Ctx->ActiveContainer.
iv2
ui_peek_cursor_position(ui_ctx *Ctx) {
    ui_layout *Layout = ui_get_layout(Ctx);
    ui_container *Container = Ctx->ActiveContainer;
    ASSERT(Layout && Container);

    iv2 Result = {};
    Result.x = Layout->Cursor.x + Container->Rect.x + Container->Scroll.x + Layout->Indent.x;
    Result.y = Layout->Cursor.y + Container->Rect.y + Container->Scroll.y + Layout->Indent.y;

    return Result;
}


s32
ui_calculate_real_control_rect_width(ui_layout *Layout, s32 Width) {
    if(Width <= 0) {
        s32 RemainingWidth = Layout->Rect.w - (Layout->Cursor.x - Layout->Rect.x + Layout->Indent.x);
        s32 RemainingSlots = Layout->ItemsPerRow - Layout->ItemCount;
        s32 WidthPerItem = RemainingWidth / MAX(RemainingSlots, 1);
        Width = MAX(WidthPerItem + Width, 0);
    }

    return Width;
}

// If Width or Height is 0 then the rect fills the parent, if it's less than 0 then
// it fills the parent and leaves the absolute value of that much space.
irect
ui_push_rect(ui_ctx *Ctx, s32 Width, s32 Height) {
    ui_container *Container = Ctx->ActiveContainer;
    ui_layout *Layout = ui_get_layout(Ctx);
    Width = ui_calculate_real_control_rect_width(Layout, Width);

    if(Height <= 0) {
        // Layouts are expanded vertically to fit their content. 
        s32 H;
        // Use parent's height
        if(Ctx->LayoutCount > 1) {
            H = Ctx->Layouts[Ctx->LayoutCount - 2].Rect.h;
        } else {
            H = Container->Rect.h;
        }
        Height = H + Height;
    }
    // The cursor does not move in Y until we move to the next row.
    // Therefore we need to keep track of the tallest control rect 
    // on the current row such that we can advance in Y appropriately.
    Layout->RowHeight = MAX(Layout->RowHeight, Height);

    irect Result = {};
    Result.p = ui_peek_cursor_position(Ctx);
    Result.w = Width; 
    Result.h = Height;

    Layout->Cursor.x += Width;

    Layout->ItemCount += 1;
    if(Layout->ItemCount == Layout->ItemsPerRow) {
        ui_next_row(Layout);
        Layout->ItemCount = 0;
    }

    return Result;
}

// TODO: Take a Height parameter and add scrolling inside a layout, we need this for
// scrolling through files with the entire container scrolling along.
void
ui_push_layout(ui_ctx *Ctx, s32 Width) {
    ui_layout *Layout = ui_get_layout(Ctx);
    irect Rect = {};
    Rect.p = ui_peek_cursor_position(Ctx);
    Rect.w = ui_calculate_real_control_rect_width(Layout, Width);
    Rect.h = 0;

    ui_layout NewLayout = {};
    NewLayout.Rect = Rect;
    NewLayout.Cursor = Rect.p;
    NewLayout.ItemsPerRow = 1;

    ASSERT(Ctx->LayoutCount + 1 < UI_LAYOUT_MAX);
    if(Ctx->LayoutCount + 1 < UI_LAYOUT_MAX) {
        Ctx->Layouts[Ctx->LayoutCount++] = NewLayout;
    }
}

ui_layout
ui_pop_layout(ui_ctx *Ctx) {
    ASSERT(Ctx->LayoutCount > 0);
    if(Ctx->LayoutCount > 0) {
        ui_layout Layout = Ctx->Layouts[--Ctx->LayoutCount];
        ui_push_rect(Ctx, Layout.Rect.w, Layout.Rect.h);
        return Layout;
    }
    return {};
}

ui_container *
ui_begin_container(ui_ctx *Ctx, irect Rect, view<u8> Name) {
    if(Ctx->ActiveContainer) {
        // No container hierarchy
        return 0;
    }

    ui_container *Container = ui_find_container(Ctx, Name);
    if(!Container || !(Container->Flags & UI_CNT_Open)) {
        return 0;
    }

    if(Container->Flags & UI_CNT_PopUp) {
        if(Ctx->Input->MousePress & MOUSE_Left) {
            ui_container *PressedContainer = ui_container_under_point(Ctx, Ctx->Input->MousePos);
            if(PressedContainer != Container) {
                ui_close_container(Ctx, Container);
                return 0;
            }
        }
    }

    Container->Rect = Rect;
    ui_layout Layout = {};
    Layout.Rect.w = Rect.w;
    Layout.Rect.h = Rect.h;
    Layout.ItemsPerRow = 1;

    ASSERT(Ctx->LayoutCount + 1 < UI_LAYOUT_MAX);
    if(Ctx->LayoutCount + 1 < UI_LAYOUT_MAX) {
        Ctx->Layouts[Ctx->LayoutCount++] = Layout;
    }

    Container->Prev = Ctx->ActiveContainer;
    Ctx->ActiveContainer = Container;

    return Container;
}

void
ui_row(ui_ctx *Ctx, u32 Count) {
    ui_layout *Layout = ui_get_layout(Ctx);
    if(Layout->ItemCount > 0) {
        ui_next_row(Layout);
    }
    Layout->ItemCount = 0;
    Layout->ItemsPerRow = Count;
}

void
ui_begin_column(ui_ctx *Ctx, s32 Width) {
    ui_push_layout(Ctx, Width);
    ui_layout *Layout = ui_get_layout(Ctx);
    ui_container *Cnt = Ctx->ActiveContainer;
    irect ClipRect = {};
    ClipRect.x = Cnt->Rect.x + Layout->Rect.x;
    ClipRect.y = Cnt->Rect.y + Layout->Rect.y;
    ClipRect.w = Layout->Rect.w;
    ClipRect.h = Cnt->Rect.h;
    ui_push_clip(Ctx, ClipRect);
}

void
ui_end_column(ui_ctx *Ctx) {
    ui_pop_clip(Ctx);
    ui_pop_layout(Ctx);
}

void
ui_end_container(ui_ctx *Ctx) {
    ASSERT(Ctx->LayoutCount > 0);
    if(Ctx->LayoutCount > 0) {
        Ctx->LayoutCount -= 1;
    }
    ui_container *Container = Ctx->ActiveContainer;
    ASSERT(Container);

    if(Ctx->Focus == Container->Id && Container->ControlCount > 0) {
#if 0        
        for(u32 i = 0; i < Ctx->Input->KeyCount; ++i) {
            key_input *Ki = &Ctx->Input->Keys[i];
            b8 Forward = (Ki->Key == KEY_Tab || Ki->Key == KEY_Down);
            b8 Backward = (Ki->Key == KEY_Tab && Ki->Modifiers == MOD_Shift) || (Ki->Key == KEY_Up);
            if(!Ki->Handled && !Ki->IsText && (Backward || Forward)) {
                if(!Ctx->Selected) {
                    Ctx->Selected = Container->Controls[0].Id;
                    Container->ControlSelected = 0;
                } else {
                    if(Backward) {
                        Container->ControlSelected += (Container->ControlSelected == 0) ? (u32)Container->ControlCount - 1 : -1;
                    } else {
                        Container->ControlSelected = (Container->ControlSelected + 1) % Container->ControlCount;
                    }
                    
                    Ctx->TextEditFocus = 0;
                    Ctx->Selected = Container->Controls[Container->ControlSelected].Id;
                    
                    // Scroll the control into view
                    ui_control *Ctrl = Container->Controls + Container->ControlSelected;
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
#endif
    }

    Container->ControlCount = 0;
    Ctx->ActiveContainer = 0;
}

s64
ui_get_tree_node(ui_ctx *Ctx, u64 Id) {
    for(u64 i = 0; i < UI_NODE_MAX; ++i) {
        if(Ctx->TreeNodes[i].Id == Id) {
            return i;
        }
    }
    return -1;
}

void
ui_update_tree_node(ui_ctx *Ctx, u64 Id, u64 Idx) {
    Ctx->TreeNodes[Idx].LastUpdated = Ctx->Frame;
}

s64
ui_set_tree_node(ui_ctx *Ctx, u64 Id, b8 IsOpen) {
    for(s64 i = 0; i < UI_NODE_MAX; ++i) {
        s64 dFrame = Ctx->Frame - Ctx->TreeNodes[i].LastUpdated;
        // Check for unused slots or slots older than 1 frame.
        if(!Ctx->TreeNodes[i].Id || dFrame > 1) {
            Ctx->TreeNodes[i].Open = IsOpen;
            Ctx->TreeNodes[i].Id = Id;
            ui_update_tree_node(Ctx, Id, i);
            return i;
        }
    }

    return -1;
}
