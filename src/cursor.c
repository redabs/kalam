s64
cursor_selection_start(cursor_t *Cursor) {
    return MIN(Cursor->Offset, Cursor->Offset + Cursor->SelectionOffset);
}

s64
cursor_selection_end(cursor_t *Cursor) {
    return MAX(Cursor->Offset, Cursor->Offset + Cursor->SelectionOffset);
}

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
            if(c->Offset >= Cursor->Offset) {
                c->Offset += n;
            }
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
    buffer_t *Buf = Panel->Buffer;
    for(s64 CursorIndex = 0; CursorIndex < sb_count(Panel->Cursors); ++CursorIndex) {
        cursor_t *Cursor = &Panel->Cursors[CursorIndex];
        
        mem_buf_delete_range(&Buf->Text, Cursor->Offset, Cursor->Offset + utf8_char_width(Buf->Text.Data + Cursor->Offset));
    }
}
