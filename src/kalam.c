#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "memory.h"
#include "types.h"
#include "event.h"
#include "platform.h"
#include "selection.h"
#include "panel.h"

#include "custom.h"

#include "kalam.h"

ctx_t Ctx = {0};

#include "selection.c"
#include "panel.c"
#include "layout.c"
#include "render.c"
#include "draw.c"

void
make_lines(buffer_t *Buf) {
    if(Buf->Text.Used == 0) {
        sb_push(Buf->Lines, (line_t){0});
    } else {
        sb_set_count(Buf->Lines, 0);
        line_t *Line = sb_add(Buf->Lines, 1);
        mem_zero_struct(Line);
        u8 n = 0;
        for(s64 i = 0; i < Buf->Text.Used; i += n) {
            n = utf8_char_width(Buf->Text.Data + i);
            
            Line->Size += n;
            if(Buf->Text.Data[i] == '\n') {
                Line->NewlineSize = n;
                // Push next line and init it
                Line = sb_add(Buf->Lines, 1);
                mem_zero_struct(Line);
                Line->Offset = i + n;
            } else {
                Line->Length += 1;
            }
        }
    }
}


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
    
    // TODO: Detect file encoding, we assume utf-8 for now
    make_lines(Buf);
}

void
k_init(platform_shared_t *Shared) {
    Ctx.Font = load_ttf("fonts/consola.ttf", 15);
    load_file("test.c");
    //load_file("../test/test.txt");
    
    u32 n = ARRAY_COUNT(Ctx.PanelCtx.Panels);
    for(u32 i = 0; i < n - 1; ++i) {
        Ctx.PanelCtx.Panels[i].Next = &Ctx.PanelCtx.Panels[i + 1];
    }
    Ctx.PanelCtx.Panels[n - 1].Next = 0;
    Ctx.PanelCtx.FreeList = &Ctx.PanelCtx.Panels[0];
    
    panel_create(&Ctx.PanelCtx);
}

b32
do_operation(operation_t Op) {
    b32 WasHandled = true;
    
    switch(Op.Type) {
        // Normal
        case OP_EnterInsertMode: {
            Ctx.PanelCtx.Selected->Mode = MODE_Insert;
        } break;
        
        case OP_DeleteSelection: {
        } break;
        
        case OP_ExtendSelection: {
            extend_selection(Ctx.PanelCtx.Selected, Op.ExtendSelection.Dir);
        } break;
        
        case OP_DropSelectionAndMove: {
            // TODO: This is mine field because pushing selections and cause a reallocation and will invalidate all previous pointers!!!
            panel_t *Panel = Ctx.PanelCtx.Selected;
            selection_t MaxIdxSel = *get_selection_max_idx(Panel);
            MaxIdxSel.Idx = Panel->SelectionIdxTop++;
            push_selection(Panel, MaxIdxSel);
            move_selection(Panel->Buffer, get_selection_max_idx(Panel), Op.DropSelectionAndMove.Dir, true);
            merge_overlapping_selections(Panel);
        } break;
        
        // Insert
        case OP_EscapeToNormal: {
            Ctx.PanelCtx.Selected->Mode = MODE_Normal;
        } break;
        
        case OP_Delete: {
        } break;
        
        case OP_DoChar: {
        } break;
        
        // Global
        case OP_Home: {
        } break;
        
        case OP_End: {
        } break;
        
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
    
    make_lines(Ctx.PanelCtx.Selected->Buffer);
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
find_mapping_match(key_mapping_t *Mappings, s64 MappingCount, input_event_t *e) {
    for(s64 i = 0; i < MappingCount; ++i) {
        key_mapping_t *m = &Mappings[i];
        if(event_mapping_match(m, e)) {
            return m;
        }
    }
    
    return 0;
}

void
k_do_editor(platform_shared_t *Shared) {
    input_event_buffer_t *Events = &Shared->EventBuffer;
    // TODO: Configurable keybindings
    for(s32 EventIndex = 0; EventIndex < Events->Count; ++EventIndex) {
        input_event_t *e = &Events->Events[EventIndex];
        if(e->Device == INPUT_DEVICE_Keyboard) {
            if(e->Type == INPUT_EVENT_Press || e->Type == INPUT_EVENT_Text) {
                b32 EventHandled = false;
                panel_t *Panel = Ctx.PanelCtx.Selected;
                if(Panel) {
                    if(Panel->Mode == MODE_Normal) {
                        key_mapping_t *m = find_mapping_match(NormalMappings, ARRAY_COUNT(NormalMappings), e);
                        if(m) {
                            EventHandled = do_operation(m->Operation);
                        }
                    } else if(Panel->Mode == MODE_Insert) {
                        key_mapping_t *m = find_mapping_match(InsertMappings, ARRAY_COUNT(InsertMappings), e);
                        if(m) {
                            EventHandled = do_operation(m->Operation);
                        } else if(e->Type == INPUT_EVENT_Text) {
                            // Ignore characters that were sent with the only modifier down being Ctrl (except for Caps Lock and Num Lock).
                            if((e->Modifiers & (INPUT_MOD_Ctrl | INPUT_MOD_Shift | INPUT_MOD_Alt)) != INPUT_MOD_Ctrl) {
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
                    }
                }
                
                if(!EventHandled) {
                    key_mapping_t *m = find_mapping_match(GlobalMappings, ARRAY_COUNT(GlobalMappings), e);
                    if(m) {
                        do_operation(m->Operation);
                    }
                }
                
            }
        }
        
    } Events->Count = 0;
    
    clear_framebuffer(Shared->Framebuffer, COLOR_BG);
    Shared->Framebuffer->Clip = (irect_t){0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
    
    if(!Ctx.PanelCtx.Root) {
        irect_t Rect = {0, 0, Shared->Framebuffer->Width, Shared->Framebuffer->Height};
        iv2_t p = center_text_in_rect(&Ctx.Font, Rect, (u8 *)"Hello there.", 0);
        draw_text_line(Shared->Framebuffer, &Ctx.Font, p.x, p.y, 0xffffffff, (u8 *)"Hello there.", 0);
    }
    
    draw_panels(Shared->Framebuffer, &Ctx.Font);
    
#if 0    
    for(u32 i = 0, x = 0; i < Ctx.Font.SetCount; ++i) {
        bitmap_t *b = &Ctx.Font.GlyphSets[i].Set->Bitmap;
        irect_t Rect = {0, 0, b->w, b->h};
        draw_glyph_bitmap(Shared->Framebuffer, x, Shared->Framebuffer->Height - Rect.h, 0xffffffff, Rect, b);
        x += b->w + 10;
    }
#endif
}
