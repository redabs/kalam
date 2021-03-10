#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "types.h"
#include "memory.h"
#include "platform.h"
#include "util.h"
#include "ui.h"
#include "kalam.h"

#include "parse.c"
#include "selection.c"
#include "panel.c"
#include "render.c"
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
    Font.Ascent = round_f32(Font.Ascent * Scale);
    Font.Descent = round_f32(Font.Descent * Scale);
    Font.LineGap = round_f32(Font.LineGap * Scale);
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
                if(p->State == PANEL_STATE_Leaf) {
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
    gUiCtx->Input = &Shared->InputState;
}

void
set_mode(panel_t *Panel, mode_t Mode) {
    switch(Mode) {
        case MODE_Select: {
            mode_select_ctx_t *SelectCtx = &Panel->Select;
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
    mem_buffer_t *SearchTerm = &Panel->Select.SearchTerm;
    mode_select_ctx_t *SelectCtx = &Panel->Select;
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
    framebuffer_t *Fb = Shared->Framebuffer;
    
    clear_framebuffer(Fb, COLOR_BG);
    Fb->Clip = (irect_t){0, 0, Fb->Width, Fb->Height};
    
    ui_begin(gUiCtx);
    
    ui_open_container(gUiCtx, C_STR_AS_RANGE("test"), false);
    
    irect_t Rect; 
    ui_id_t Id;
    
    irect_t r = {.w = Fb->Width, .h = Fb->Height};
    ui_container_t *Container = ui_begin_container(gUiCtx, r, C_STR_AS_RANGE("test"));
    if(Container) {
        ui_push_clip(gUiCtx, Container->Rect);
        ui_draw_rect(gUiCtx, Container->Rect, color_u32(0xff0d1117));
        
        ui_row(gUiCtx, 2);
        
        ui_begin_column(gUiCtx, 200);
        static mem_buffer_t Buffer = {0};
        
        ui_text_box(gUiCtx, &Buffer, C_STR_AS_RANGE("search box"), C_STR_AS_RANGE("search"), false);
        directory_t *Dir = &gCtx.CurrentDirectory;
        for(s64 i = 0; i < sb_count(Dir->Files); ++i) {
            file_info_t *Fi = Dir->Files + i;
            range_t FileName = {.Data = Dir->FileNameBuffer.Data + Fi->Name.Offset, .Size = Fi->Name.Size};
            if(filter_pass(FileName, mem_buf_as_range(Buffer))) {
                if(ui_selectable(gUiCtx, FileName, false)) {
                    //push_directory_or_open_file(Fi);
                    //mem_buf_clear(&Buffer);
                }
            }
            
        }
        ui_end_column(gUiCtx);
        
        { // Code edit widget
            ui_begin_column(gUiCtx, 0);
            Id = ui_make_id(gUiCtx, C_STR_AS_RANGE("code"));
            Rect = ui_push_rect(gUiCtx, 0, 0);
            ui_update_input_state(gUiCtx, Rect, Id);
            if(gUiCtx->Active == Id || gUiCtx->Selected == Id) {
                gUiCtx->TextEditFocus = Id;
            }
            
            if(gUiCtx->Hot == Id) {
                gCtx.PanelCtx.Selected->Scroll.y -= gUiCtx->Input->Scroll * 40; 
            }
            
            if(gUiCtx->TextEditFocus == Id) {
                // TODO: Code edit input
                
            }
            
            ui_panels(gUiCtx, gCtx.PanelCtx.Root, Rect);
            ui_end_column(gUiCtx);
        }
        
        ui_pop_clip(gUiCtx);
        ui_end_container(gUiCtx);
    }
    
    ui_end(gUiCtx);
    draw_ui(Fb);
    
    MEM_STACK_CLEAR(Shared->InputState.Keys);
}
