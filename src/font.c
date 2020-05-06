#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

#include "intrinsics.h"
#include "platform.h"


void
f_load_ttf(char *Path) {
    stbtt_fontinfo FontInfo = {0};
    platform_file_data_t FontFileData;
    ASSERT(platform_read_file(Path, &FontFileData));
    
    int Status = stbtt_InitFont(&FontInfo, FontFileData.Data, 0);
    
#if 0
    stbtt_GetFontVMetrics();
    stbtt_ScaleForMappingEmToPixels();
#endif
}