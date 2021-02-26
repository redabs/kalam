#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "platform.h"
#include "util.h"
#include "ui.h"
#include "kalam.h"
#include "custom.h"

b8 handle_panel_input(input_event_t Event);

#include "parse.c"
#include "selection.c"
#include "panel.c"
#include "layout.c"
#include "render.c"
#include "draw.c"
#include "ui.c"
#include "widgets.c"

ctx_t gCtx = {0};
ui_ctx_t *gUiCtx = &gCtx.UiCtx;

font_t
load_ttf(range_t Path, f32 Size) {
    font_t Font = {0};
    Font.Size = Size;
    
    ASSERT(platform_read_file(Path, &Font.File));
    
    s32 Status = stbtt_InitFont(&Font.StbInfo, Font.File.Data, 0);
    
    stbtt_GetFontVMetrics(&Font.StbInfo, &Font.Ascent, &Font.Descent, &Font.LineGap);
    f32 Scale = stbtt_ScaleForMappingEmToPixels(&Font.StbInfo, Size);
    Font.Ascent = (s32)(Font.Ascent * Scale + 0.5);
    Font.Descent = (s32)(Font.Descent * Scale + 0.5);
    Font.LineGap = (s32)(Font.LineGap * Scale + 0.5);
    Font.LineHeight = Font.Ascent - Font.Descent + Font.LineGap; 
    
    stbtt_bakedchar *g = get_stbtt_bakedchar(&Font, 'M');
    if(g) {
        Font.MHeight = (s32)(g->y1 - g->y0 + 0.5);
        Font.MWidth = (s32)(g->x1 - g->x0 + 0.5);
    }
    
    Font.SpaceWidth = get_x_advance(&Font, ' ');
    
    return Font;
}

buffer_t *
add_buffer() {
    buffer_t *Buf = sb_add(gCtx.Buffers, 1);
    mem_zero_struct(Buf);
    make_lines(Buf);
    
    return Buf;
}

b32
load_file(range_t Path) {
    platform_file_data_t File;
    b32 FileReadSuccess = platform_read_file(Path, &File);
    if(FileReadSuccess) {
        // TODO: Detect file encoding, we assume utf-8 for now
        buffer_t *OldBase = gCtx.Buffers;
        buffer_t *Buf = add_buffer();
        if(gCtx.Buffers != OldBase) {
            for(u32 i = 0; i < PANEL_MAX; ++i) {
                panel_t *p = &gCtx.PanelCtx.Panels[i];
                if(p->IsLeaf) {
                    p->Buffer = &gCtx.Buffers[p->Buffer - OldBase];
                }
            }
        }
        
        if(File.Size > 0) {
            mem_buf_add(&Buf->Text, File.Size);
            mem_copy(Buf->Text.Data, File.Data, File.Size);
        } else {
            mem_buf_grow(&Buf->Text, 128);
        }
        
        platform_free_file(&File);
        
        make_lines(Buf);
    }
    
    return FileReadSuccess;
}

void
k_init(platform_shared_t *Shared, range_t CurrentDirectory) {
    gCtx.Font = load_ttf(C_STR_AS_RANGE("fonts/consola.ttf"), 14);
    if(!load_file(C_STR_AS_RANGE("../kalam.c"))) {
        add_buffer();
    }
    
    platform_get_files_in_directory(CurrentDirectory, &gCtx.CurrentDirectory);
    
    u32 n = ARRAY_COUNT(gCtx.PanelCtx.Panels);
    for(u32 i = 0; i < n - 1; ++i) {
        gCtx.PanelCtx.Panels[i].Next = &gCtx.PanelCtx.Panels[i + 1];
    }
    gCtx.PanelCtx.Panels[n - 1].Next = 0;
    gCtx.PanelCtx.FreeList = &gCtx.PanelCtx.Panels[0];
    
    panel_create(&gCtx);
    ui_init(gUiCtx, &gCtx.Font);
}

void
set_mode(panel_t *Panel, mode_t Mode) {
    switch(Mode) {
        case MODE_Select: {
            mode_select_ctx_t *SelectCtx = &Panel->ModeCtx.Select;
            mem_buf_clear(&SelectCtx->SearchTerm);
            SelectCtx->SelectionGroup.Owner = Panel;
            SelectCtx->SelectionGroup.SelectionIdxTop = 0;
            if(SelectCtx->SelectionGroup.Selections) {
                sb_set_count(SelectCtx->SelectionGroup.Selections, 0);
            }
        } break;
    }
    Panel->Mode = Mode;
}

b8
do_operation(operation_t Op) {
    b8 WasHandled = true;
    
    switch(Op.Type) {
        case OP_SetMode: {
            set_mode(gCtx.PanelCtx.Selected, Op.SetMode.Mode);
        } break;
        
        // Normal
        case OP_Normal_Home: {
            panel_t *Panel = gCtx.PanelCtx.Selected;
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *s = &SelGrp->Selections[i];
                line_t *Line = &Panel->Buffer->Lines[offset_to_line_index(Panel->Buffer, s->Cursor)];
                s->Anchor = s->Cursor;
                s->Cursor = Line->Offset;
                s->ColumnIs = s->ColumnWas = 0;
            }
            merge_overlapping_selections(Panel);
        } break;
        
        case OP_Normal_End: {
            panel_t *Panel = gCtx.PanelCtx.Selected;
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *s = &SelGrp->Selections[i];
                line_t *Line = &Panel->Buffer->Lines[offset_to_line_index(Panel->Buffer, s->Cursor)];
                s->Anchor = s->Cursor;
                s->Cursor = Line->Offset + Line->Size - Line->NewlineSize;
                s->ColumnWas = s->ColumnIs = Line->Length;
            }
            merge_overlapping_selections(Panel);
        } break;
        
        case OP_DeleteSelection: {
            delete_selection(gCtx.PanelCtx.Selected);
        } break;
        
        case OP_ExtendSelection: {
            extend_selection(gCtx.PanelCtx.Selected, Op.ExtendSelection.Dir);
        } break;
        
        case OP_DropSelectionAndMove: {
            panel_t *Panel = gCtx.PanelCtx.Selected;
            
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            selection_t Sel = get_selection_max_idx(SelGrp);
            Sel.Idx = SelGrp->SelectionIdxTop++;
            move_selection(Panel->Buffer, &Sel, Op.DropSelectionAndMove.Dir, true);
            sb_push(SelGrp->Selections, Sel);
            
            merge_overlapping_selections(Panel);
        } break;
        
        case OP_ClearSelections: {
            clear_selections(gCtx.PanelCtx.Selected);
        } break;
        
        case OP_OpenFileSelection: {
        } break;
        
        case OP_SelectEntireBuffer: {
            panel_t *Panel = gCtx.PanelCtx.Selected;
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            SelGrp->Selections[0].Anchor = 0;
            SelGrp->Selections[0].Cursor = Panel->Buffer->Text.Used;
            SelGrp->Selections[0].Idx = SelGrp->SelectionIdxTop++;
            sb_set_count(SelGrp->Selections, 1);
        } break;
        
        // Insert
        case OP_Insert_Home: {
            panel_t *Panel = gCtx.PanelCtx.Selected;
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *s = &SelGrp->Selections[i];
                line_t *Line = &Panel->Buffer->Lines[offset_to_line_index(Panel->Buffer, s->Cursor)];
                s->Cursor = s->Anchor = Line->Offset;
                s->ColumnWas = s->ColumnIs = 0;
            }
            merge_overlapping_selections(Panel);
        } break;
        
        case OP_Insert_End: {
            panel_t *Panel = gCtx.PanelCtx.Selected;
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *s = &SelGrp->Selections[i];
                line_t *Line = &Panel->Buffer->Lines[offset_to_line_index(Panel->Buffer, s->Cursor)];
                s->Cursor = s->Anchor = Line->Offset + Line->Size - Line->NewlineSize;
                s->ColumnWas = s->ColumnIs = Line->Length;
            }
            merge_overlapping_selections(Panel);
        } break;
        
        case OP_EscapeToNormal: {
            gCtx.PanelCtx.Selected->Mode = MODE_Normal;
        } break;
        
        case OP_Delete: {
            delete_selection(gCtx.PanelCtx.Selected);
        } break;
        
        case OP_DoChar: {
            do_char(gCtx.PanelCtx.Selected, Op.DoChar.Character);
        } break;
        
        // Global
        case OP_ToggleSplitMode: {
            gCtx.PanelCtx.Selected->Split ^= 1;
        } break;
        
        case OP_NewPanel: {
            panel_create(&gCtx);
        } break;
        
        case OP_KillPanel: {
            panel_kill(&gCtx.PanelCtx, gCtx.PanelCtx.Selected);
        } break;
        
        case OP_MovePanelSelection: {
            panel_move_selection(&gCtx.PanelCtx, Op.MovePanelSelection.Dir); 
        } break;
        
        case OP_MoveSelection: {
            move_all_selections_in_panel(gCtx.PanelCtx.Selected, Op.MoveSelection.Dir);
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
find_mapping_match(key_mapping_t *Mappings, u64 MappingCount, input_event_t *e) {
    for(u64 i = 0; i < MappingCount; ++i) {
        key_mapping_t *m = &Mappings[i];
        if(event_mapping_match(m, e)) {
            return m;
        }
    }
    
    return 0;
}

b32 
find_next_search_term_in_range(buffer_t *Buffer, mem_buffer_t *SearchTerm, u64 Start, u64 End, u64 *Offset) {
    if(SearchTerm->Used == 0) { 
        return false;
    }
    
    u64 TermIdx = 0;
    for(u64 i = Start; i < End; ++i) {
        if(Buffer->Text.Data[i] == SearchTerm->Data[TermIdx]) {
            ++TermIdx;
            if(TermIdx == SearchTerm->Used) {
                *Offset = i - TermIdx + 1;
                return true;
            }
        } else {
            TermIdx = 0;
        }
    }
    return false;
}

void
update_select_state(panel_t *Panel) {
    // Given the current selections in the buffer owned by the selected panel, find all occurences of 
    // the entered search term. 
    // The original set of selections need to be preserved if select mode is exited without submitting
    // (hitting return). I think we need a separate working set of selections that - on submission - replaces
    // the set in the selection group. As a matter of interactivity, the working is the one that should be
    // rendered. I suppose we can in draw_panel check if the panel is in MODE_Select and then draw the 
    // working set instead of the in the selection group for the buffer. As such the working set should be in the mode context in the panel struct.
    mem_buffer_t *SearchTerm = &Panel->ModeCtx.Select.SearchTerm;
    mode_select_ctx_t *SelectCtx = &Panel->ModeCtx.Select;
    for(s64 GrpIdx = 0; GrpIdx < sb_count(Panel->Buffer->SelectionGroups); ++GrpIdx) {
        selection_group_t *SelGroup = &Panel->Buffer->SelectionGroups[GrpIdx];
        if(SelGroup->Owner == Panel) {
            for(s64 SelIdx = 0; SelIdx < sb_count(SelGroup->Selections); ++SelIdx) {
                selection_t *Sel = &SelGroup->Selections[SelIdx];
                u64 Start = selection_start(Sel); 
                u64 End = selection_end(Sel); 
                u64 ResultOffset;
                while(find_next_search_term_in_range(Panel->Buffer, &SelectCtx->SearchTerm, Start, End, &ResultOffset)) {
                    Start = ResultOffset + SelectCtx->SearchTerm.Used;
                    selection_t s = make_selection(Panel->Buffer, ResultOffset, ResultOffset + SelectCtx->SearchTerm.Used - 1, SelectCtx->SelectionGroup.SelectionIdxTop++);
                    sb_push(SelectCtx->SelectionGroup.Selections, s);
                }
            }
            return;
        }
    }
}

b8
handle_panel_input(input_event_t Event) {
    b8 EventHandled = false;
    if(Event.Device == INPUT_DEVICE_Keyboard) {
        if(Event.Type == INPUT_EVENT_Press || Event.Type == INPUT_EVENT_Text) {
            panel_t *Panel = gCtx.PanelCtx.Selected;
            if(Panel) {
                switch(Panel->Mode) {
                    case MODE_Normal: {
                        key_mapping_t *m = find_mapping_match(NormalMappings, ARRAY_COUNT(NormalMappings), &Event);
                        if(m) {
                            EventHandled = do_operation(m->Operation);
                        }
                    } break;
                    
                    case MODE_Insert: {
                        key_mapping_t *m = find_mapping_match(InsertMappings, ARRAY_COUNT(InsertMappings), &Event);
                        if(m) {
                            EventHandled = do_operation(m->Operation);
                        } else if(Event.Type == INPUT_EVENT_Text) {
                            // Ignore characters that were sent with the only modifier down being Ctrl (except for Caps Lock and Num Lock). I.Event. ignore control sequences.
                            if(!event_is_control_sequence(&Event)) {
                                operation_t o = {
                                    .Type = OP_DoChar, 
                                    .DoChar.Character[0] = Event.Text.Character[0],
                                    .DoChar.Character[1] = Event.Text.Character[1],
                                    .DoChar.Character[2] = Event.Text.Character[2],
                                    .DoChar.Character[3] = Event.Text.Character[3],
                                };
                                do_operation(o);
                            }
                            EventHandled = true;
                        }
                    } break;
                    
                    case MODE_Select: {
                        key_mapping_t *m = find_mapping_match(SelectMappings, ARRAY_COUNT(SelectMappings), &Event);
                        if(m) {
                            EventHandled = do_operation(m->Operation);
                        } else if(Event.Type == INPUT_EVENT_Text && !event_is_control_sequence(&Event)) {
                            // Append text to search term 
                            u8 *Char = Event.Text.Character;
                            mode_select_ctx_t *SelectCtx = &Panel->ModeCtx.Select;
                            mem_buffer_t *SearchTerm = &Panel->ModeCtx.Select.SearchTerm;
                            if(*Char < 0x20) {
                                switch(*Char) {
                                    case '\n':
                                    case '\r': {
                                        // Commit selections
                                        if(SearchTerm->Used > 0) {
                                            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
                                            free_selections_and_reset_group(SelGrp);
                                            *SelGrp = SelectCtx->SelectionGroup;
                                            mem_zero_struct(&SelectCtx->SelectionGroup);
                                        }
                                        Panel->Mode = MODE_Normal;
                                    } break;
                                    
                                    case '\t': {
                                        
                                        //mem_buf_append(SearchTerm, "\t", 1);
                                    } break;
                                    
                                    case '\b': {
                                        if(SearchTerm->Used > 0) {
                                            u8 *End = SearchTerm->Data + SearchTerm->Used;
                                            u8 *Begin = utf8_move_back_one(End);
                                            mem_buf_pop_size(SearchTerm, (u64)(End - Begin));
                                            sb_set_count(Panel->ModeCtx.Select.SelectionGroup.Selections, 0);
                                            update_select_state(Panel);
                                        }
                                    } break;
                                }
                            } else {
                                sb_set_count(Panel->ModeCtx.Select.SelectionGroup.Selections, 0);
                                mem_buf_append(SearchTerm, Char, utf8_char_width(Char));
                                update_select_state(Panel);
                            }
                        }
                    } break;
                }
            }
        }
    }
    
    return EventHandled;
}

#define IS_DIRECTORY_DELMITER(Character) (Character == '\\' || Character == '/')

// "/path/too/foo" -> "/path/too"
// "/path/too/foo/" -> "/path/too/foo/"
// "/path" -> "/"
// "C:/" -> "C:/" same with backslash \
// "C:" -> ""
void
truncate_path_to_nearest_directory(mem_buffer_t *Path) {
    if(Path->Used > 0) {
        // Return if the last character is a directory delimiter
        if(!IS_DIRECTORY_DELMITER(Path->Data[Path->Used - 1])) {
            
            // Chop off characters until we reach either the base of the path, or a directory delimiter
            while(Path->Used > 0) {
                u8 *c = utf8_move_back_one(Path->Data + Path->Used);
                if(IS_DIRECTORY_DELMITER(*c)) {
                    break;
                }
                --Path->Used;
            }
        }
    }
}

void
handle_file_select_input(input_event_t Event) {
#if 0
    if(Event.Type == INPUT_EVENT_Text && !event_is_control_sequence(&Event)) {
        u8 *Char = Event.Text.Character;
        if(*Char < 0x20) {
            switch(*Char) {
                case '\r':
                case '\n': {
                    if(sb_count(gCtx.FileNameInfo) > 0) {
                        file_name_info_t *Fi = &gCtx.FileNameInfo[gCtx.SelectedFileIndex];
                        if(Fi->Flags & FILE_FLAGS_Directory) {
                            truncate_path_to_nearest_directory(&gCtx.SearchDirectory);
                            mem_buf_append(&gCtx.SearchDirectory, &Ctx.FileNames.Data[Fi->Offset], Fi->Size);
                            mem_buf_append_range(&Ctx.SearchDirectory, C_STR_AS_RANGE("/"));
                            
                            mem_buf_replace(&Ctx.WorkingDirectory, &Ctx.SearchDirectory);
                            update_current_directory_files();
                        } else {
                            
                            truncate_path_to_nearest_directory(&Ctx.SearchDirectory);
                            mem_buf_append_range(&Ctx.SearchDirectory, (range_t){.Data = Ctx.FileNames.Data + Fi->Offset, .Size = Fi->Size});
                            
                            load_file(mem_buf_as_range(Ctx.SearchDirectory));
                            panel_show_buffer(Ctx.PanelCtx.Selected, &Ctx.Buffers[sb_count(Ctx.Buffers) - 1]);
                            
                            Ctx.WidgetFocused = WIDGET_Panels;
                        }
                    }
                } break;
                
                case '\b': {
                    if(Ctx.SearchDirectory.Used > 0) {
                        u8 *c = utf8_move_back_one(Ctx.SearchDirectory.Data + Ctx.SearchDirectory.Used);
                        Ctx.SearchDirectory.Used -= utf8_char_width(c);
                        // If a directory delimiter is deleted then truncate the search path down to the nearest folder.
                        // This allows popping the last directory off by hitting backspace once. E.g. hitting backspace on
                        // the directory "/path/to/foo/" jumps to "/path/to/".
                        if(IS_DIRECTORY_DELMITER(*c)) {
                            truncate_path_to_nearest_directory(&Ctx.SearchDirectory);
                            mem_buf_replace(&Ctx.WorkingDirectory, &Ctx.SearchDirectory);
                        }
                        
                        if(Ctx.SearchDirectory.Used > 0) {
                            update_current_directory_files();
                        }
                    }
                } break;
            }
        } else {
            // TODO: Right now we're filtering using wildcards on the platform side. We should get all the files in the current directory 
            // and do the filtering in the editor instead, so it's consistent across platforms. Additionally, we would only need to call out 
            // to the platform when the search directory changes. 
            mem_buf_append(&Ctx.SearchDirectory, Char, utf8_char_width(Char));
            update_current_directory_files();
        }
    } else if(Event.Type == INPUT_EVENT_Press) {
        switch(Event.Key.KeyCode) {
            case KEY_Up: {
                Ctx.SelectedFileIndex = MAX(Ctx.SelectedFileIndex - 1, 0);
            } break;
            
            case KEY_Down: {
                Ctx.SelectedFileIndex = MIN(Ctx.SelectedFileIndex + 1, sb_count(Ctx.FileNameInfo) - 1);
            } break;
            
            case KEY_Escape: {
                Ctx.WidgetFocused = WIDGET_Panels;
            } break;
        }
    }
#endif
}

b8
is_event_key_press(input_event_t *Event, input_key_t Key) {
    if(Event->Type == INPUT_EVENT_Press && Event->Key.KeyCode == Key) {
        return true;
    }
    
    return false;
}

void
push_directory_or_open_file(file_info_t *FileInfo) {
    directory_t *Dir = &gCtx.CurrentDirectory;
    // Make full path of the file or directory to open
    mem_buffer_t Path = {0};
    mem_buf_replace(&Path, Dir->Path);
    mem_buf_append_range(&Path, (range_t){.Data = Dir->FileNameBuffer.Data + FileInfo->Name.Offset, .Size = FileInfo->Name.Size});
    
    if(FileInfo->Flags & FILE_FLAGS_Directory) {
        // Push directory
        mem_buf_append_range(&Path, C_STR_AS_RANGE("/"));
        platform_get_files_in_directory(mem_buf_as_range(Path), Dir);
    } else {
        // Open file
        load_file(mem_buf_as_range(Path));
        panel_show_buffer(gCtx.PanelCtx.Selected, &gCtx.Buffers[sb_count(gCtx.Buffers) - 1]);
    }
    
    mem_buf_free(&Path);
}

void
draw_ui(framebuffer_t *Fb) {
    ui_cmd_t *Cmd = 0;
    for(u64 Offset = 0; Offset < gUiCtx->CmdBuf.Used; Offset += Cmd->Size) {
        Cmd = (ui_cmd_t *)(gUiCtx->CmdBuf.Data + Offset);
        switch(Cmd->Type) {
            case UI_CMD_Rect: {
                draw_rect(Fb, Cmd->Rect.Rect, Cmd->Rect.Color);
            } break;
            
            case UI_CMD_Text: {
                draw_text_line(Fb, &gCtx.Font, Cmd->Text.Baseline.x, Cmd->Text.Baseline.y, Cmd->Text.Color, (range_t){.Data = Cmd->Text.Text, .Size = Cmd->Text.Size});
            } break;
            
            case UI_CMD_Clip: {
                Fb->Clip = Cmd->Clip.Rect;
            } break;
        }
    }
}

b8
filter_pass(range_t Filter, range_t Input) {
    if(Input.Size == 0) { 
        return true;
    }
    u64 Sf = Filter.Size;
    u64 Si = Input.Size;
    for(u64 i = 0; i < Sf; ++i) {
        if(Filter.Data[i] == Input.Data[0]) {
            u64 j = i + 1, k = 1;
            for(; (j < Sf && k < Si); ++j, ++k) {
                if(Filter.Data[j] != Input.Data[k]) {
                    break;
                }
            }
            if(k == Si) {
                return true;
            }
            
            i += k;
        }
    }
    return false;
}

void
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    for(u64 i = 0; i < Events->Count; ++i) {
        input_event_t *e = &Events->Events[i];
        if(e->Device == INPUT_DEVICE_Mouse) {
            ui_mouse_t Btn;
            switch(e->Mouse.Button) {
                case INPUT_MOUSE_Left:     { Btn = UI_MOUSE_Left; } break;
                case INPUT_MOUSE_Right:    { Btn = UI_MOUSE_Right; } break;
                case INPUT_MOUSE_Middle:   { Btn = UI_MOUSE_Middle; } break;
                case INPUT_MOUSE_Forward:  { Btn = UI_MOUSE_Forward; } break;
                case INPUT_MOUSE_Backward: { Btn = UI_MOUSE_Backward; } break;
                default: { continue; }
            }
            if(e->Type == INPUT_EVENT_Press) {
                ui_mouse_down(gUiCtx, Btn);
            } else if(e->Type == INPUT_EVENT_Release) {
                ui_mouse_up(gUiCtx, Btn);
            }
        } else if(e->Device == INPUT_DEVICE_Keyboard) {
            if(e->Type == INPUT_EVENT_Text) {
                ui_text_input(gUiCtx, e->Text.Character);
            } else if(e->Type == INPUT_EVENT_Press || e->Type == INPUT_EVENT_Release) {
                ui_key_t Key;
                switch(e->Key.KeyCode) {
                    case KEY_Tab:      { Key = UI_KEY_Tab; } break;
                    case KEY_Shift:    { Key = UI_KEY_Shift; } break;
                    case KEY_Ctrl:     { Key = UI_KEY_Ctrl; } break;
                    case KEY_Alt:      { Key = UI_KEY_Alt; } break;
                    case KEY_Escape:   { Key = UI_KEY_Escape; } break;
                    case KEY_PageUp:   { Key = UI_KEY_PageUp; } break;
                    case KEY_PageDown: { Key = UI_KEY_PageDown; } break;
                    case KEY_End:      { Key = UI_KEY_End; } break;
                    case KEY_Home:     { Key = UI_KEY_Home; } break;
                    case KEY_Left:     { Key = UI_KEY_Left; } break;
                    case KEY_Up:       { Key = UI_KEY_Up; } break;
                    case KEY_Right:    { Key = UI_KEY_Right; } break;
                    case KEY_Down:     { Key = UI_KEY_Down; } break;
                    case KEY_Delete:   { Key = UI_KEY_Delete; } break;
                    default: { continue; } break;
                }
                if(e->Type == INPUT_EVENT_Press) {
                    ui_key_down(gUiCtx, Key);
                } else {
                    ui_key_up(gUiCtx, Key);
                }
            }
        }
    }
    ui_mouse_pos(gUiCtx, Shared->MousePos);
    framebuffer_t *Fb = Shared->Framebuffer;
    
    clear_framebuffer(Fb, COLOR_BG);
    Fb->Clip = (irect_t){0, 0, Fb->Width, Fb->Height};
    
    draw_panels(Fb, &gCtx.PanelCtx, &gCtx.Font);
    
    ui_begin(gUiCtx);
    
    ui_open_container(gUiCtx, C_STR_AS_RANGE("test"), false);
    
    
    irect_t r = {.x = 100, .y = 100, .w = 600, .h = 700};
    ui_container_t *Container = ui_begin_container(gUiCtx, r, C_STR_AS_RANGE("test"));
    if(Container) {
        ui_push_clip(gUiCtx, Container->Rect);
        ui_draw_rect(gUiCtx, Container->Rect, color_u32(0xff0d1117));
        
        ui_row(gUiCtx, 1);
        {
            
            static mem_buffer_t Buffer = {0};
            ui_text_box(gUiCtx, &Buffer, C_STR_AS_RANGE("search box"), true);
            directory_t *Dir = &gCtx.CurrentDirectory;
            for(s64 i = 0; i < sb_count(Dir->Files); ++i) {
                file_info_t *Fi = Dir->Files + i;
                range_t FileName = {.Data = Dir->FileNameBuffer.Data + Fi->Name.Offset, .Size = Fi->Name.Size};
                if(filter_pass(FileName, mem_buf_as_range(Buffer))) {
                    if(ui_selectable(gUiCtx, FileName, false)) {
                        push_directory_or_open_file(Fi);
                        mem_buf_clear(&Buffer);
                    }
                }
            }
        }
        ui_pop_clip(gUiCtx);
        ui_end_container(gUiCtx);
    }
    
    ui_end(gUiCtx);
    draw_ui(Fb);
    
    Events->Count = 0;
}
