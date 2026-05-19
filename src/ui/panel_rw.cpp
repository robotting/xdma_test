#include "ui/panels.h"

#include "app/app_state.h"
#include "ui/hex_view.h"

#include <imgui.h>

#include <algorithm>

namespace xdma_app {

void draw_rw_panel(AppState& app) {
    ImGui::TextDisabled("参考官方 xdma_rw：control | user | h2c_* | c2h_*");
    const char* nodes[] = {"control", "user", "h2c", "c2h"};
    int node_idx = static_cast<int>(app.rw_node);
    ImGui::Combo("节点", &node_idx, nodes, IM_ARRAYSIZE(nodes));
    app.rw_node = static_cast<RwNode>(node_idx);
    if (app.rw_node == RwNode::H2C || app.rw_node == RwNode::C2H)
        ImGui::SliderInt("通道", &app.rw_channel, 0, 3);
    ImGui::InputText("偏移 (hex)", app.rw_offset_hex, sizeof(app.rw_offset_hex));
    ImGui::InputInt("长度 (-l)", &app.rw_length);
    app.rw_length = std::clamp(app.rw_length, 1, 16 * 1024 * 1024);
    ImGui::InputTextMultiline("写入数据 (hex/dec 字节)", app.rw_write_hex, sizeof(app.rw_write_hex),
                              ImVec2(-1, 48));
    if (ImGui::Button("读")) app.rw_transfer(false);
    ImGui::SameLine();
    if (ImGui::Button("写")) app.rw_transfer(true);
    ImGui::Separator();
    ImGui::Text("读回数据 (%zu B)", app.rw_buf.size());
    ImGui::BeginChild("rw_hex", ImVec2(0, 0), ImGuiChildFlags_Borders);
    draw_hex_dump(app.rw_buf, app.parse_rw_offset());
    ImGui::EndChild();
}

}  // namespace xdma_app
