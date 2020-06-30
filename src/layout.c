irect_t
line_number_rect(panel_t *Panel, font_t *Font, irect_t PanelRect) {
    s64 LineCount = sb_count(Panel->Buffer->Lines);
    s32 n = 0;
    while(LineCount > 0) {
        n += 1;
        LineCount /= 10;
    }
    
    return (irect_t){
        .x = PanelRect.x + BORDER_SIZE, 
        .y = PanelRect.y + BORDER_SIZE, 
        .w = Font->MWidth * n,
        .h = PanelRect.h - BORDER_SIZE * 2
    };
}

irect_t
status_bar_rect(irect_t PanelRect) {
    irect_t Rect = {
        .x = PanelRect.x + BORDER_SIZE, 
        .y = PanelRect.y + PanelRect.h - STATUS_BAR_HEIGHT - BORDER_SIZE,
        .w = PanelRect.w - BORDER_SIZE * 2,
        .h = STATUS_BAR_HEIGHT
    };
    
    return Rect;
}

irect_t
text_buffer_rect(panel_t *Panel, font_t *Font, irect_t PanelRect) {
    irect_t s = status_bar_rect(PanelRect);
    irect_t l = line_number_rect(Panel, Font, PanelRect);
    
    irect_t Rect = {
        .x = PanelRect.x + BORDER_SIZE + l.w + LINE_NUMBER_PADDING_RIGHT,
        .y = PanelRect.y + BORDER_SIZE,
        .w = PanelRect.w - l.w - BORDER_SIZE * 2 - LINE_NUMBER_PADDING_RIGHT,
        .h = PanelRect.h - s.h - BORDER_SIZE * 2
    };
    return Rect;
}

void
panel_border_rects(irect_t PanelRect, irect_t *r0, irect_t *r1, irect_t *r2, irect_t *r3) {
    *r0 = (irect_t){PanelRect.x, PanelRect.y, BORDER_SIZE, PanelRect.h};
    *r1 = (irect_t){PanelRect.x + PanelRect.w - BORDER_SIZE, PanelRect.y, BORDER_SIZE, PanelRect.h};
    *r2 = (irect_t){PanelRect.x, PanelRect.y, PanelRect.w, BORDER_SIZE};
    *r3 = (irect_t){PanelRect.x, PanelRect.y + PanelRect.h - BORDER_SIZE, PanelRect.w, BORDER_SIZE};
}

#if 0
s32 
line_height(font_t *Font) {
    s32 LineHeight = Font->Ascent - Font->Descent + Font->LineGap; 
    return LineHeight;
}

stbtt_bakedchar *
get_stbtt_bakedchar(font_t *Font, u32 Codepoint) {
    glyph_set_t *Set;
    if(get_glyph_set(Font, Codepoint, &Set)) {
        return &Set->Glyphs[Codepoint % 256];
    }
    return 0;
}

s32
text_width(font_t *Font, u8 *Start, u8 *End) {
    if(!End) {
        End = Start;
        while(*End) { ++End; }
    }
    
    s32 Result = 0;
    u8 *c = Start;
    u32 Codepoint;
    while(c < End) {
        c = utf8_to_codepoint(c, &Codepoint);
        if(*c == '\n') {
            Result += Font->MWidth;
        } else {
            Result += get_x_advance(Font, Codepoint);
        }
    }
    
    return Result;
}

// Returns {x, baseline}
iv2_t 
center_text_in_rect(font_t *Font, irect_t Rect, u8 *Start, u8 *End) {
    if(!End) {
        End = Start;
        while(*End) { ++End; }
    }
    
    s32 TextWidth = text_width(Font, Start, End);
    iv2_t Result;
    Result.x = Rect.x + (Rect.w - TextWidth) / 2;
    Result.y = Rect.y + (Rect.h + Font->MHeight) / 2;
    return Result;
}
#endif
