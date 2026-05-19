#include "ui/panels.h"

#include "app/app_state.h"

#include <imgui.h>

namespace xdma_app {

void draw_log_panel(AppState& app) {
    if (ImGui::Button("Clear log")) app.log.clear();
    ImGui::BeginChild("log_scroll", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (const auto& line : app.log) ImGui::TextUnformatted(line.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

}  // namespace xdma_app
