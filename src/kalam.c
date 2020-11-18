#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "platform.h"

#include "kalam.h"
#include "custom.h"

ctx_t Ctx = {0};

#include "selection.c"
#include "panel.c"
#include "layout.c"
#include "render.c"
#include "draw.c"

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
    
    stbtt_bakedchar *g = get_stbtt_bakedchar(&Font, 'M');
    if(g) {
        Font.MHeight = (s32)(g->y1 - g->y0 + 0.5);
        Font.MWidth = (s32)(g->x1 - g->x0 + 0.5);
    }
    
    Font.SpaceWidth = get_x_advance(&Font, ' ');
    
    return Font;
}

void
load_file(range_t Path) {
    platform_file_data_t File;
    if(platform_read_file(Path, &File)) {
        // TODO: Detect file encoding, we assume utf-8 for now
        buffer_t *OldBase = Ctx.Buffers;
        buffer_t *Buf = sb_add(Ctx.Buffers, 1);
        mem_zero_struct(Buf);
        
        if(Ctx.Buffers != OldBase) {
            for(u32 i = 0; i < PANEL_MAX; ++i) {
                panel_t *p = &Ctx.PanelCtx.Panels[i];
                if(p->IsLeaf) {
                    p->Buffer = &Ctx.Buffers[(p->Buffer - OldBase) / sizeof(buffer_t)];
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
}

void
k_init(platform_shared_t *Shared, range_t WorkingDirectory) {
    Ctx.Font = load_ttf(C_STR_AS_RANGE("fonts/consola.ttf"), 14);
    load_file(C_STR_AS_RANGE("test.c"));
    //load_file("../test/test.txt");
    
    mem_buf_append_range(&Ctx.WorkingDirectory, WorkingDirectory);
    mem_buf_append_range(&Ctx.SearchDirectory, WorkingDirectory);
    
    u32 n = ARRAY_COUNT(Ctx.PanelCtx.Panels);
    for(u32 i = 0; i < n - 1; ++i) {
        Ctx.PanelCtx.Panels[i].Next = &Ctx.PanelCtx.Panels[i + 1];
    }
    Ctx.PanelCtx.Panels[n - 1].Next = 0;
    Ctx.PanelCtx.FreeList = &Ctx.PanelCtx.Panels[0];
    
    panel_create(&Ctx.PanelCtx);
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

void
update_current_directory_files() {
    Ctx.SelectedFileIndex = 0;
    // Get the files and populate the options list.
    files_in_directory_t Dir = platform_get_files_in_directory(mem_buf_as_range(Ctx.SearchDirectory));
    
    mem_buf_clear(&Ctx.FileNames);
    sb_set_count(Ctx.FileNameInfo, 0);
    
    for(s64 i = 0; i < sb_count(Dir.Files); ++i) {
        file_info_t *Fi = &Dir.Files[i];
        if((Fi->FileName.Used == 1 && Fi->FileName.Data[0] == '.') || 
           (Fi->FileName.Used == 2 && Fi->FileName.Data[0] == '.' || Fi->FileName.Data[1] == '.')) {
            continue;
        }
        // Store the file name
        u64 Offset;
        void *d = mem_buf_add_idx(&Ctx.FileNames, Fi->FileName.Used, 1, &Offset);
        mem_copy(d, Fi->FileName.Data, Fi->FileName.Used);
        
        // Push file info
        file_name_info_t Info = {.Size = Fi->FileName.Used, .Offset = Offset, .Flags = Fi->Flags};
        sb_push(Ctx.FileNameInfo, Info);
    }
    
    free_files_in_directory(&Dir);
}

b32
do_operation(operation_t Op) {
    b32 WasHandled = true;
    
    switch(Op.Type) {
        case OP_SetMode: {
            set_mode(Ctx.PanelCtx.Selected, Op.SetMode.Mode);
        } break;
        
        // Normal
        case OP_Normal_Home: {
            panel_t *Panel = Ctx.PanelCtx.Selected;
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
            panel_t *Panel = Ctx.PanelCtx.Selected;
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
            delete_selection(Ctx.PanelCtx.Selected);
        } break;
        
        case OP_ExtendSelection: {
            extend_selection(Ctx.PanelCtx.Selected, Op.ExtendSelection.Dir);
        } break;
        
        case OP_DropSelectionAndMove: {
            panel_t *Panel = Ctx.PanelCtx.Selected;
            
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            selection_t Sel = get_selection_max_idx(SelGrp);
            Sel.Idx = SelGrp->SelectionIdxTop++;
            move_selection(Panel->Buffer, &Sel, Op.DropSelectionAndMove.Dir, true);
            sb_push(SelGrp->Selections, Sel);
            
            merge_overlapping_selections(Panel);
        } break;
        
        case OP_ClearSelections: {
            clear_selections(Ctx.PanelCtx.Selected);
        } break;
        
        case OP_OpenFileSelection: {
            mem_buf_replace(&Ctx.SearchDirectory, &Ctx.WorkingDirectory);
            update_current_directory_files();
            Ctx.WidgetFocused = WIDGET_FileSelect;
        } break;
        
        // Insert
        case OP_Insert_Home: {
            panel_t *Panel = Ctx.PanelCtx.Selected;
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
            panel_t *Panel = Ctx.PanelCtx.Selected;
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
            Ctx.PanelCtx.Selected->Mode = MODE_Normal;
        } break;
        
        case OP_Delete: {
            delete_selection(Ctx.PanelCtx.Selected);
        } break;
        
        case OP_DoChar: {
            do_char(Ctx.PanelCtx.Selected, Op.DoChar.Character);
        } break;
        
        // Global
        case OP_ToggleSplitMode: {
            Ctx.PanelCtx.Selected->Split ^= 1;
        } break;
        
        case OP_NewPanel: {
            panel_create(&Ctx.PanelCtx);
        } break;
        
        case OP_KillPanel: {
            panel_kill(&Ctx.PanelCtx, Ctx.PanelCtx.Selected);
        } break;
        
        case OP_MovePanelSelection: {
            panel_move_selection(&Ctx.PanelCtx, Op.MovePanelSelection.Dir); 
        } break;
        
        case OP_MoveSelection: {
            move_all_selections_in_panel(Ctx.PanelCtx.Selected, Op.MoveSelection.Dir);
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
event_is_control_sequence(input_event_t *Event) {
    if(Event->Device == INPUT_DEVICE_Keyboard && Event->Type == INPUT_EVENT_Text && 
       (Event->Modifiers & (INPUT_MOD_Ctrl | INPUT_MOD_Shift | INPUT_MOD_Alt)) != INPUT_MOD_Ctrl) {
        return false;
    }
    return true;
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

void
handle_panel_input(input_event_t Event) {
    if(Event.Device == INPUT_DEVICE_Keyboard) {
        if(Event.Type == INPUT_EVENT_Press || Event.Type == INPUT_EVENT_Text) {
            panel_t *Panel = Ctx.PanelCtx.Selected;
            if(Panel) {
                b32 EventHandled = false;
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
}

void
handle_file_select_input(input_event_t Event) {
    if(Event.Type == INPUT_EVENT_Text && !event_is_control_sequence(&Event)) {
        u8 *Char = Event.Text.Character;
        if(*Char < 0x20) {
            switch(*Char) {
                case '\r':
                case '\n': {
                    if(sb_count(Ctx.FileNameInfo) > 0) {
                        file_name_info_t *Fi = &Ctx.FileNameInfo[Ctx.SelectedFileIndex];
                        // TODO: The paths are not constructed correctly. What we need to do is take the search path and 
                        // auto-correct with the file or directory selected. This requires we truncate the search path
                        // and append the selected item. We need to take the type of delimiter into account when we do this.
                        if(Fi->Flags & FILE_FLAGS_Directory) {
                            mem_buf_append(&Ctx.WorkingDirectory, &Ctx.FileNames.Data[Fi->Offset], Fi->Size);
                            mem_buf_append_range(&Ctx.WorkingDirectory, C_STR_AS_RANGE("/"));
                            
                            mem_buf_replace(&Ctx.SearchDirectory, &Ctx.WorkingDirectory);
                            update_current_directory_files();
                        } else {
                            mem_buffer_t Path = {0};
                            
                            mem_buf_append_range(&Path, mem_buf_as_range(Ctx.SearchDirectory));
                            mem_buf_append_range(&Path, (range_t){.Data = Ctx.FileNames.Data + Fi->Offset, .Size = Fi->Size});
                            
                            load_file(mem_buf_as_range(Path));
                            panel_show_buffer(Ctx.PanelCtx.Selected, &Ctx.Buffers[sb_count(Ctx.Buffers) - 1]);
                            mem_buf_free(&Path);
                            
                            Ctx.WidgetFocused = WIDGET_Panels;
                        }
                    } else {
                        // Strip file name from Ctx.SearchDirectory and ask the user if a file with that name should be created.
                    }
                } break;
                
                case '\b': {
                    if(Ctx.SearchDirectory.Used > 0) {
                        u64 i = Ctx.SearchDirectory.Used - 1;
                        for(; i > 0 && (Ctx.SearchDirectory.Data[i] & 0xc0) == 0x80; --i);
                        Ctx.SearchDirectory.Used = i;
                        update_current_directory_files();
                    }
                } break;
            }
        } else {
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
}

void
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    for(s32 EventIndex = 0; EventIndex < Events->Count; ++EventIndex) {
        switch(Ctx.WidgetFocused) {
            case WIDGET_Panels: {
                handle_panel_input(Events->Events[EventIndex]);
            } break;
            
            case WIDGET_FileSelect: {
                handle_file_select_input(Events->Events[EventIndex]);
            } break;
        }
    } Events->Count = 0;
    
    clear_framebuffer(Shared->Framebuffer, COLOR_BG);
    Shared->Framebuffer->Clip = (irect_t){0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
    
    draw_panels(Shared->Framebuffer, &Ctx.Font);
    
    if(Ctx.WidgetFocused == WIDGET_FileSelect) {
        draw_file_menu(Shared->Framebuffer);
    }
}
