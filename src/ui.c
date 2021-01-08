
void
ui_begin(ui_ctx_t *Ctx) {
    Ctx->DrawCommandStack.Top = (ui_draw_cmd_t *)Ctx->DrawCommandStack.Data;
}

void
ui_end(ui_ctx_t *Ctx) {
    // If there is an active Id but the last mouse event was left mouse button release then
    // we should reset the active Id. This happens if you press the application window and 
    // drag the mouse cursor outside the window and release.
    for(s32 i = Ctx->EventBuffer->Count - 1; i >= 0; --i) {
        input_event_t *e = &Ctx->EventBuffer->Events[i];
        if(e->Device == INPUT_DEVICE_Mouse && e->Mouse.Button == INPUT_MOUSE_Left && e->Type == INPUT_EVENT_Release) {
            Ctx->Active = 0;
            break;
        }
    }
    
    Ctx->Hot = 0;
    
    for(s64 i = 0, j = 0; i < UI_CONTAINER_MAX && j < Ctx->OpenContainerCount; ++i) {
        ui_container_t *c = &Ctx->Containers[i];
        if(c->Open) {
            c->ScrollX = 0;
            c->ScrollY = 0;
            c->ContentWidth = 0;
            c->ContentHeight = 0;
            j += 1;
        }
    }
}