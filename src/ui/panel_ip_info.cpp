#include "ui/panels.h"

#include "app/app_state.h"

#include <imgui.h>

namespace xdma_app {

void draw_ip_info_panel(AppState& app) {
    ImGui::TextDisabled("参考官方 xdma_info：读取 control 寄存器");
    if (ImGui::Button("刷新 IP 信息")) app.refresh_ip_info();
    ImGui::SameLine();
    if (app.device.is_open()) {
        ImGui::Text("接口: %s", app.ip_axi_st ? "AXI-ST (流式)" : "AXI-MM (存储映射)");
    } else {
        ImGui::TextDisabled("未打开设备");
    }
    ImGui::BeginChild("ip_info_scroll", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (app.ip_info_text.empty())
        ImGui::TextDisabled("打开设备后点击「刷新 IP 信息」");
    else
        ImGui::TextUnformatted(app.ip_info_text.c_str());
    ImGui::EndChild();
}

}  // namespace xdma_app
