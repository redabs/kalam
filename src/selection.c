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

selection_t *
add_selection(panel_t *Panel) {
    selection_t *s = sb_add(Panel->Selections, 1);
    mem_zero_struct(s);
    s->Idx = Panel->SelectionIdxTop++;
    return s;
}

// It is the caller's responsibilty to set Idx of Selection and increment the top index in the context.
void
push_selection(panel_t *Panel, selection_t Selection) {
    selection_t *s = sb_add(Panel->Selections, 1);
    *s = Selection;
}

selection_t
get_selection_max_idx(panel_t *Panel) {
    u64 Max = Panel->Selections[0].Idx;
    selection_t *Result = &Panel->Selections[0];
    for(s64 i = 1; i < sb_count(Panel->Selections); ++i) {
        selection_t *s = &Panel->Selections[i];
        if(s->Idx > Max) { 
            Result = s;
            Max = s->Idx;
        }
    }
    
    return *Result;
}

void
clear_selections(panel_t *Panel) {
    selection_t Sel = get_selection_max_idx(Panel);
    sb_set_count(Panel->Selections, 0);
    sb_push(Panel->Selections, Sel);
}

void
move_selection(buffer_t *Buf, selection_t *Selection, dir_t Dir, b32 CarryAnchor) {
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
    
    if(CarryAnchor) {
        Selection->Anchor = Selection->Cursor; 
    }
}

b32
selections_overlap(selection_t *a, selection_t *b) {
    s64 aEnd = selection_end(a);
    s64 bStart = selection_start(b);
    return (aEnd >= bStart);
}

s64
partition_selections(selection_t *Selections, s64 Low, s64 High) {
    selection_t Pivot = Selections[High];
    s64 pStart = selection_start(&Pivot);
    s64 Mid = Low;
#define SWAP(a, b) selection_t _temp_ = a; a = b; b = _temp_;
    
    for(s64 i = Low; i < High; ++i) {
        if(selection_start(&Selections[i]) <= pStart) {
            SWAP(Selections[i], Selections[Mid]);
            ++Mid;
        }
    }
    SWAP(Selections[High], Selections[Mid]);
#undef SWAP
    return Mid;
}

#define sort_selections(Panel) sort_selections_(Panel, 0, sb_count(Panel->Selections) - 1)
void
sort_selections_(panel_t *Panel, s64 Low, s64 High) {
    if(Low >= High) return;
    s64 i = partition_selections(Panel->Selections, Low, High);
    sort_selections_(Panel, Low, i - 1);
    sort_selections_(Panel, i + 1, High);
}

void
merge_overlapping_selections(panel_t *Panel) {
    sort_selections(Panel);
    
    // It's important that this sb_count is not optimized out by and means as we
    // delete selections in this loop when there is overlap.
    s64 i = 0; 
    while(i < sb_count(Panel->Selections) - 1) {
        selection_t *a = &Panel->Selections[i];
        selection_t *b = &Panel->Selections[i + 1];
        if(selections_overlap(a, b)) {
            selection_t New = {0};
            if(a->Cursor <= a->Anchor) {
                New.Cursor = MIN(a->Cursor, b->Cursor);
                New.Anchor = MAX(a->Anchor, b->Anchor);
            } else {
                New.Cursor = MAX(a->Cursor, b->Cursor);
                New.Anchor = MIN(a->Anchor, b->Anchor);
            }
            New.Column = global_offset_to_column(Panel->Buffer, New.Cursor);
            New.Idx = Panel->SelectionIdxTop++;
            
            for(s64 j = i; j < sb_count(Panel->Selections) - 1; ++j) {
                Panel->Selections[j] = Panel->Selections[j + 1];
            }
            Panel->Selections[i] = New;
            
            sb_set_count(Panel->Selections, sb_count(Panel->Selections) - 1);
        } else {
            ++i;
        }
    }
}

void
move_all_selections_in_panel(panel_t *Panel, dir_t Dir) {
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        move_selection(Panel->Buffer, &Panel->Selections[i], Dir, true);
    }
    merge_overlapping_selections(Panel);
}

void
extend_selection(panel_t *Panel, dir_t Dir) {
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        move_selection(Panel->Buffer, &Panel->Selections[i], Dir, false);
    }
    merge_overlapping_selections(Panel);
}
