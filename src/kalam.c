#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

#include "intrinsics.h"
#include "types.h"
#include "event.h"
#include "platform.h"
#include "kalam.h"

#include "render.c"
#include "font.c"

ctx_t Ctx = {0};

void
do_char(u8 *Char) {
    int n;
    switch(*Char & 0xf0) {
        case 0xf0: { n = 4; } break;
        case 0xe0: { n = 3; } break;
        case 0xd0:
        case 0xc0: { n = 2; } break;
        default:   { n = 1; } break;
    }
    
    // TODO: tab, backspace 
    text_buffer_t *b = &Ctx.Buffer;
    if((b->Size + n) > b->Capacity) {
        u64 NewCapacity = b->Capacity + KILOBYTES(4);
        b->Data = realloc(b->Data, NewCapacity);
        b->Capacity = NewCapacity;
    }
    
    // Move all bytes after cursor forward n bytes
    for(s64 i = b->Size - 1; i >= b->Cursor.ByteOff; i--) {
        b->Data[i + n] = b->Data[i];
    }
    
    for(u8 i = 0; i < n; i++) {
        b->Data[b->Cursor.ByteOff++] = Char[i];
    }
    b->Cursor.CharOff += 1;
    b->Size += n;
}


void
k_init(platform_shared_t *Shared) {
    Ctx.Font = load_ttf("fonts/calibri.ttf", 64);
    
}

void
k_do_editor(platform_shared_t *Shared) {
    {
        input_event_buffer_t *Events = &Shared->EventBuffer;
        for(s32 i = 0; i < Events->Count; ++i) {
            input_event_t *e = &Events->Events[i];
            if(e->Type == INPUT_EVENT_Press && e->Device == INPUT_DEVICE_Keyboard) {
                if(e->Key.HasCharacterTranslation) {
                    do_char(e->Key.Character);
                }
            } else if(e->Type == INPUT_EVENT_Scroll) {
                
            }
        }
        
        Events->Count = 0;
    }
    
    Ctx.p = (ivec2_t){100, 100};
    {
        ivec2_t p = Ctx.p;
        u8 *End = Ctx.Buffer.Data + Ctx.Buffer.Size;
        u8 *c = Ctx.Buffer.Data;
        while(c < End) {
            u32 Codepoint;
            c = utf8_to_codepoint(c, &Codepoint);
            glyph_set_t *Set;
            if(get_glyph_set(&Ctx.Font, Codepoint, &Set)) {
                stbtt_bakedchar *g = &Set->Glyphs[Codepoint & 0xff];
                irect_t Rect = {.x = g->x0, .y = g->y0, .w = g->x1 - g->x0, .h = g->y1 - g->y0};
                s32 y = p.y + (s32)g->yoff + Ctx.Font.MHeight / 2;
                
                
                draw_rect_bitmap(Shared->Framebuffer, p.x + (s32)g->xoff, y, Rect, &Set->Bitmap);
                p.x += (s32)(g->xadvance + 0.5);
            }
        }
    }
    
    {
        s32 x = 0;
        s32 y = 400;
        for(u32 i = 0; i < Ctx.Font.SetCount; ++i) {
            bitmap_t *b = &Ctx.Font.GlyphSets[i].Set->Bitmap;
            irect_t Rect = {0, 0, b->w, b->h};
            draw_rect_bitmap(Shared->Framebuffer, x, y, Rect, b);
            x += b->w + 10;
        }
    }
    
}