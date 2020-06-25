#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"
#include "deps/stretchy_buffer.h"

#include "intrinsics.h"
#include "memory.h"
#include "types.h"
#include "event.h"
#include "platform.h"
#include "panel.h"

#include "custom.h"

#include "kalam.h"

ctx_t Ctx = {0};

#include "panel.c"
#include "layout.c"
#include "render.c"
#include "draw.c"

void
cursor_move(panel_t *Panel, cursor_t *Cursor, dir_t Dir, s64 StepSize) {
    if(!Panel) { return; }
    buffer_t *Buf = Panel->Buffer;
    int LineEndChars = 0; // TODO: CRLF, LF, LFCR, .....
    s64 Column = 0;
    switch(Dir) {
        case DOWN: 
        case UP: {
            if((Dir == UP && Cursor->Line > 0) ||
               (Dir == DOWN && Cursor->Line < (sb_count(Buf->Lines) - 1))) {
                s64 LineNum = CLAMP(0, sb_count(Buf->Lines) - 1, Cursor->Line + ((Dir == DOWN) ? StepSize : -StepSize));
                Column = MIN(Cursor->ColumnWas, Buf->Lines[LineNum].Length);
                
                Cursor->Line = LineNum;
                Cursor->ColumnIs = Column;
                Cursor->Offset = Buf->Lines[LineNum].Offset;
                for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
                    Cursor->Offset += utf8_char_width(Buf->Text.Data + Cursor->Offset);
                }
            }
        } break;
        
        case RIGHT: {
            s64 LineNum = Cursor->Line;
            Column = Cursor->ColumnIs;
            s64 LineCount = sb_count(Buf->Lines);
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column + 1 > Buf->Lines[LineNum].Length) {
                    if(LineNum + 1 >= LineCount) {
                        Column = Buf->Lines[LineNum].Length;
                        break;
                    }
                    LineNum++;
                    Column = 0;
                } else {
                    Column++;
                }
            }
            
            Cursor->Line = LineNum;
            Cursor->ColumnIs = Column;
            Cursor->ColumnWas = Cursor->ColumnIs;
            Cursor->Offset = Buf->Lines[LineNum].Offset;
            for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
                Cursor->Offset += utf8_char_width(Buf->Text.Data + Cursor->Offset);
            }
        } break;
        
        case LEFT: {
            s64 LineNum = Cursor->Line;
            Column = Cursor->ColumnIs;
            for(s64 i = 0; i < StepSize; ++i) {
                if(Column - 1 < 0) {
                    if(LineNum - 1 < 0) {
                        Column = 0;
                        break;
                    }
                    LineNum--;
                    Column = Buf->Lines[LineNum].Length;
                } else {
                    Column--;
                }
            }
            
            Cursor->Line = LineNum;
            Cursor->ColumnIs = Column;
            Cursor->ColumnWas = Cursor->ColumnIs;
            Cursor->Offset = Buf->Lines[LineNum].Offset;
            for(s64 i = 0; i < Cursor->ColumnIs; ++i) {
                Cursor->Offset += utf8_char_width(Buf->Text.Data + Cursor->Offset);
            }
        } break;
    }
}

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

void
insert_char(panel_t *Panel, u8 *Char) {
    
    // TODO: Cursor update
    
    buffer_t *Buf = Panel->Buffer;
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
        cursor_t *Cursor = &Panel->Cursors[CursorIndex];
        
        u8 n = utf8_char_width(Char);
        mem_buf_add(&Buf->Text, n);
        
        for(s64 i = Buf->Text.Used - 1; i >= Cursor->Offset; --i) {
            Buf->Text.Data[i + n] = Buf->Text.Data[i];
        }
        
        for(s64 i = 0; i < n; ++i) {
            Buf->Text.Data[Cursor->Offset + i] = Char[i];
        }
        
        // Update the following cursors
        for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
            cursor_t *c = &Panel->Cursors[i];
            c->Offset += n;
        }
    }
}

void
do_backspace_ex(panel_t *Panel, cursor_t *Cursor) {
    buffer_t *Buf = Panel->Buffer;
    if(Cursor->Offset != 0) {
        s32 n = 1;
        for(u8 *c = Buf->Text.Data + Cursor->Offset; (*(--c) & 0xc0) == 0x80; ++n);
        
        Cursor->Offset -= n;
        mem_buf_delete_range(&Buf->Text, Cursor->Offset, Cursor->Offset + n);
    }
}

void
do_backspace(panel_t *Panel) {
    buffer_t *Buf = Panel->Buffer;
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
        do_backspace_ex(Panel, &Panel->Cursors[CursorIndex]);
    }
}

void
scan_cursor_back_bytes(buffer_t *Buf, cursor_t *Cursor, u64 n) {
    if(n == 0) { return; }
    
    s64 DestOffset = Cursor->Offset - n;
    s64 Line = Cursor->Line;
    while(Buf->Lines[Line].Offset > DestOffset) {
        --Line;
    }
    s64 Column = 0;
    s64 Offset = Buf->Lines[Line].Offset;
    while(Offset < DestOffset) {
        Offset += utf8_char_width(Buf->Text.Data + Offset);
        ++Column;
    }
    Cursor->ColumnIs = Cursor->ColumnWas = Column;
    Cursor->Offset = DestOffset;
    Cursor->Line = Line;
}

void
delete_selection(panel_t *Panel) {
    
    // TODO: Cursor update
    
    for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
        cursor_t *Cursor = &Panel->Cursors[i];
        s64 Start, End;
        Start = MIN(Cursor->Offset, Cursor->Offset + Cursor->SelectionOffset);
        End = MAX(Cursor->Offset, Cursor->Offset + Cursor->SelectionOffset) + 1;
        s64 SelectionSize = End - Start;
        
        if(Cursor->SelectionOffset < 0) {
            scan_cursor_back_bytes(Panel->Buffer, Cursor, -Cursor->SelectionOffset);
        }
        
        mem_buf_delete_range(&Panel->Buffer->Text, Start, End);
        Cursor->SelectionOffset = 0;
    }
}

void
do_char(panel_t *Panel, u8 *Char) {
    if(!Panel) { return; }
    if(*Char < 0x20) {
        switch(*Char) {
            case '\n':
            case '\r': {
                insert_char(Panel, (u8 *)"\n");
            } break;
            
            case '\t': {
                insert_char(Panel, (u8 *)"\t");
            } break;
            
            case '\b': {
                do_backspace(Panel);
            } break;
        }
    } else {
        insert_char(Panel, Char);
    }
}

void
do_delete(panel_t *Panel) {
    
    // TODO: Cursor update
    
    buffer_t *Buf = Panel->Buffer;
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
        cursor_t *Cursor = &Panel->Cursors[CursorIndex];
        
        mem_buf_delete_range(&Buf->Text, Cursor->Offset, Cursor->Offset + utf8_char_width(Buf->Text.Data + Cursor->Offset));
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


void
fix_all_cursors_after_operation(panel_t *Panel, operation_t Op) {
    // We assume that atleast the offset of each cursor is correct
    buffer_t *Buf = Panel->Buffer;
    make_lines(Buf);
    
    for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
        cursor_t *Cursor = &Panel->Cursors[i];
        
        for(s64 li = 0; li < sb_count(Buf->Lines); ++li) {
            line_t *l = &Buf->Lines[li];
            if(Cursor->Offset >= l->Offset && Cursor->Offset <= (l->Offset + l->Size - l->NewlineSize)) {
                Cursor->Line = li;
                break;
            }
        }
        
        // TODO: CursorWas?
        Cursor->ColumnIs = 0;
        s64 Offset = Buf->Lines[Cursor->Line].Offset;
        while(Offset < Cursor->Offset) {
            Offset += utf8_char_width(Buf->Text.Data + Offset);
            ++Cursor->ColumnIs;
        }
    }
    
    // Remove any cursors that overlap
    for(s64 i = 0; i < sb_count(Panel->Cursors); ++i) {
        cursor_t *c0 = &Panel->Cursors[i];
        
        for(s64 j = i + 1; j < sb_count(Panel->Cursors); ++j) {
            cursor_t *c1 = &Panel->Cursors[j];
            
            if(c1->Offset == c0->Offset) {
                // Rearrange following cursors
                for(s64 k = j; k < sb_count(Panel->Cursors) - 1; ++k) {
                    Panel->Cursors[k] = Panel->Cursors[k + 1];
                }
                sb_set_count(Panel->Cursors, sb_count(Panel->Cursors) - 1);
            }
        }
    }
}

b32
do_operation(operation_t Op) {
    b32 WasHandled = true;
    
    switch(Op.Type) {
        // Normal
        case OP_EnterInsertMode: {
            Ctx.PanelCtx.Selected->Mode = MODE_Insert;
        } break;
        
        case OP_MoveCursorWithSelection: {
            switch(Op.MoveCursorWithSelection.Dir) {
                case LEFT: {
                    for(s64 i = 0; i < sb_count(Ctx.PanelCtx.Selected->Cursors); ++i) {
                        cursor_t Cursor = Ctx.PanelCtx.Selected->Cursors[i];
                        cursor_move(Ctx.PanelCtx.Selected, &Ctx.PanelCtx.Selected->Cursors[i], LEFT, 1);
                        Ctx.PanelCtx.Selected->Cursors[i].SelectionOffset = Cursor.SelectionOffset + ABS(Cursor.Offset - Ctx.PanelCtx.Selected->Cursors[i].Offset);
                    }
                } break;
                
                case RIGHT: {
                    for(s64 i = 0; i < sb_count(Ctx.PanelCtx.Selected->Cursors); ++i) {
                        cursor_t Cursor = Ctx.PanelCtx.Selected->Cursors[i];
                        cursor_move(Ctx.PanelCtx.Selected, &Ctx.PanelCtx.Selected->Cursors[i], RIGHT, 1);
                        Ctx.PanelCtx.Selected->Cursors[i].SelectionOffset = Cursor.SelectionOffset - ABS(Cursor.Offset - Ctx.PanelCtx.Selected->Cursors[i].Offset);
                    }
                } break;
                
                case UP: {
                } break;
                
                case DOWN: {
                } break;
            }
        } break;
        
        case OP_DeleteSelection: {
            delete_selection(Ctx.PanelCtx.Selected);
        } break;
        
        // Insert
        case OP_EscapeToNormal: {
            Ctx.PanelCtx.Selected->Mode = MODE_Normal;
        } break;
        
        case OP_Delete: {
            do_delete(Ctx.PanelCtx.Selected);
        } break;
        
        case OP_DoChar: {
            do_char(Ctx.PanelCtx.Selected, Op.DoChar.Character);
        } break;
        
        // Global
        case OP_Home: {
            buffer_t *Buf = Ctx.PanelCtx.Selected->Buffer;
            for(s64 i = 0; i < sb_count(Ctx.PanelCtx.Selected->Cursors); ++i) {
                cursor_t *c = &Ctx.PanelCtx.Selected->Cursors[i];
                line_t *l = &Buf->Lines[c->Line];
                c->Offset = l->Offset;
                c->ColumnIs = c->ColumnWas = 0;
                c->SelectionOffset = 0;
            }
        } break;
        
        case OP_End: {
            buffer_t *Buf = Ctx.PanelCtx.Selected->Buffer;
            for(s64 i = 0; i < sb_count(Ctx.PanelCtx.Selected->Cursors); ++i) {
                cursor_t *c = &Ctx.PanelCtx.Selected->Cursors[i];
                line_t *l = &Buf->Lines[c->Line];
                c->Offset = l->Offset + l->Size - l->NewlineSize;
                c->ColumnIs = c->ColumnWas = l->Length;
                c->SelectionOffset = 0;
            }
        } break;
        
        case OP_MoveCursor: {
            for(s64 i = 0; i < sb_count(Ctx.PanelCtx.Selected->Cursors); ++i) {
                cursor_t *Cursor = &Ctx.PanelCtx.Selected->Cursors[i];
                cursor_move(Ctx.PanelCtx.Selected, Cursor, Op.MoveCursor.Dir, Op.MoveCursor.StepSize); 
                Cursor->SelectionOffset = 0;
            }
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
        
        default: {
            WasHandled = false;
        } break;
    }
    
    fix_all_cursors_after_operation(Ctx.PanelCtx.Selected, Op);
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
    
    set_panel_scroll(Ctx.PanelCtx.Root, workspace_rect(Shared->Framebuffer));
    
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
