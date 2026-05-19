#include "ui/panels.h"

#include "app/app_state.h"

#include <imgui.h>

#include <algorithm>

namespace xdma_app {

void draw_user_panel(AppState& app) {
    ImGui::InputText("Offset (hex)", app.user_offset_hex, sizeof(app.user_offset_hex));
    ImGui::InputInt("Size (bytes)", &app.user_transfer_size);
    app.user_transfer_size = std::clamp(app.user_transfer_size, 1, 16 * 1024 * 1024);

    const char* patterns[] = {"Increment", "Random", "Zeros", "0xAA"};
    ImGui::Combo("Write pattern", &app.user_pattern, patterns, IM_ARRAYSIZE(patterns));

    if (ImGui::Button("USER Read")) app.user_read_write(false);
    ImGui::SameLine();
    if (ImGui::Button("USER Write")) app.user_read_write(true);
}

}  // namespace xdma_app
