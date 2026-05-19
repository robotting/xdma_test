#include "ui/panels.h"

#include "app/app_state.h"

#include <imgui.h>

#include <algorithm>

namespace xdma_app {

void draw_auto_test_panel(AppState& app) {
    ImGui::TextDisabled("参考官方 xdma_test：各通道 4KB 回环校验 (示例 BRAM 设计)");
    ImGui::InputInt("传输大小 (B)", &app.auto_test_size);
    app.auto_test_size = std::clamp(app.auto_test_size, 4, 16 * 1024 * 1024);
    if (ImGui::Button("运行自动测试")) app.run_auto_test();

    if (!app.auto_report.summary.empty()) {
        ImGui::Separator();
        const ImVec4 col = app.auto_report.ok ? ImVec4(0.3f, 0.9f, 0.4f, 1.f)
                                              : ImVec4(0.95f, 0.35f, 0.3f, 1.f);
        ImGui::TextColored(col, "%s", app.auto_report.summary.c_str());
        ImGui::Text("模式: %s", app.auto_report.axi_streaming ? "AXI-ST (并行 H2C/C2H)"
                                                             : "AXI-MM (顺序 H2C→C2H)");
        if (ImGui::BeginTable("auto_ch", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("通道");
            ImGui::TableSetupColumn("存在");
            ImGui::TableSetupColumn("结果");
            ImGui::TableSetupColumn("说明");
            ImGui::TableHeadersRow();
            for (const auto& ch : app.auto_report.channels) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("ch%d", ch.channel);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ch.present ? "是" : "否");
                ImGui::TableNextColumn();
                if (!ch.present)
                    ImGui::TextDisabled("-");
                else if (ch.passed)
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.f), "PASS");
                else
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.3f, 1.f), "FAIL");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ch.detail.c_str());
            }
            ImGui::EndTable();
        }
    }
}

}  // namespace xdma_app
