void
make_lines(buffer_t *Buf) {
    sb_set_count(Buf->Lines, 0);
    if(Buf->Text.Used == 0) {
        sb_push(Buf->Lines, (line_t){0});
    } else {
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
            Low = i + 1;
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

selection_group_t *
get_selection_group(buffer_t *Buffer, panel_t *Owner) {
    selection_group_t *SelGrp = 0;
    for(s64 i = 0; i < sb_count(Owner->Buffer->SelectionGroups); ++i) {
        selection_group_t *s = &Owner->Buffer->SelectionGroups[i];
        if(s->Owner == Owner) {
            SelGrp = s;
            break;
        }
    }
    ASSERT(SelGrp);
    return SelGrp;
}

selection_t
get_selection_max_idx(panel_t *Panel) {
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    selection_t Result = SelGrp->Selections[0];
    for(s64 i = 1; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *s = &SelGrp->Selections[i];
        if(s->Idx > Result.Idx) {
            Result = *s;
        }
    }
    return Result;
}

void
clear_selections(panel_t *Panel) {
    selection_t Sel = get_selection_max_idx(Panel);
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    sb_set_count(SelGrp->Selections, 0);
    sb_push(SelGrp->Selections, Sel);
}

void
move_selection(buffer_t *Buf, selection_t *Selection, dir_t Dir, b32 CarryAnchor) {
    switch(Dir) {
        case LEFT: {
            if(Selection->Cursor > 0) {
                u8 *c = utf8_move_back_one(Buf->Text.Data + Selection->Cursor);
                Selection->Cursor = (s64)(c - Buf->Text.Data);
                Selection->ColumnWas = global_offset_to_column(Buf, Selection->Cursor);
            }
        } break;
        
        case RIGHT: {
            if(Selection->Cursor < Buf->Text.Used) {
                Selection->Cursor += utf8_char_width(Buf->Text.Data + Selection->Cursor);
                Selection->ColumnWas = global_offset_to_column(Buf, Selection->Cursor);
            }
        } break;
        
        case UP: {
            s64 Li = offset_to_line_index(Buf, Selection->Cursor) - 1;
            if(Li < 0) { break; }
            Selection->Cursor = column_in_line_to_offset(Buf, &Buf->Lines[Li], Selection->ColumnWas);
        } break;
        
        case DOWN: {
            s64 Li = offset_to_line_index(Buf, Selection->Cursor) + 1;
            if(Li >= sb_count(Buf->Lines)) { break; }
            Selection->Cursor = column_in_line_to_offset(Buf, &Buf->Lines[Li], Selection->ColumnWas);
        } break;
    }
    
    Selection->ColumnIs = global_offset_to_column(Buf, Selection->Cursor);
    
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

#define sort_selection_group(Group) sort_selection_group_(Group, 0, sb_count(Group->Selections) - 1)
void
sort_selection_group_(selection_group_t *SelGrp, s64 Low, s64 High) {
    if(Low >= High) return; 
    s64 i = partition_selections(SelGrp->Selections, Low, High);
    sort_selection_group_(SelGrp, Low, i - 1);
    sort_selection_group_(SelGrp, i + 1, High);
}

void
merge_overlapping_selections(panel_t *Panel) {
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    sort_selection_group(SelGrp);
    
    // It's important that this sb_count is not optimized out by and means as we
    // delete selections in this loop when there is overlap.
    s64 i = 0; 
    while(i < sb_count(SelGrp->Selections) - 1) {
        selection_t *a = &SelGrp->Selections[i];
        selection_t *b = &SelGrp->Selections[i + 1];
        if(selections_overlap(a, b)) {
            selection_t New = {0};
            if(a->Cursor <= a->Anchor) {
                New.Cursor = MIN(a->Cursor, b->Cursor);
                New.Anchor = MAX(a->Anchor, b->Anchor);
            } else {
                New.Cursor = MAX(a->Cursor, b->Cursor);
                New.Anchor = MIN(a->Anchor, b->Anchor);
            }
            New.ColumnIs = New.ColumnWas = global_offset_to_column(Panel->Buffer, New.Cursor);
            New.Idx = SelGrp->SelectionIdxTop++;
            
            for(s64 j = i; j < sb_count(SelGrp->Selections) - 1; ++j) {
                SelGrp->Selections[j] = SelGrp->Selections[j + 1];
            }
            SelGrp->Selections[i] = New;
            
            sb_set_count(SelGrp->Selections, sb_count(SelGrp->Selections) - 1);
        } else {
            ++i;
        }
    }
}

void
move_all_selections_in_panel(panel_t *Panel, dir_t Dir) {
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        move_selection(Panel->Buffer, &SelGrp->Selections[i], Dir, true);
    }
    
    merge_overlapping_selections(Panel);
}

void
extend_selection(panel_t *Panel, dir_t Dir) {
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        move_selection(Panel->Buffer, &SelGrp->Selections[i], Dir, false);
    }
    merge_overlapping_selections(Panel);
}

typedef struct { 
    s64 Offset;
    s64 Size;
} deletion_t;

void
update_all_selections_after_deletions(panel_t *Panel, deletion_t *Deletions, u64 DeletionCount) {
    for(s64 SelGrpIndex = 0; SelGrpIndex < sb_count(Panel->Buffer->SelectionGroups); ++SelGrpIndex) {
        selection_group_t *SelGrp = &Panel->Buffer->SelectionGroups[SelGrpIndex];
        if(SelGrp->Owner == Panel) {
            // There's nothing to do for the selection group owned by Panel as it is assumed to be the 
            // panel that owns the selections with which the deletions was done.
            continue;
        } 
        
        for(s64 SelIndex = 0; SelIndex < sb_count(SelGrp->Selections); ++SelIndex) {
            selection_t *Sel = &SelGrp->Selections[SelIndex]; 
            for(s64 i = 0; i < sb_count(Deletions); ++i) {
                if(Sel->Cursor > Deletions[i].Offset) {
                    // A deletion might start before a cursor but end after it. So decrease the cursor at most by
                    // the distance from the start of the deletion to the cursor.
                    s64 d = Sel->Cursor - Deletions[i].Offset;
                    Sel->Cursor -= MIN(d, Deletions[i].Size);
                    Sel->Anchor = Sel->Cursor;
                    Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
                } else {
                    break;
                }
            } 
        }
        merge_overlapping_selections(SelGrp->Owner);
    }
}

typedef struct {
    s64 Offset; 
    s64 Size;
} insertion_t;

void
update_all_selections_after_insertions(panel_t *Panel, insertion_t *Insertions, u64 InsertionCount) {
    for(s64 SelGrpIndex = 0; SelGrpIndex < sb_count(Panel->Buffer->SelectionGroups); ++SelGrpIndex) {
        selection_group_t *SelGrp = &Panel->Buffer->SelectionGroups[SelGrpIndex];
        if(SelGrp->Owner == Panel) {
            // There's nothing to do for the selection group owned by Panel as it is assumed to be the 
            // panel that owns the selections with which the insertions was done.
            continue;
        } 
        
        for(s64 SelIndex = 0; SelIndex < sb_count(SelGrp->Selections); ++SelIndex) {
            selection_t *Sel = &SelGrp->Selections[SelIndex]; 
            for(s64 i = 0; i < sb_count(Insertions); ++i) {
                if(Sel->Cursor > Insertions[i].Offset) {
                    Sel->Cursor += Insertions[i].Size;
                    Sel->Anchor += Insertions[i].Size;
                    Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
                } else {
                    break;
                }
            } 
        }
        merge_overlapping_selections(SelGrp->Owner);
    }
}

void
delete_selection(panel_t *Panel) {
    deletion_t *Deletions = 0;
    
    buffer_t *Buf = Panel->Buffer;
    s64 BytesDeleted = 0;
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        Sel->Cursor -= BytesDeleted;
        Sel->Anchor -= BytesDeleted;
        
        
        s64 Start = selection_start(Sel);
        s64 End = selection_end(Sel);
        
        // This is block cursor specific behavior. We're including the character under cursor in the deletion
        End += utf8_char_width(Buf->Text.Data + End);
        mem_buf_delete(&Buf->Text, Start, End - Start);
        
        deletion_t d = {.Offset = Start, End - Start};
        sb_push(Deletions, d);
        
        Sel->Cursor = Sel->Anchor = Start;
        BytesDeleted += (End - Start);
    }
    
    make_lines(Buf);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        Sel->ColumnWas = Sel->ColumnIs = global_offset_to_column(Panel->Buffer, Sel->Cursor);
    }
    merge_overlapping_selections(Panel);
    
    update_all_selections_after_deletions(Panel, Deletions, sb_count(Deletions));
    sb_free(Deletions);
}

// If non-block cursors are ever supported then do_delete might be needed, until then delete_selection should behave the save
// for single character selection with a block cursor.
#if 0
void
do_delete(panel_t *Panel) {
    deletion_t *Deletions = 0;
    
    s64 BytesDeleted = 0;
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        if(Sel->Cursor < Panel->Buffer->Text.Used) {
            // Deletions at previous cursor shifts the buffer back so this cursor has to be moved by the number of bytes deleted.
            Sel->Cursor -= BytesDeleted;
            
            // Compute size of character before the cursor.
            s32 CharacterSize = utf8_char_width(Panel->Buffer->Text.Data + Sel->Cursor);
            mem_buf_delete(&Panel->Buffer->Text, Sel->Cursor, CharacterSize);
            
            deletion_t d = {.Offset = Sel->Cursor, .Size = CharacterSize};
            sb_push(Deletions, d);
            
            BytesDeleted += CharacterSize;
        }
    }
    
    make_lines(Panel->Buffer);
    
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        Sel->ColumnWas = Sel->ColumnIs = global_offset_to_column(Panel->Buffer, Sel->Cursor);
    }
    
    merge_overlapping_selections(Panel);
    
    update_all_selections_after_deletions(Panel, Deletions, sb_count(Deletions));
    sb_free(Deletions);
}
#endif


void
insert_char(panel_t *Panel, u8 *Char) {
    insertion_t *Insertions = 0;
    u8 n = utf8_char_width(Char);
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    
    // The selections are assumed to be in sorted order by cursor
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        Sel->Cursor += i * n; // Advances for inserts at previous selections
        mem_buf_insert(&Panel->Buffer->Text, Sel->Cursor, n, Char);
        
        insertion_t Insertion = {.Size = n, .Offset = Sel->Cursor};
        sb_push(Insertions, Insertion);
        
        Sel->Cursor += n; // Advance the cursor past the written character
        Sel->Anchor = Sel->Cursor;
    }
    
    make_lines(Panel->Buffer);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
    }
    
    update_all_selections_after_insertions(Panel, Insertions, sb_count(Insertions));
    sb_free(Insertions);
}

void
do_backspace(panel_t *Panel) {
    deletion_t *Deletions = 0;
    
    s64 BytesDeleted = 0;
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        if(Sel->Cursor > 0) {
            // Deletions at previous cursor shifts the buffer back so this cursor has to be moved by the number of bytes deleted.
            Sel->Cursor -= BytesDeleted;
            
            // Compute size of character before the cursor.
            s32 CharacterSize = 1;
            for(u8 *c = Panel->Buffer->Text.Data + Sel->Cursor; (*(--c) & 0xc0) == 0x80; ++CharacterSize);
            
            Sel->Cursor -= CharacterSize;
            Sel->Anchor = Sel->Cursor;
            mem_buf_delete(&Panel->Buffer->Text, Sel->Cursor, CharacterSize);
            
            deletion_t d = {.Offset = Sel->Cursor, .Size = CharacterSize};
            sb_push(Deletions, d);
            
            BytesDeleted += CharacterSize;
        }
    }
    
    make_lines(Panel->Buffer);
    
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        selection_t *Sel = &SelGrp->Selections[i];
        Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
    }
    
    update_all_selections_after_deletions(Panel, Deletions, sb_count(Deletions));
    sb_free(Deletions);
}

void
do_char(panel_t *Panel, u8 *Char) {
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
