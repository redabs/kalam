#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "memory.h"
#include "types.h"
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
load_ttf(char *Path, f32 Size) {
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
load_file(char *Path) {
    // TODO: Detect file encoding, we assume utf-8 for now
    buffer_t *Buf = sb_add(Ctx.Buffers, 1);
    mem_zero_struct(Buf);
    platform_file_data_t File;
    ASSERT(platform_read_file(Path, &File));
    
    if(File.Size > 0) {
        mem_buf_add(&Buf->Text, File.Size);
        mem_copy(Buf->Text.Data, File.Data, File.Size);
    } else {
        mem_buf_grow(&Buf->Text, 128);
    }
    
    platform_free_file(&File);
    
    make_lines(Buf);
}

void
k_init(platform_shared_t *Shared, range_t WorkingDirectory) {
    Ctx.Font = load_ttf("fonts/consola.ttf", 14);
    load_file("test.c");
    //load_file("../test/test.txt");
    mem_buf_append(&Ctx.WorkingDirectory, WorkingDirectory.Data, WorkingDirectory.Size);
    
    u32 n = ARRAY_COUNT(Ctx.PanelCtx.Panels);
    for(u32 i = 0; i < n - 1; ++i) {
        Ctx.PanelCtx.Panels[i].Next = &Ctx.PanelCtx.Panels[i + 1];
    }
    Ctx.PanelCtx.Panels[n - 1].Next = 0;
    Ctx.PanelCtx.FreeList = &Ctx.PanelCtx.Panels[0];
    
    panel_create(&Ctx.PanelCtx);
}

file_select_option_t *
add_file_select_option(mem_buffer_t *OptionStorage, range_t String) {
    file_select_option_t *Dest = mem_buf_add(OptionStorage, sizeof(file_select_option_t) + String.Size);
    Dest->Size = String.Size;
    mem_copy(Dest->String, String.Data, String.Size);
    
    return Dest;
}

b32
do_operation(operation_t Op) {
    b32 WasHandled = true;
    
    switch(Op.Type) {
        
        case OP_SetMode: {
            panel_t *Panel = Ctx.PanelCtx.Selected;
            switch(Op.SetMode.Mode) {
                case MODE_Select: {
                    mode_select_ctx_t *SelectCtx = &Panel->ModeCtx.Select;
                    mem_buf_clear(&SelectCtx->SearchTerm);
                    SelectCtx->SelectionGroup.Owner = Panel;
                    SelectCtx->SelectionGroup.SelectionIdxTop = 0;
                    if(SelectCtx->SelectionGroup.Selections) {
                        sb_set_count(SelectCtx->SelectionGroup.Selections, 0);
                    }
                } break;
                
                case MODE_FileSelect: {
                    // Get the files and populate the options list.
                    files_in_directory_t Dir = platform_get_files_in_directory(mem_buffer_as_range(Ctx.WorkingDirectory));
                    
                    // Reset options
                    mem_buf_clear(&Ctx.EnumeratedFiles);
                    
                    for(s64 i = 0; i < sb_count(Dir.Files); ++i) {
                        file_info_t *Fi = &Dir.Files[i];
                        add_file_select_option(&Ctx.EnumeratedFiles, (range_t){.Data = Fi->FileName.Data, .Size = Fi->FileName.Used});
                    }
                    
                    free_files_in_directory(&Dir);
                } break;
            }
            
            Panel->Mode = Op.SetMode.Mode;
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
        
        case OP_EnterInsertMode: {
            Ctx.PanelCtx.Selected->Mode = MODE_Insert;
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
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    for(s32 EventIndex = 0; EventIndex < Events->Count; ++EventIndex) {
        input_event_t *e = &Events->Events[EventIndex];
        if(e->Device == INPUT_DEVICE_Keyboard) {
            if(e->Type == INPUT_EVENT_Press || e->Type == INPUT_EVENT_Text) {
                panel_t *Panel = Ctx.PanelCtx.Selected;
                if(Panel) {
                    b32 EventHandled = false;
                    switch(Panel->Mode) {
                        case MODE_Normal: {
                            key_mapping_t *m = find_mapping_match(NormalMappings, ARRAY_COUNT(NormalMappings), e);
                            if(m) {
                                EventHandled = do_operation(m->Operation);
                            }
                        } break;
                        
                        case MODE_Insert: {
                            key_mapping_t *m = find_mapping_match(InsertMappings, ARRAY_COUNT(InsertMappings), e);
                            if(m) {
                                EventHandled = do_operation(m->Operation);
                            } else if(e->Type == INPUT_EVENT_Text) {
                                // Ignore characters that were sent with the only modifier down being Ctrl (except for Caps Lock and Num Lock). I.e. ignore control sequences.
                                if(!event_is_control_sequence(e)) {
                                    operation_t o = {
                                        .Type = OP_DoChar, 
                                        .DoChar.Character[0] = e->Text.Character[0],
                                        .DoChar.Character[1] = e->Text.Character[1],
                                        .DoChar.Character[2] = e->Text.Character[2],
                                        .DoChar.Character[3] = e->Text.Character[3],
                                    };
                                    do_operation(o);
                                }
                                EventHandled = true;
                            }
                        } break;
                        
                        case MODE_Select: {
                            key_mapping_t *m = find_mapping_match(SelectMappings, ARRAY_COUNT(SelectMappings), e);
                            if(m) {
                                EventHandled = do_operation(m->Operation);
                            } else if(e->Type == INPUT_EVENT_Text && !event_is_control_sequence(e)) {
                                // Append text to search term 
                                u8 *Char = e->Text.Character;
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
                                            u8 *End = SearchTerm->Data + SearchTerm->Used;
                                            u8 *Begin = utf8_move_back_one(End);
                                            mem_buf_pop_size(SearchTerm, (u64)(End - Begin));
                                            sb_set_count(Panel->ModeCtx.Select.SelectionGroup.Selections, 0);
                                            update_select_state(Panel);
                                        } break;
                                    }
                                } else {
                                    sb_set_count(Panel->ModeCtx.Select.SelectionGroup.Selections, 0);
                                    mem_buf_append(SearchTerm, Char, utf8_char_width(Char));
                                    update_select_state(Panel);
                                }
                            }
                        } break;
                        
                        case MODE_FileSelect: {
                            key_mapping_t *m = find_mapping_match(FileSelectMappings, ARRAY_COUNT(FileSelectMappings), e);
                            if(m) {
                                EventHandled = do_operation(m->Operation);
                            } 
                        } break;
                    }
                    
                    if(!EventHandled) {
                        key_mapping_t *m = find_mapping_match(GlobalMappings, ARRAY_COUNT(GlobalMappings), e);
                        if(m) {
                            do_operation(m->Operation);
                        } 
                    }
                }
                
            }
        }
    } Events->Count = 0;
    
    clear_framebuffer(Shared->Framebuffer, COLOR_BG);
    Shared->Framebuffer->Clip = (irect_t){0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
    
    draw_panels(Shared->Framebuffer, &Ctx.Font);
    
    for(u64 Offset = 0, i = 0; Offset < Ctx.EnumeratedFiles.Used; ++i) {
        file_select_option_t *Opt = (file_select_option_t *)(&Ctx.EnumeratedFiles.Data[Offset]); 
        range_t Path = {.Data = Opt->String, .Size = Opt->Size};
        draw_text_line(Shared->Framebuffer, &Ctx.Font, 600, line_height(&Ctx.Font) * (s32)(i + 1), 0xffffffff, Path);
        Offset += Opt->Size + sizeof(file_select_option_t);
    }
    
#if 0    
    for(u32 i = 0, x = 0; i < Ctx.Font.SetCount; ++i) {
        bitmap_t *b = &Ctx.Font.GlyphSets[i].Set->Bitmap;
        irect_t Rect = {0, 0, b->w, b->h};
        draw_glyph_bitmap(Shared->Framebuffer, x, Shared->Framebuffer->Height - Rect.h, 0xffffffff, Rect, b);
        x += b->w + 10;
    }
#endif
}
