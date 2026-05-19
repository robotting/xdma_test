#include "ui/fonts.h"

#include <imgui.h>

namespace xdma_app {

void setup_imgui_fonts(ImGuiIO& io) {
    ImFontConfig cfg;
    cfg.OversampleH = cfg.OversampleV = 2;
    cfg.PixelSnapH = true;
    const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();

#ifdef _WIN32
    const char* font_paths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyhbd.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",
    };
#else
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    };
#endif

    for (const char* path : font_paths) {
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, 18.0f, &cfg, ranges)) {
            io.FontDefault = font;
            return;
        }
    }
    io.FontDefault = io.Fonts->AddFontDefault();
}

}  // namespace xdma_app
