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
get_selection_max_idx_(panel_t *Panel) {
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
    sb_set_count(Panel->Selections, 0);
    sb_push(Panel->Selections, Sel);
}

void
clear_selections_(panel_t *Panel) {
    selection_t Sel = get_selection_max_idx_(Panel);
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

#define sort_selections(Panel) sort_selections_(Panel, 0, sb_count(Panel->Selections) - 1)
void
sort_selections_(panel_t *Panel, s64 Low, s64 High) {
    if(Low >= High) return;
    s64 i = partition_selections(Panel->Selections, Low, High);
    sort_selections_(Panel, Low, i - 1);
    sort_selections_(Panel, i + 1, High);
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
            New.ColumnWas = New.ColumnIs = global_offset_to_column(Panel->Buffer, New.Cursor);
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
merge_overlapping_selections_(panel_t *Panel) {
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
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        move_selection(Panel->Buffer, &Panel->Selections[i], Dir, true);
    }
    
    merge_overlapping_selections(Panel);
}

void
move_all_selections_in_panel_(panel_t *Panel, dir_t Dir) {
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        move_selection(Panel->Buffer, &SelGrp->Selections[i], Dir, true);
    }
    
    merge_overlapping_selections_(Panel);
}

void
extend_selection(panel_t *Panel, dir_t Dir) {
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        move_selection(Panel->Buffer, &Panel->Selections[i], Dir, false);
    }
    merge_overlapping_selections(Panel);
}

void
extend_selection_(panel_t *Panel, dir_t Dir) {
    selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
    for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
        move_selection(Panel->Buffer, &SelGrp->Selections[i], Dir, false);
    }
    merge_overlapping_selections_(Panel);
}

void
delete_selection(panel_t *Panel) {
    buffer_t *Buf = Panel->Buffer;
    s64 BytesDeleted = 0;
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        selection_t *Sel = &Panel->Selections[i];
        Sel->Cursor -= BytesDeleted;
        Sel->Anchor -= BytesDeleted;
        
        s64 Start = selection_start(Sel);
        s64 End = selection_end(Sel);
        End += utf8_char_width(Buf->Text.Data + End);
        
        mem_buf_delete(&Buf->Text, Start, End - Start);
        
        Sel->Cursor = Sel->Anchor = Start;
        BytesDeleted += (End - Start);
    }
    
    make_lines(Buf);
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        selection_t *Sel = &Panel->Selections[i];
        Sel->ColumnWas = Sel->ColumnIs = global_offset_to_column(Panel->Buffer, Sel->Cursor);
    }
    merge_overlapping_selections(Panel);
}

void
delete_selection_(panel_t *Panel) {
    typedef struct { 
        s64 Offset;
        s64 Size;
    } deletion_t;
    
    deletion_t *Deletions = 0;
    {
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
        merge_overlapping_selections_(Panel);
    }
    
    for(s64 SelGrpIndex = 0; SelGrpIndex < sb_count(Panel->Buffer->SelectionGroups); ++SelGrpIndex) {
        selection_group_t *SelGrp = &Panel->Buffer->SelectionGroups[SelGrpIndex];
        if(SelGrp->Owner == Panel) {
            // We've handled this already
            continue;
        } 
        
        for(s64 SelIndex = 0; SelIndex < sb_count(SelGrp->Selections); ++SelIndex) {
            selection_t *Sel = &SelGrp->Selections[SelIndex]; 
            for(s64 i = 0; i < sb_count(Deletions); ++i) {
                if(Sel->Cursor > Deletions[i].Offset) {
                    Sel->Cursor -= Deletions[i].Size;
                    Sel->Anchor = Sel->Cursor;
                    Sel->ColumnWas = Sel->ColumnIs = global_offset_to_column(Panel->Buffer, Sel->Cursor);
                } else {
                    break;
                }
            } 
        }
        merge_overlapping_selections_(SelGrp->Owner);
    }
    
    
    sb_free(Deletions);
}

void
do_delete(panel_t *Panel) {
    buffer_t *Buf = Panel->Buffer;
    s64 BytesDeleted = 0;
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        selection_t *Sel = &Panel->Selections[i];
        Sel->Cursor -= BytesDeleted;
        if(Sel->Cursor < Buf->Text.Used) {
            u8 n = utf8_char_width(Buf->Text.Data + Sel->Cursor);
            mem_buf_delete(&Buf->Text, Sel->Cursor, n);
            
            BytesDeleted += n;
        }
    }
    
    make_lines(Panel->Buffer);
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        selection_t *Sel = &Panel->Selections[i];
        Sel->Anchor = Sel->Cursor;
        Sel->ColumnWas = Sel->ColumnIs = global_offset_to_column(Panel->Buffer, Sel->Cursor);
    }
    merge_overlapping_selections(Panel);
}

void
do_delete_(panel_t *Panel) {
    typedef struct { 
        s64 Offset;
        s64 Size;
    } deletion_t;
    
    deletion_t *Deletions = 0;
    {
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
    }
    
    // Update the other selection sets
    
    for(s64 SelGrpIndex = 0; SelGrpIndex < sb_count(Panel->Buffer->SelectionGroups); ++SelGrpIndex) {
        selection_group_t *SelGrp = &Panel->Buffer->SelectionGroups[SelGrpIndex];
        if(SelGrp->Owner == Panel) {
            // We've handled this already
            continue;
        } 
        
        for(s64 SelIndex = 0; SelIndex < sb_count(SelGrp->Selections); ++SelIndex) {
            selection_t *Sel = &SelGrp->Selections[SelIndex]; 
            for(s64 i = 0; i < sb_count(Deletions); ++i) {
                if(Sel->Cursor > Deletions[i].Offset) {
                    Sel->Cursor -= Deletions[i].Size;
                    Sel->Anchor = Sel->Cursor;
                    Sel->ColumnWas = Sel->ColumnIs = global_offset_to_column(Panel->Buffer, Sel->Cursor);
                } else {
                    break;
                }
            } 
        }
        merge_overlapping_selections(SelGrp->Owner);
    }
    
    sb_free(Deletions);
}


void
insert_char(panel_t *Panel, u8 *Char) {
    u8 n = utf8_char_width(Char);
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        selection_t *Sel = &Panel->Selections[i];
        Sel->Cursor += i * n; // Advances for the previous cursors
        mem_buf_insert(&Panel->Buffer->Text, Sel->Cursor, n, Char);
        
        Sel->Cursor += n;
        Sel->Anchor = Sel->Cursor;
    }
    
    make_lines(Panel->Buffer);
    
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        selection_t *Sel = &Panel->Selections[i];
        Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
    }
    merge_overlapping_selections(Panel);
}

void
insert_char_(panel_t *Panel, u8 *Char) {
    // method 1, separate temp buffer, linear pass fast, gather scatter slow
    // Gather all selections into same buffer and tag on an owner panel pointer
    // Sort all owned selections
    // Write char at each selection owned by Panel and advance following selections accordingly
    // Scatter selections back into their respective selection groups
    // Merge overlapping cursors in each selection group indivudually
    
    
    {
        // method 2, in-place, for every insertion for each selection in every selection group
        // Write char at each selection in selection set owned by Panel and advance selections accordingly, but also store the step size and at which offset the char was written
        // For each selection in each selection group, go through each insertion and check if the offset is less than the offset at which the insertion was made then move the selection by the step size, repeat for each insertion for each selection in each selection group
        
        s64 *Offsets = 0;
        u8 n = utf8_char_width(Char);
        {
            selection_group_t *SelGrp = get_selection_group(Panel->Buffer, Panel);
            
            // The selections are assumed to be in sorted order by cursor
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *Sel = &SelGrp->Selections[i];
                Sel->Cursor += i * n; // Advances for inserts at previous selections
                mem_buf_insert(&Panel->Buffer->Text, Sel->Cursor, n, Char);
                sb_push(Offsets, Sel->Cursor);
                
                Sel->Cursor += n; // Advance the cursor past the written character
                Sel->Anchor = Sel->Cursor;
            }
            
            make_lines(Panel->Buffer);
            for(s64 i = 0; i < sb_count(SelGrp->Selections); ++i) {
                selection_t *Sel = &SelGrp->Selections[i];
                Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
            }
        }
        {
            // Now we need to advance every selection in every other selection group using the Offsets buffer
            for(s64 SelGrpIndex = 0; SelGrpIndex < sb_count(Panel->Buffer->SelectionGroups); ++SelGrpIndex) {
                selection_group_t *SelGrp = &Panel->Buffer->SelectionGroups[SelGrpIndex];
                if(SelGrp->Owner == Panel) {
                    // We've handled this already
                    continue;
                } 
                
                for(s64 SelIndex = 0; SelIndex < sb_count(SelGrp->Selections); ++SelIndex) {
                    selection_t *Sel = &SelGrp->Selections[SelIndex]; 
                    for(s64 OffsetIndex = 0; OffsetIndex < sb_count(Offsets); ++OffsetIndex) {
                        if(Sel->Cursor > Offsets[OffsetIndex]) {
                            Sel->Cursor += n;
                            Sel->Anchor += n;
                            Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
                        } else {
                            break;
                        }
                    } 
                }
            }
        }
        sb_free(Offsets);
    }
}

void
do_backspace(panel_t *Panel) {
    s64 BytesDeleted = 0;
    for(s64 i = 0; i < sb_count(Panel->Selections); ++i) {
        selection_t *Sel = &Panel->Selections[i];
        if(Sel->Cursor > 0) {
            Sel->Cursor -= BytesDeleted;
            
            s32 n = 1;
            for(u8 *c = Panel->Buffer->Text.Data + Sel->Cursor; (*(--c) & 0xc0) == 0x80; ++n);
            
            Sel->Cursor -= n;
            Sel->Anchor = Sel->Cursor;
            mem_buf_delete(&Panel->Buffer->Text, Sel->Cursor, n);
            
            BytesDeleted += n;
        }
    }
    
    make_lines(Panel->Buffer);
    merge_overlapping_selections(Panel);
}

void
do_backspace_(panel_t *Panel) {
    // The process is the same as inserting characters. For each cursor in the selection group owned by panel, delete a character backwards and save the offset in a temp buffer.
    // Adjusting all the other selection groups accordingly and remember to merge overlapping cursor and make_lines before setting the columns
    
    
    typedef struct { 
        s64 Offset;
        s64 Size;
    } deletion_t;
    
    deletion_t *Deletions = 0;
    {
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
    }
    
    // Update the other selection sets
    
    for(s64 SelGrpIndex = 0; SelGrpIndex < sb_count(Panel->Buffer->SelectionGroups); ++SelGrpIndex) {
        selection_group_t *SelGrp = &Panel->Buffer->SelectionGroups[SelGrpIndex];
        if(SelGrp->Owner == Panel) {
            // We've handled this already
            continue;
        } 
        
        for(s64 SelIndex = 0; SelIndex < sb_count(SelGrp->Selections); ++SelIndex) {
            selection_t *Sel = &SelGrp->Selections[SelIndex]; 
            for(s64 i = 0; i < sb_count(Deletions); ++i) {
                if(Sel->Cursor > Deletions[i].Offset) {
                    Sel->Cursor -= Deletions[i].Size;
                    Sel->Anchor = Sel->Cursor;
                    Sel->ColumnIs = Sel->ColumnWas = global_offset_to_column(Panel->Buffer, Sel->Cursor);
                } else {
                    break;
                }
            } 
        }
    }
    
    sb_free(Deletions);
}

void
do_char(panel_t *Panel, u8 *Char) {
    if(*Char < 0x20) {
        switch(*Char) {
            case '\n':
            case '\r': {
                //insert_char(Panel, (u8 *)"\n");
                insert_char_(Panel, (u8 *)"\n");
            } break;
            
            case '\t': {
                //insert_char(Panel, (u8 *)"\t");
                insert_char_(Panel, (u8 *)"\t");
            } break;
            
            case '\b': {
                //do_backspace(Panel);
                do_backspace_(Panel);
            } break;
        }
    } else {
        //insert_char(Panel, Char);
        insert_char_(Panel, Char);
    }
}
