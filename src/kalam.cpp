#define _USE_MATH_DEFINES
#include <math.h>

#include "kalam.h"

#define STBTT_malloc(x, u) ((void)(u), gPlatform.alloc(x))
#define STBTT_free(x, u) ((void)(u), gPlatform.free(x))

#include "k_memory.cpp"
#include "k_glyph.cpp"

kalam_ctx gCtx;

file_buffer
load_file(view<u8> Path) {
    file_buffer FileBuffer = {};
    
    FileBuffer.Text = gPlatform.read_file(Path);
    push(&FileBuffer.Path, Path);
    FileBuffer.Mode = EDIT_MODE_Command;
    add(&FileBuffer.Selections, 1);
    
    return FileBuffer;
}

void
save_buffer(file_buffer *Buffer) {
    gPlatform.write_file(make_view(Buffer->Path), make_view(Buffer->Text));
}

void
kalam_init() {
    gCtx.GlyphCache = glyph_cache_make(C_STR_VIEW("../fonts/liberation-mono.ttf"), 48);
    file_buffer Buffer = load_file(C_STR_VIEW("test.cpp"));
    push(&gCtx.Buffers, Buffer);
}

b8
overlap(irect Clip, irect Rect, irect *Out) {
    irect Result = {};
    Result.x = MAX(Clip.x, Rect.x);
    Result.y = MAX(Clip.y, Rect.y);
    
    s32 ClippedLeft = Result.x - Rect.x;
    s32 SpareWidthInClip = Clip.x + Clip.w - Result.x;
    Result.w = MAX(0, MIN(Rect.w - ClippedLeft, SpareWidthInClip));
    
    s32 ClippedTop = Result.y - Rect.y;
    s32 SpareHeightInClip = Clip.y + Clip.h - Result.y;
    Result.h = MAX(0, MIN(Rect.h - ClippedTop, SpareHeightInClip));
    
    if(Out) { *Out = Result; }
    return (Result.w > 0 && Result.h > 0);
}

void
draw_rect(framebuffer *Fb, irect Clip, irect Rect, u32 Color) {
    irect Overlap = {};
    if(!overlap(Clip, Rect, &Overlap)) {
        return;
    }
    
    f32 a = (f32)((Color >> 24) & 0xff) / 255.f;
    f32 r = (f32)((Color >> 16) & 0xff) * a;
    f32 g = (f32)((Color >> 8) & 0xff) * a;
    f32 b = (f32)((Color >> 0) & 0xff) * a;
    
    for(s32 y = Overlap.y; y < (Overlap.y + Overlap.h); ++y) {
        for(s32 x = Overlap.x; x < (Overlap.x +  Overlap.w); ++x) {
            
            u32 DestPixel = Fb->Pixels[y * Fb->Width + x];
            
            f32 Dr = (f32)((DestPixel >> 16) & 0xff);
            f32 Dg = (f32)((DestPixel >> 8) & 0xff);
            f32 Db = (f32)((DestPixel >> 0) & 0xff);
            
            u8 Rr = (u8)(r * a + Dr * (1 - a));
            u8 Rg = (u8)(g * a + Dg * (1 - a));
            u8 Rb = (u8)(b * a + Db * (1 - a));
            
            Fb->Pixels[y * Fb->Width + x] = Rr << 16 | Rg << 8 | Rb;
        }
    }
}

void
draw_cursor(framebuffer *Fb, irect Rect, u32 Color) {
    irect FbClip = {0, 0, Fb->Width, Fb->Height};
    if(!overlap(FbClip, Rect, &Rect)) {
        return;
    }
    
    f32 a = (f32)((Color >> 24) & 0xff) / 255.f;
    f32 r = (f32)((Color >> 16) & 0xff) * a;
    f32 g = (f32)((Color >> 8) & 0xff) * a;
    f32 b = (f32)((Color >> 0) & 0xff) * a;
    
    for(s32 y = Rect.y; y < (Rect.y + Rect.h); ++y) {
        for(s32 x = Rect.x; x < (Rect.x +  Rect.w); ++x) {
            u32 DestPixel = Fb->Pixels[y * Fb->Width + x];
            
            f32 Dr = (f32)((DestPixel >> 16) & 0xff);
            f32 Dg = (f32)((DestPixel >> 8) & 0xff);
            f32 Db = (f32)((DestPixel >> 0) & 0xff);
            
            u8 Rr = (u8)(r * a + Dr * (1 - a));
            u8 Rg = (u8)(g * a + Dg * (1 - a));
            u8 Rb = (u8)(b * a + Db * (1 - a));
            
            Fb->Pixels[y * Fb->Width + x] = Rr << 16 | Rg << 8 | Rb;
        }
    }
}

void
draw_glyph(framebuffer *Fb, irect Clip, s32 Top, s32 Left, u8 *Glyph, s32 Stride, s32 Width, s32 Height, u32 Color) {
    irect GlyphRect = {Left, Top, Width, Height};
    irect ClippedGlyphRect = {};
    if(!overlap(Clip, GlyphRect, &ClippedGlyphRect)) {
        return;
    }
    
    f32 a = (f32)((Color >> 24) & 0xff) / 255.f;
    f32 r = (f32)((Color >> 16) & 0xff) * a;
    f32 g = (f32)((Color >> 8) & 0xff) * a;
    f32 b = (f32)((Color >> 0) & 0xff) * a;
    
    s32 SrcStartX = ClippedGlyphRect.x - GlyphRect.x;
    s32 SrcStartY = ClippedGlyphRect.y - GlyphRect.y;
    for(s32 y = 0; y < ClippedGlyphRect.h; ++y) {
        s32 DestY = y + ClippedGlyphRect.y;
        s32 SrcY = y + SrcStartY;
        
        for(s32 x = 0; x < ClippedGlyphRect.w; ++x) {
            s32 DestX = x + ClippedGlyphRect.x;
            s32 SrcX = x + SrcStartX;
            
            f32 Alpha = (f32)Glyph[SrcY * Stride + SrcX] / 255.f;
            
            u32 DestPixel = Fb->Pixels[DestY * Fb->Width + DestX];
            
            f32 Dr = (f32)((DestPixel >> 16) & 0xff);
            f32 Dg = (f32)((DestPixel >> 8) & 0xff);
            f32 Db = (f32)((DestPixel >> 0) & 0xff);
            
            u8 Rr = (u8)(r * Alpha + Dr * (1 - Alpha));
            u8 Rg = (u8)(g * Alpha + Dg * (1 - Alpha));
            u8 Rb = (u8)(b * Alpha + Db * (1 - Alpha));
            
            Fb->Pixels[DestY * Fb->Width + DestX] = Rr << 16 | Rg << 8 | Rb;
        }
    }
}

// Returns a line_end with the type set to LINE_ENDING_None if it's not a line ending.
inline line_ending_type
is_line_ending(view<u8> Text) {
    line_ending_type Result = LINE_ENDING_None;
    
    u16 Char = (Text.Count > 1) ? *(u16 *)Text.Ptr : *Text.Ptr;
    switch(Char & 0xFF) {
        case LINE_ENDING_Cr: {
            if(Char == LINE_ENDING_CrLf) {
                Result = LINE_ENDING_CrLf;
            } else {
                Result = LINE_ENDING_Cr;
            }
            break;
        } 
        
        case LINE_ENDING_Rs:
        case LINE_ENDING_Nl:
        case LINE_ENDING_Lf: { 
            Result = (line_ending_type)(Char & 0xff); 
        } break;
    }
    return Result;
}

u64
offset_to_line_index(view<line> Lines, u64 Offset) {
    // TODO: Better search/lookup
    u64 LineIdx = 0;
    for(; LineIdx < Lines.Count - 1; ++LineIdx) {
        line *Line = &Lines.Ptr[LineIdx];
        if(Offset >= Line->Offset && Offset < (Line->Offset + Line->Size + line_ending_size(Line->LineEnding)))  {
            return LineIdx;
        }
    }
    line *LastLine = &Lines.Ptr[Lines.Count - 1];
    // Cursor can be one past the end of the last line, more than that is a bug.
    ASSERT(Offset < (LastLine->Offset + LastLine->Size + 1));
    return LineIdx;
}

u64
column_in_line_to_offset(view<u8> Text, line *Line, u64 Column) {
    u64 Offset = Line->Offset;
    u64 LineEnd = Offset + Line->Size;
    
    for(u64 i = 0; (i < Column && Offset < LineEnd); ++i) {
        Offset += utf8_char_size(Text.Ptr[Offset]);
    }
    
    return Offset;
}

u64
offset_to_column(view<u8> Text, line *Line, u64 Offset) {
    u64 Column = 0;
    u64 End = MIN(Line->Offset + Line->Size, Offset);
    for(u64 i = Line->Offset; i < End; Column++) {
        i += utf8_char_size(Text.Ptr[i]);
    }
    
    return Column;
}

void
move_cursor_up(file_buffer *Buffer, selection *Sel) {
    u64 LineIdx = offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
    if(LineIdx > 0) {
        Sel->Cursor = column_in_line_to_offset(make_view(Buffer->Text), Buffer->Lines.Ptr + LineIdx - 1, Sel->Column);
    }
}

void
move_cursor_down(file_buffer *Buffer, selection *Sel) {
    u64 NextLineIdx = offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor) + 1;
    if(NextLineIdx < Buffer->Lines.Count) {
        Sel->Cursor = column_in_line_to_offset(make_view(Buffer->Text), Buffer->Lines.Ptr + NextLineIdx, Sel->Column);
    }
}

void
move_cursor_left(file_buffer *Buffer, selection *Sel) {
    u64 LineIdx = offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
    line *Line = Buffer->Lines.Ptr + LineIdx;
    
    if(LineIdx > 0 && Sel->Cursor == Line->Offset) {
        // Move up one line
        line *PrevLine = Line - 1;
        if(PrevLine->LineEnding) {
            Sel->Cursor -= line_ending_size(PrevLine->LineEnding);
        } else {
            Sel->Cursor -= utf8_step_back_one(Buffer->Text.Ptr + Sel->Cursor, Sel->Cursor);
        }
        Sel->Column = offset_to_column(make_view(Buffer->Text), PrevLine, PrevLine->Offset + PrevLine->Size);
    } else {
        Sel->Cursor -= utf8_step_back_one(Buffer->Text.Ptr + Sel->Cursor, Sel->Cursor);
        Sel->Column -= 1;
    }
}

void
move_cursor_right(file_buffer *Buffer, selection *Sel) {
    if(Sel->Cursor < Buffer->Text.Count) {
        u64 LineIdx = offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
        line *Line = Buffer->Lines.Ptr + LineIdx;
        if(Sel->Cursor == (Line->Offset + Line->Size)) {
            // If we're at the end of the line then move past its line ending (which can be multiple bytes)
            Sel->Cursor += line_ending_size(Line->LineEnding);
            Sel->Column = 0; 
        } else {
            Sel->Cursor += utf8_char_size(Buffer->Text.Ptr[Sel->Cursor]);
            Sel->Column += 1; 
        }
    }
}

inline u64
selection_start(selection *Sel) {
    u64 Start = MIN(Sel->Cursor, Sel->Anchor);
    return Start;
}

inline u64
selection_end(selection *Sel) {
    u64 End = MAX(Sel->Cursor, Sel->Anchor);
    return End;
}

u64
partition_selections(selection *Selections, s64 Low, s64 High) {
    u64 Pivot = selection_start(&Selections[High]);
    u64 Mid = Low;
#define SWAP(a, b) selection _temp_ = *a; *a = *b; *b = _temp_;
    for(s64 i = Low; i < High; ++i) {
        if(selection_start(&Selections[i]) <= Pivot) {
            SWAP(&Selections[i], &Selections[Mid]);
            ++Mid;
        }
    }
    
    SWAP(&Selections[High], &Selections[Mid]);
#undef SWAP
    return Mid;
}

inline void
sort_selections(selection *Selections, s64 Low, s64 High) {
    if(Low >= High) return;
    u64 i = partition_selections(Selections, Low, High);
    sort_selections(Selections, Low, i - 1);
    sort_selections(Selections, i + 1, High);
}

inline void
sort_selections(view<selection> Selections) {
    if(Selections.Count <= 1) return;
    sort_selections(Selections.Ptr, 0, Selections.Count - 1);
}

void
merge_overlapping_selections(buffer<selection> *Selections) {
    sort_selections(make_view(*Selections));
    
    for(u64 i = 0; i < Selections->Count - 1;) {
        selection *a = &Selections->Ptr[i];
        selection *b = &Selections->Ptr[i + 1];
        // Check for overlap
        if(selection_end(a) >= selection_start(b)) {
            // Join selections
            selection New = {};
            if(a->Cursor <= a->Anchor) {
                New.Cursor = MIN(a->Cursor, b->Cursor);
                New.Anchor = MAX(a->Anchor, b->Anchor);
                New.Column = a->Column;
            } else {
                New.Cursor = MAX(a->Cursor, b->Cursor);
                New.Anchor = MIN(a->Anchor, b->Anchor);
                New.Column = b->Column;
            }
            //New.Idx = SelectionGroup->Idx++;
            
            // Shuffle the following selections down as a hole was just created
            // TODO: Performance, can we do this at the end instead of doing it for every merge?
            for(u64 j = i; j < (Selections->Count - 1); ++j) {
                Selections->Ptr[j] = Selections->Ptr[j + 1];
            }
            Selections->Ptr[i] = New;
            Selections->Count -= 1;
        } else {
            ++i;
        }
    }
}

void
insert_text(file_buffer *Buffer, view<u8> Text) {
    for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
        selection *Sel = Buffer->Selections.Ptr + i;
        if(Sel->Cursor > Sel->Anchor) {
            u64 Temp = Sel->Cursor;
            Sel->Cursor = Sel->Anchor;
            Sel->Anchor = Temp;
        }
        u64 n = i * Text.Count;
        insert(&Buffer->Text, Text.Ptr, Text.Count, Sel->Cursor + n);
        Sel->Cursor += Text.Count + n;
        Sel->Anchor += Text.Count + n;
        Sel->Column += utf8_char_count(Text);
    }
}

void
delete_selection(file_buffer *Buffer) {
    u64 DeletedBytesAccumulator = 0;
    for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
        selection *Sel = Buffer->Selections.Ptr + i;
        u64 Start = selection_start(Sel) - DeletedBytesAccumulator;
        u64 End = selection_end(Sel) - DeletedBytesAccumulator;
        if(Sel->Cursor == Sel->Anchor) {
            End += utf8_char_size(Buffer->Text.Ptr[End]);
        }
        End = MIN(End, Buffer->Text.Count);
        u64 Count = End - Start;
        remove(&Buffer->Text, Start, Count);
        DeletedBytesAccumulator += Count;
        Sel->Cursor = Sel->Anchor = Start;
    }
    merge_overlapping_selections(&Buffer->Selections);
}

void
do_backspace(file_buffer *Buffer) {
    u64 DeletedBytesAccumulator = 0;
    for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
        selection *Sel = Buffer->Selections.Ptr + i;
        Sel->Cursor -= DeletedBytesAccumulator;
        Sel->Anchor -= DeletedBytesAccumulator;
        u8 n = utf8_step_back_one(Buffer->Text.Ptr + Sel->Cursor, Sel->Cursor);
        Sel->Cursor -= n;
        Sel->Anchor -= n;
        Sel->Column--;
        remove(&Buffer->Text, Sel->Cursor, n);
        DeletedBytesAccumulator += n;
    }
    merge_overlapping_selections(&Buffer->Selections);
}

b8
event_key_match(key_event Event, key Key, modifier Modifiers) {
    return !Event.IsText && Event.Key == Key && Event.Modifiers == Modifiers;
}

void
draw_text(framebuffer *Fb, irect Clip, view<u8> Text, iv2 Origin, f32 Scale) {
    glyph_key_data Gkd = {};
    Gkd.Scale = Scale; 
    
    u8 CharSize = 0;
    glyph_info *Glyph = 0;
    s32 LineX = Origin.x;
    for(u64 i = 0; i < Text.Count; i += CharSize) {
        CharSize = utf8_to_codepoint(Text.Ptr + i, &Gkd.Codepoint);
        Glyph = glyph_cache_get(&gCtx.GlyphCache, Gkd);
        s32 GlyphAdvance = (s32)(Glyph->Scale * Glyph->Advance + 0.5f);
        s32 LeftBearing = (s32)(Glyph->LeftSideBearing * Glyph->Scale + 0.5f);
        irect GlyphRect = {
            LineX + LeftBearing,
            Origin.y + Glyph->y0, 
            Glyph->x1 - Glyph->x0,
            Glyph->y1 - Glyph->y0
        };
        
        draw_glyph(Fb, 
                   Clip,
                   GlyphRect.y, GlyphRect.x, 
                   gCtx.GlyphCache.Texture + Glyph->TextureOffset, 
                   gCtx.GlyphCache.Columns * gCtx.GlyphCache.CellWidth, 
                   GlyphRect.w, GlyphRect.h, 
                   0xfffafafa);
        
        LineX += GlyphAdvance;
    }
}

s32
text_width(view<u8> Text, f32 Scale) {
    glyph_key_data GlyphKeyData = {};
    GlyphKeyData.Scale = Scale;
    GlyphKeyData.FontId = 0;
    
    s32 Result = 0;
    for(u32 i = 0; i < Text.Count;) {
        i += utf8_to_codepoint(Text.Ptr + i, &GlyphKeyData.Codepoint);
        glyph_info *Glyph = glyph_cache_get(&gCtx.GlyphCache, GlyphKeyData);
        Result += (s32)(Glyph->Scale * Glyph->Advance + 0.5f);
    }
    return Result;
}

struct font_metrics {
    s32 LineGap; // Positive number of pixels between lines
    s32 Ascent;  // Positive number of pixels extending down from baseline
    s32 Descent; // Positive number of pixels extending up from baseline
    f32 Scale;
    s32 VerticalAdvance;
};

font_metrics
get_font_metrics(f32 FontSize) {
    font_metrics Result = {};
    Result.Scale = stbtt_ScaleForPixelHeight(&gCtx.GlyphCache.FontInfo, FontSize);
    stbtt_GetFontVMetrics(&gCtx.GlyphCache.FontInfo, &Result.Ascent, &Result.Descent, &Result.LineGap);
    Result.Descent = (s32)(-Result.Descent * Result.Scale + 0.5f); 
    Result.Ascent = (s32)(Result.Ascent * Result.Scale + 0.5f);
    Result.LineGap = (s32)(Result.LineGap * Result.Scale + 0.5f); 
    Result.VerticalAdvance = Result.Ascent + Result.Descent + Result.LineGap;
    return Result;
}

void
draw_selections(framebuffer *Fb, view<u8> Buffer, view<line> Lines, view<selection> Selections, irect TextRect, font_metrics FontMetrics) {
    s32 Baseline = TextRect.y + FontMetrics.Ascent + gCtx.Scroll.y;
    
    glyph_key_data GlyphKeyData = {};
    GlyphKeyData.Scale = FontMetrics.Scale;
    GlyphKeyData.FontId = 0;
    
    // Draw selections
    for(u64 SelIdx = 0; SelIdx < Selections.Count; ++SelIdx) {
        selection *Sel = Selections.Ptr + SelIdx;
        u64 StartOffset = MIN(Sel->Cursor, Sel->Anchor);
        u64 EndOffset = MAX(Sel->Cursor, Sel->Anchor);
        
        u64 StartLineIdx = offset_to_line_index(Lines, StartOffset);
        u64 EndLineIdx = offset_to_line_index(Lines, EndOffset);
        
        for(u64 LineIdx = StartLineIdx; LineIdx < (EndLineIdx + 1); ++LineIdx) {
            line *Line = Lines.Ptr + LineIdx;
            u64 InLineStartOffset = MAX(StartOffset, Line->Offset);
            u64 InLineEndOffset = MIN(EndOffset, Line->Offset + Line->Size + line_ending_size(Line->LineEnding));
            
            irect SelectionRect = {};
            SelectionRect.x = TextRect.x;
            SelectionRect.y = (s32)(TextRect.y + FontMetrics.VerticalAdvance * LineIdx + gCtx.Scroll.y);
            SelectionRect.h = FontMetrics.Ascent + FontMetrics.Descent;
            // Find the pixel offset to the first character included in the selection
            for(u64 i = Line->Offset, CharSize = 0; i < InLineStartOffset; i += CharSize) {
                CharSize = utf8_to_codepoint(Buffer.Ptr + i, &GlyphKeyData.Codepoint);
                glyph_info *Glyph = glyph_cache_get(&gCtx.GlyphCache, GlyphKeyData);
                SelectionRect.x += (s32)(Glyph->Advance * Glyph->Scale + 0.5f);
            }
            
            for(u64 i = InLineStartOffset, CharSize = 0; i < InLineEndOffset; i += CharSize) {
                CharSize = utf8_to_codepoint(Buffer.Ptr + i, &GlyphKeyData.Codepoint);
                glyph_info *Glyph = glyph_cache_get(&gCtx.GlyphCache, GlyphKeyData);
                SelectionRect.w += (s32)(Glyph->Advance * Glyph->Scale + 0.5f);
            }
            
            draw_cursor(Fb, SelectionRect, 0x6fffff7c);
        }
    }
    
    // Draw cursor
    for(u64 i = 0; i < Selections.Count; ++i) {
        selection *Sel = Selections.Ptr + i;
        
        irect CursorRect = {};
        glyph_info *Glyph = 0; 
        
        u64 LineIdx = offset_to_line_index(Lines, Sel->Cursor);
        line *Line = Lines.Ptr + LineIdx;
        CursorRect.x = TextRect.x;
        CursorRect.y = (s32)(TextRect.y + FontMetrics.VerticalAdvance * LineIdx + gCtx.Scroll.y);
        
        Glyph = glyph_cache_get(&gCtx.GlyphCache, GlyphKeyData);
        ASSERT(Glyph);
        
        u8 CharByteCount = 1;
        // Search forward in the line to find the character under the cursor so we can
        // size the cursor properly. 
        for(u64 Offset = Line->Offset; Offset < Sel->Cursor; Offset += CharByteCount) {
            u8 *Char = Buffer.Ptr + Offset;
            CharByteCount = utf8_to_codepoint(Char, &GlyphKeyData.Codepoint);
            Glyph = glyph_cache_get(&gCtx.GlyphCache, GlyphKeyData);
            CursorRect.x += (s32)(Glyph->Scale * Glyph->Advance + 0.5f);
        }
        CursorRect.w = (s32)(300 * Glyph->Scale + 0.5f); //(s32)(Glyph->Advance * Glyph->Scale + 0.5f);
        CursorRect.h = FontMetrics.Ascent + FontMetrics.Descent;
        
        draw_cursor(Fb, CursorRect, 0xff74fc0c);
    }
}

void
handle_input_event(key_event Event, file_buffer *Buffer) {
    // TODO: Keybindings to commands mapping...
    switch(Buffer->Mode) {
        case EDIT_MODE_Insert: {
            if(Event.IsText) {
                insert_text(Buffer, make_view(Event.Char, utf8_char_size(*Event.Char)));
                
            } else if(event_key_match(Event, KEY_Return, MOD_None)) {
                insert_text(Buffer, C_STR_VIEW("\n"));
                
            } else if(event_key_match(Event, KEY_Home, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    line *Line = Buffer->Lines.Ptr + offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    Sel->Cursor = Sel->Anchor = Line->Offset;
                    Sel->Column = 0;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_End, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    line *Line = Buffer->Lines.Ptr + offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    Sel->Cursor = Sel->Anchor = Line->Offset + Line->Size;
                    Sel->Column = Line->Length;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_Delete, MOD_None)) {
                delete_selection(Buffer);
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_Backspace, MOD_None)) {
                do_backspace(Buffer);
                
            } else if(event_key_match(Event, KEY_Escape, MOD_None)) {
                Buffer->Mode = EDIT_MODE_Command;
                
            } else if(event_key_match(Event, KEY_Up, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_up(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_Down, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_down(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_Left, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_left(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_Right, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_right(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            }
        } break;
        
        case EDIT_MODE_Command: {
            if(event_key_match(Event, KEY_S, MOD_Ctrl)) {
                save_buffer(Buffer);
                
            } else if(event_key_match(Event, KEY_Return, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    if(Sel->Cursor > Sel->Anchor) {
                        u64 Temp = Sel->Cursor;
                        Sel->Cursor = Sel->Anchor;
                        Sel->Anchor = Temp;
                    }
                }
                Buffer->Mode = EDIT_MODE_Insert;
                
            } else if(Event.IsText && Event.Char[0] == 's') {
                Buffer->Mode = EDIT_MODE_Select;
                clear(&Buffer->SelectSelections);
                push(&Buffer->SelectSelections, make_view(Buffer->Selections)); 
                clear(&Buffer->SelectTerm);
                
            } else if(Event.IsText && Event.Char[0] == 'A') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    u64 LineIdx = offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    line *Line = Buffer->Lines.Ptr + LineIdx;
                    Sel->Cursor = Sel->Anchor = Line->Offset + Line->Size;
                    Sel->Column = offset_to_column(make_view(Buffer->Text), Line, Sel->Cursor);
                }
                Buffer->Mode = EDIT_MODE_Insert;
                merge_overlapping_selections(&Buffer->Selections);
                
            } else if(event_key_match(Event, KEY_Return, MOD_Shift)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    u64 LineIdx = offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    line *Line = Buffer->Lines.Ptr + LineIdx;
                    Sel->Cursor = Sel->Anchor = Line->Offset;
                    Sel->Column = 0;
                }
                Buffer->Mode = EDIT_MODE_Insert;
                merge_overlapping_selections(&Buffer->Selections);
                
            } else if(Event.IsText && Event.Char[0] == 'x') {
                // Select the entire line until the line ending character
                // If pressed multiple times the selection should proceed to the next line
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    // If the cursor is at the end of the line moving it right puts it at the start of the next line.
                    // This effectively scan following lines one at a time each keypress
                    move_cursor_right(Buffer, Sel);
                    line *Line = Buffer->Lines.Ptr + offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    Sel->Anchor = Line->Offset;
                    Sel->Cursor = Line->Offset + Line->Size;
                    Sel->Column = offset_to_column(make_view(Buffer->Text), Line, Sel->Cursor);
                }
                merge_overlapping_selections(&Buffer->Selections);
                
            } else if(Event.IsText && Event.Char[0] == 'X') {
                // Same as 'x' but extend the current selection down
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    line *Line = Buffer->Lines.Ptr + offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    
                    Sel->Anchor = MIN(Line->Offset, Sel->Anchor);
                    move_cursor_right(Buffer, Sel);
                    
                    Line = Buffer->Lines.Ptr + offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    Sel->Cursor = Line->Offset + Line->Size;
                    Sel->Column = offset_to_column(make_view(Buffer->Text), Line, Sel->Cursor);
                }
                merge_overlapping_selections(&Buffer->Selections);
                
            } else if(event_key_match(Event, KEY_Home, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    line *Line = Buffer->Lines.Ptr + offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    Sel->Anchor = Sel->Cursor;
                    Sel->Cursor = Line->Offset;
                    Sel->Column = 0;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_End, MOD_None)) {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    line *Line = Buffer->Lines.Ptr + offset_to_line_index(make_view(Buffer->Lines), Sel->Cursor);
                    Sel->Anchor = Sel->Cursor;
                    Sel->Cursor = Line->Offset + Line->Size;
                    Sel->Column = Line->Length;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            }
            
            // Extend selection
            else if(Event.IsText && Event.Char[0] == 'I') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_up(Buffer, Sel);
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(Event.IsText && Event.Char[0] == 'K') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_down(Buffer, Sel);
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(Event.IsText && Event.Char[0] == 'J') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_left(Buffer, Sel);
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(Event.IsText && Event.Char[0] == 'L') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_right(Buffer, Sel);
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(Event.IsText && Event.Char[0] == 'i') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_up(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(Event.IsText && Event.Char[0] == 'k') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_down(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(Event.IsText && Event.Char[0] == 'j') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_left(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(Event.IsText && Event.Char[0] == 'l') {
                for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                    selection *Sel = Buffer->Selections.Ptr + i;
                    move_cursor_right(Buffer, Sel);
                    Sel->Anchor = Sel->Cursor;
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_Space, MOD_None)) {
                Buffer->Selections.Count = 1;
                
            } else if(Event.IsText && Event.Key == 'd' ||
                      event_key_match(Event, KEY_Delete, MOD_None)) {
                delete_selection(Buffer);
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_I, MOD_Alt)) {
                u64 Count = Buffer->Selections.Count;
                for(u64 i = 0; i < Count; ++i) {
                    selection SelectionCopy = Buffer->Selections.Ptr[i];
                    move_cursor_up(Buffer, &SelectionCopy);
                    SelectionCopy.Anchor = SelectionCopy.Cursor;
                    push(&Buffer->Selections, SelectionCopy);
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_K, MOD_Alt)) {
                u64 Count = Buffer->Selections.Count;
                for(u64 i = 0; i < Count; ++i) {
                    selection SelectionCopy = Buffer->Selections.Ptr[i];
                    move_cursor_down(Buffer, &SelectionCopy);
                    SelectionCopy.Anchor = SelectionCopy.Cursor;
                    push(&Buffer->Selections, SelectionCopy);
                }
                merge_overlapping_selections(&Buffer->Selections); 
                
            } else if(event_key_match(Event, KEY_I, MOD_Alt | MOD_Shift)) {
                Buffer->Mode = EDIT_MODE_SelectInner;
            }
            
        } break;
        
        case EDIT_MODE_Select: {    
            b32 UpdateSelection = false;
            if(Event.IsText) {
                push(&Buffer->SelectTerm, make_view(Event.Char, utf8_char_size(*Event.Char)));
                UpdateSelection = true;
                
            } else if(event_key_match(Event, KEY_Escape, MOD_None)) {
                Buffer->Mode = EDIT_MODE_Command;
                
            } else if(event_key_match(Event, KEY_Backspace, MOD_None)) {
                Buffer->SelectTerm.Count -= utf8_step_back_one(Buffer->SelectTerm.Ptr + Buffer->SelectTerm.Count, Buffer->SelectTerm.Count);
                UpdateSelection = true;
                
            } else if(event_key_match(Event, KEY_Return, MOD_None)) {
                clear(&Buffer->Selections);
                push(&Buffer->Selections, make_view(Buffer->SelectSelections));
                Buffer->Mode = EDIT_MODE_Command;
            }
            
            if(UpdateSelection) {
                clear(&Buffer->SelectSelections);
                if(Buffer->SelectTerm.Count > 0) {
                    for(u64 SelIdx = 0; SelIdx < Buffer->Selections.Count; ++SelIdx) {
                        selection *Sel = Buffer->Selections.Ptr + SelIdx;
                        u64 Start = selection_start(Sel);
                        u64 End = selection_end(Sel);
                        
                        u64 SearchStart = Start;
                        u64 FoundOffset = 0;
                        while(utf8_find_in_text(make_view(Buffer->Text.Ptr + SearchStart, End - SearchStart), make_view(Buffer->SelectTerm), &FoundOffset)) {
                            selection NewSel = {};
                            NewSel.Anchor = SearchStart + FoundOffset;
                            NewSel.Cursor = SearchStart + FoundOffset + Buffer->SelectTerm.Count;
                            // TODO: This is a global search through the entire file. The Search could be narrowed to be
                            // between lines where the selection starts and ends.
                            u64 LineIdx = offset_to_line_index(make_view(Buffer->Lines), NewSel.Cursor);
                            NewSel.Column = offset_to_column(make_view(Buffer->Text), &Buffer->Lines.Ptr[LineIdx], NewSel.Cursor);
                            NewSel.Panel = Sel->Panel;
                            push(&Buffer->SelectSelections, NewSel);
                            
                            // Continue search after found search term
                            SearchStart = SearchStart + FoundOffset + Buffer->SelectTerm.Count;
                        }
                    }
                }
            }
        } break;
        
        case EDIT_MODE_SelectInner: {
            if(Event.IsText) {
                b32 Invalid = false;
                u32 Open = 0;
                u32 Close = 0;
                switch(Event.Char[0]) {
                    case '[':
                    case ']': {
                        Open = '[';
                        Close = ']';
                    } break;
                    
                    case '<':
                    case '>': {
                        Open = '<';
                        Close = '>';
                    } break;
                    
                    case '(':
                    case ')': {
                        Open = '(';
                        Close = ')';
                    } break;
                    
                    case '{':
                    case '}': {
                        Open = '{';
                        Close = '}';
                    } break;
                    
                    case '\'':
                    case '"': {
                        Open = Close = Event.Char[0];
                    } break;
                    
                    default: { Invalid = true; }
                } 
                
                if(!Invalid) {
                    for(u64 i = 0; i < Buffer->Selections.Count; ++i) {
                        selection *Sel = Buffer->Selections.Ptr + i;
                        s64 OpenOffset = -1;
                        s64 CloseOffset = -1;
                        s64 Closed = 0;
                        s64 Opened = 0;
                        
                        // Try to find the opening and closing character and expand the selection to all characters
                        // between the opening and close
                        for(u64 Cursor = Sel->Cursor; Cursor > 0; Cursor -= utf8_step_back_one(Buffer->Text.Ptr + Cursor, Cursor)) {
                            // Only count openings and closings 'if we're selecting inside e.g. [] {} <>, i.e. the opening and closing
                            // characters are not the same
                            if(Open != Close && utf8_char_equals(Buffer->Text.Ptr + Cursor, (u8*)&Close)) {
                                Closed++;
                                
                            } else if(utf8_char_equals(Buffer->Text.Ptr + Cursor, (u8*)&Open)) {
                                if(Closed > 0) {
                                    Closed--;
                                } else {
                                    OpenOffset = (s64)Cursor + utf8_char_size((u8)Buffer->Text.Ptr[Cursor]);
                                    break;
                                }
                            }
                        }
                        
                        for(u64 Cursor = Sel->Cursor; Cursor < Buffer->Text.Count; Cursor += utf8_char_size(Buffer->Text.Ptr[Cursor])) {
                            if(Open != Close && utf8_char_equals(Buffer->Text.Ptr + Cursor, (u8*)&Open)) {
                                Opened++;
                                
                            } else if(utf8_char_equals(Buffer->Text.Ptr + Cursor, (u8*)&Close)) {
                                if(Opened > 0) {
                                    Opened--;
                                } else {
                                    CloseOffset = (s64)Cursor;
                                    break;
                                }
                            }
                        }
                        
                        if(OpenOffset != -1 && CloseOffset != -1) {
                            Sel->Anchor = OpenOffset;
                            Sel->Cursor = CloseOffset;
                            
                            merge_overlapping_selections(&Buffer->Selections);
                        }
                        
                        Buffer->Mode = EDIT_MODE_Command;
                        
                        
                    }
                } else {
                    Buffer->Mode = EDIT_MODE_Command;
                }
            } else if (event_key_match(Event, KEY_Escape, MOD_None)) {
                Buffer->Mode = EDIT_MODE_Command;
            }
            
        } break;
    }
}

void 
kalam_update_and_render(input_state *Input, framebuffer *Fb, f32 Dt) {
    draw_rect(Fb, {0, 0, Fb->Width, Fb->Height}, {0, 0, Fb->Width, Fb->Height}, 0xff0d1117);
    
    for(u32 EventIndex = 0; EventIndex < Input->EventCount; ++EventIndex) {
        handle_input_event(Input->Events[EventIndex], &gCtx.Buffers.Ptr[gCtx.BufferIdx]);
    }
    
    file_buffer *Buffer = &gCtx.Buffers.Ptr[gCtx.BufferIdx];
    
    int ScrollSpeed = 20;
    static f32 FontSize = 17;
    font_metrics FontMetrics = get_font_metrics(FontSize);
    
    if(Input->ScrollModifiers == MOD_Ctrl) {
        FontSize += Input->Scroll;
        FontSize = MAX(0, MIN(FontSize, gCtx.GlyphCache.MaxFontPixelHeight));
        
    } else if (Input->ScrollModifiers == MOD_Shift) {
        gCtx.Scroll.x += Input->Scroll * ScrollSpeed;
        
    } else if (Input->ScrollModifiers == MOD_None) {
        gCtx.Scroll.y += Input->Scroll * ScrollSpeed;
    }
    
    irect Panel = {0, 0, Fb->Width, Fb->Height};
    irect LineNumberMargin = {Panel.x, Panel.y, 0, Panel.h};
    
    {
        // Compute line number margin width or whatever it's called..
        glyph_key_data GlyphKeyData = {};
        GlyphKeyData.Scale = FontMetrics.Scale;
        GlyphKeyData.FontId = 0;
        utf8_to_codepoint((u8*)"0", &GlyphKeyData.Codepoint);
        
        glyph_info *Glyph = glyph_cache_get(&gCtx.GlyphCache, GlyphKeyData);
        s32 GlyphAdvance = (s32)(Glyph->Scale * Glyph->Advance + 0.5f);
        
        for(u64 i = Buffer->Lines.Count; i > 0; i /= 10) {
            LineNumberMargin.w += GlyphAdvance;
        }
    }
    s32 BottomBarsHeightTotal = 0;
    irect CommandLine = {LineNumberMargin.x, Panel.h - FontMetrics.VerticalAdvance, Panel.w, FontMetrics.VerticalAdvance};
    {
        draw_rect(Fb, Panel, CommandLine, 0xff161616);
        s32 Baseline = CommandLine.y + FontMetrics.Ascent;
        if(Buffer->Mode == EDIT_MODE_Select) {
            draw_text(Fb, CommandLine, make_view(Buffer->SelectTerm), {CommandLine.x, Baseline}, FontMetrics.Scale);
        }
    }
    BottomBarsHeightTotal += CommandLine.h;
    
    irect StatusBar = {LineNumberMargin.x, CommandLine.y - FontMetrics.VerticalAdvance, Panel.w, FontMetrics.VerticalAdvance};
    {
        draw_rect(Fb, Panel, StatusBar, 0xff262626);
        view<u8> ModeString = edit_mode_string(Buffer->Mode);
        s32 ModeStringWidth = text_width(ModeString, FontMetrics.Scale);
        irect ModeRect = {StatusBar.x, StatusBar.y, ModeStringWidth + (s32)(1229 * FontMetrics.Scale * 2), StatusBar.h};
        
        s32 Baseline = StatusBar.y + FontMetrics.Ascent;
        u32 ModeColor = Buffer->Mode == EDIT_MODE_Command ? 0xff000066 : 0xff669933;
        draw_rect(Fb, StatusBar, ModeRect, ModeColor);
        draw_text(Fb, StatusBar, make_view(Buffer->Path), {ModeRect.x + ModeRect.w + (s32)(1229 * FontMetrics.Scale), Baseline}, FontMetrics.Scale);
        iv2 ModeStringPosition = {StatusBar.x + (ModeRect.w - ModeStringWidth) / 2, Baseline};
        draw_text(Fb, StatusBar, ModeString, ModeStringPosition, FontMetrics.Scale);
    }
    
    BottomBarsHeightTotal += StatusBar.h;
    LineNumberMargin.h -= BottomBarsHeightTotal;
    irect TextRect = {LineNumberMargin.x + LineNumberMargin.w, Panel.y, Panel.w - LineNumberMargin.w, Panel.h - BottomBarsHeightTotal};
    
    // Computing lines from buffer inside rect
    {
        buffer<line> *Lines = &Buffer->Lines;
        buffer<u8> *Text = &Buffer->Text;
        clear(Lines);
        if(Text->Count == 0) {
            line Line = {};
            push(Lines, Line);
        } else {
            line *Line = Lines->Ptr + add(Lines, 1);
            u8 n = 0; 
            s32 x = TextRect.x;
            
            glyph_key_data GlyphKeyData = {};
            GlyphKeyData.Scale = FontMetrics.Scale;
            
            for(u64 Offset = 0; Offset < Text->Count; Offset += n) {
                line_ending_type LineEnding = is_line_ending(make_view(Text->Ptr + Offset, Text->Count - Offset));
                if(LineEnding) {
                    Line->LineEnding = LineEnding;
                    
                    // Push next line and init it
                    Line = &Lines->Ptr[add(Lines, 1)];
                    u8 LineEndingSize = line_ending_size(LineEnding);
                    Line->Offset = Offset + LineEndingSize; // Next line
                    
                    n = LineEndingSize;
                    x = TextRect.x;
                    continue;
                }
                
                n = utf8_to_codepoint(Text->Ptr + Offset, &GlyphKeyData.Codepoint);
                glyph_info *Glyph = glyph_cache_get(&gCtx.GlyphCache, GlyphKeyData);
                s32 GlyphAdvance = (s32)(Glyph->Scale * Glyph->Advance + 0.5f);
                s32 LeftBearing = (s32)(Glyph->LeftSideBearing * Glyph->Scale + 0.5f);
                
                b32 NeedWrap = (x + GlyphAdvance) > (TextRect.x + TextRect.w);
                b32 IsFirstChar = Offset != Line->Offset; // We don't want to wrap the first character on the line
                if(NeedWrap && IsFirstChar) {
                    // Push next line and init it but dont character at current offset. 
                    // It needs to go into the next line since it does not fit on the current one
                    Line = &Lines->Ptr[add(Lines, 1)];
                    Line->Offset = Offset; // Next line starts on the current character which does not fit
                    Line->Wrapped = true;
                    n = 0; 
                    x = TextRect.x;
                } else {
                    Line->Size += n;
                    Line->Length += 1;
                    
                    x += GlyphAdvance;
                }
            }
        }
        
        // Draw lines
        s32 Baseline = FontMetrics.Ascent + gCtx.Scroll.y;
        for(u64 LineIdx = 0, LineNumber = 1; LineIdx < Lines->Count; ++LineIdx) {
            line *Line = &Lines->Ptr[LineIdx];
            u8 CharByteCount = 1;
            
            if(Baseline < gCtx.Scroll.y) { continue; }
            if(Baseline > (Fb->Height + FontMetrics.VerticalAdvance)) { break; }
            
            if(!Line->Wrapped) {
                // Draw line number
                u8 NumStr[64];
                u64 n = 0;
                u8 *c = NumStr + sizeof(NumStr);
                for(u64 i = LineNumber; i > 0; i /= 10, n++) {
                    --c;
                    *c = '0' + (i % 10);
                }
                view<u8> v = {c, n};
                s32 LineX = 0;
                draw_text(Fb, LineNumberMargin, v, {LineX, Baseline}, FontMetrics.Scale); 
            }
            if(Line->LineEnding) { ++LineNumber; }
            
            draw_text(Fb, TextRect, {Buffer->Text.Ptr + Line->Offset, Line->Size}, {TextRect.x, Baseline}, FontMetrics.Scale);
            Baseline += FontMetrics.VerticalAdvance;
        }
        
        if(Buffer->Mode == EDIT_MODE_Select && Buffer->SelectTerm.Count > 0) {
            draw_selections(Fb, make_view(Buffer->Text), make_view(Buffer->Lines), make_view(Buffer->SelectSelections), TextRect, FontMetrics);
        } else {
            draw_selections(Fb, make_view(Buffer->Text), make_view(Buffer->Lines), make_view(Buffer->Selections), TextRect, FontMetrics);
        }
    }
}
