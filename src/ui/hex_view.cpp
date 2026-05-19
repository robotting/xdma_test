#include "ui/hex_view.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace xdma_app {

void draw_hex_dump(const std::vector<uint8_t>& data, uint64_t base_addr, size_t max_rows) {
    if (data.empty()) {
        ImGui::TextDisabled("(empty)");
        return;
    }
    const size_t row = 16;
    const size_t total_lines = (data.size() + row - 1) / row;
    const size_t lines = max_rows > 0 ? std::min(max_rows, total_lines) : total_lines;
    for (size_t r = 0; r < lines; ++r) {
        char line[192]{};
        int pos = 0;
        const uint64_t addr = base_addr + r * row;
        pos += std::snprintf(line + pos, sizeof(line) - pos, "0x%04llX:  ",
                             static_cast<unsigned long long>(addr));
        const size_t cols = std::min(row, data.size() - r * row);
        for (size_t c = 0; c < cols; ++c)
            pos += std::snprintf(line + pos, sizeof(line) - pos, "%02x ", data[r * row + c]);
        for (size_t c = cols; c < row; ++c) pos += std::snprintf(line + pos, sizeof(line) - pos, "   ");
        pos += std::snprintf(line + pos, sizeof(line) - pos, "   ");
        for (size_t c = 0; c < cols; ++c) {
            const char ch = static_cast<char>(data[r * row + c]);
            pos += std::snprintf(line + pos, sizeof(line) - pos, "%c",
                                 (ch >= 32 && ch != 127) ? ch : '.');
        }
        ImGui::TextUnformatted(line);
    }
    if (max_rows > 0 && data.size() > lines * row)
        ImGui::TextDisabled("... (%zu bytes total)", data.size());
}

void draw_hex_preview(const char* title, const std::vector<uint8_t>& data, size_t max_rows) {
    if (ImGui::TreeNode(title)) {
        draw_hex_dump(data, 0, max_rows);
        ImGui::TreePop();
    }
}

}  // namespace xdma_app
