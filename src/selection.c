s64
selection_start(selection_t *Selection) {
    s64 Start = MIN(Selection->Anchor, Selection->Cursor);
    return Start;
}

s64
selection_end(selection_t *Selection) {
    s64 End = MAX(Selection->Anchor, Selection->Cursor);
    return End;
}

s64
offset_to_line_index(buffer_t *Buf, s64 Offset) {
    s64 Low = 0;
    s64 High = sb_count(Buf->Lines);
    s64 i = 0;
    while(Low < High) {
        i = (Low + High) / 2;
        line_t *l = &Buf->Lines[i];
        if(Offset >= l->Offset && Offset < (l->Offset + l->Size)) {
            break;
        }
        
        if(l->Offset > Offset) {
            High = i;
        } else {
            Low = i;
        }
    }
    ASSERT(i >= 0 && i < sb_count(Buf->Lines)) ;
    return i;
}

s64
global_offset_to_column(buffer_t *Buf, s64 Offset) {
    line_t *Line = &Buf->Lines[offset_to_line_index(Buf, Offset)];
    s64 LineOffset = Offset - Line->Offset;
    s64 Column = 0;
    s64 c = 0;
    while(c < LineOffset) {
        c += utf8_char_width(Buf->Text.Data + Line->Offset + c);
        ++Column;
    }
    return Column;
}

s64
column_in_line_to_offset(buffer_t *Buf, line_t *Line, s64 Column) {
    Column = MIN(Line->Length, Column);
    s64 Offset = Line->Offset;
    for(s64 i = 0; i < Column; ++i) {
        Offset += utf8_char_width(Buf->Text.Data + Offset);
    }
    
    return Offset;
}

void
move_selection(buffer_t *Buf, selection_t *Selection, dir_t Dir, b32 WithAnchor) {
    switch(Dir) {
        case LEFT: {
            if(Selection->Cursor <= 0) { break; }
            u8 *c = utf8_move_back_one(Buf->Text.Data + Selection->Cursor);
            Selection->Cursor = (s64)(c - Buf->Text.Data);
            Selection->Column = global_offset_to_column(Buf, Selection->Cursor);
        } break;
        
        case RIGHT: {
            Selection->Cursor += utf8_char_width(Buf->Text.Data + Selection->Cursor);
            Selection->Column = global_offset_to_column(Buf, Selection->Cursor);
        } break;
        
        case UP: {
            s64 Li = offset_to_line_index(Buf, Selection->Cursor) - 1;
            if(Li < 0) { break; }
            Selection->Cursor = column_in_line_to_offset(Buf, &Buf->Lines[Li], Selection->Column);
        } break;
        
        case DOWN: {
            s64 Li = offset_to_line_index(Buf, Selection->Cursor) + 1;
            if(Li >= sb_count(Buf->Lines)) { break; }
            Selection->Cursor = column_in_line_to_offset(Buf, &Buf->Lines[Li], Selection->Column);
        } break;
        
    }
    
    if(WithAnchor) {
        Selection->Anchor = Selection->Cursor; 
    }
}

void
move_all_selections_in_panel(panel_t *Panel, dir_t Dir) {
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        move_selection(Panel->Buffer, &Panel->Selections[i], Dir, true);
    }
}

b32
selections_overlap(selection_t *a, selection_t *b) {
    s64 aEnd = selection_end(a);
    s64 bStart = selection_start(b);
    return (aEnd >= bStart);
}

void
merge_overlapping_selections(panel_t *Panel) {
}

void
extend_selection(panel_t *Panel, dir_t Dir) {
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        move_selection(Panel->Buffer, &Panel->Selections[i], Dir, false);
    }
}
