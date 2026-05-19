#include "ui/panels.h"

#include "app/app_state.h"

#include <imgui.h>

namespace xdma_app {

void draw_device_panel(AppState& app) {
    if (ImGui::Button("刷新列表")) app.refresh_devices(true);
    ImGui::SameLine();
    if (ImGui::Button("打开设备")) app.open_device();
    ImGui::SameLine();
    if (ImGui::Button("关闭设备")) app.close_device();
    ImGui::SameLine();
    ImGui::Checkbox("启动时刷新", &app.auto_refresh_on_startup);
    if (!app.devices_enumerated)
        ImGui::TextDisabled("尚未枚举设备");
    else if (app.devices.empty())
        ImGui::TextDisabled("无设备，请手动输入 ID 或点「刷新列表」");

    ImGui::Separator();

    ImGui::Text("设备列表 (%zu)", app.devices.size());
    if (ImGui::BeginTable("dev_list", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          ImVec2(0, 120))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28);
        ImGui::TableSetupColumn("设备 ID");
        ImGui::TableSetupColumn("探测", ImGuiTableColumnFlags_WidthFixed, 56);
        ImGui::TableHeadersRow();

        if (app.devices.empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("-");
            ImGui::TableNextColumn();
            ImGui::TextDisabled("(无设备，使用下方手动 ID)");
            ImGui::TableNextColumn();
        } else {
            for (int i = 0; i < static_cast<int>(app.devices.size()); ++i) {
                const auto& d = app.devices[i];
                ImGui::TableNextRow();
                ImGui::PushID(i);
                ImGui::TableNextColumn();
                char idx[8];
                std::snprintf(idx, sizeof(idx), "%d", i);
                const bool selected = (app.selected_device == i);
                if (ImGui::Selectable(idx, selected, ImGuiSelectableFlags_SpanAllColumns))
                    app.selected_device = i;

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(d.id.c_str());

                ImGui::TableNextColumn();
                const bool ok =
                    (i < static_cast<int>(app.devices_online.size())) && app.devices_online[i];
                if (ok)
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.f), "在线");
                else
                    ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.3f, 1.f), "离线");
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("手动设备 ID", app.manual_device, sizeof(app.manual_device));
#ifdef _WIN32
    ImGui::TextDisabled("节点: \\\\.\\<id>_h2c_0 / _c2h_0 / _user");
#else
    ImGui::TextDisabled("节点: /dev/<id>_h2c_0 / _c2h_0 / _user");
#endif

    ImGui::Separator();

    const bool opened = app.device.is_open();
    ImGui::Text("当前设备: %s", opened ? app.device.device_id().c_str() : "(未打开)");
    ImGui::SameLine();
    ImGui::Text("| 状态:");
    ImGui::SameLine();
    if (opened)
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.f), "已打开");
    else
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "已关闭");

    if (opened) {
        if (ImGui::Button("刷新通道")) app.refresh_channels();
        ImGui::SameLine();
        if (ImGui::Button("打开全部通道")) {
            const int n = app.device.open_all_present_channels();
            app.refresh_channels();
            app.add_log("[OK] Opened " + std::to_string(n) + " channel(s)");
        }
        ImGui::SameLine();
        if (ImGui::Button("关闭全部通道")) {
            const int n = app.device.close_all_open_channels();
            app.refresh_channels();
            app.add_log("[OK] Closed " + std::to_string(n) + " channel(s)");
        }

        ImGui::Text("通道管理");
        if (ImGui::BeginTable("ch_list", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 0))) {
            ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 64);
            ImGui::TableSetupColumn("路径");
            ImGui::TableSetupColumn("存在", ImGuiTableColumnFlags_WidthFixed, 44);
            ImGui::TableSetupColumn("句柄", ImGuiTableColumnFlags_WidthFixed, 44);
            ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 72);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < app.channels.size(); ++i) {
                const auto& ch = app.channels[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ch.name.c_str());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ch.path.c_str());

                ImGui::TableNextColumn();
                if (ch.presence == xdma::ChannelPresence::Present)
                    ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.4f, 1.f), "是");
                else
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "否");

                ImGui::TableNextColumn();
                if (ch.is_open)
                    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.f, 1.f), "开");
                else
                    ImGui::TextDisabled("关");

                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(i));
                const bool can_toggle =
                    ch.presence == xdma::ChannelPresence::Present || ch.is_open;
                if (!can_toggle) ImGui::BeginDisabled();
                if (ImGui::SmallButton(ch.is_open ? "关闭" : "打开")) app.toggle_channel(ch);
                if (!can_toggle) ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::TextDisabled("打开设备后可管理 H2C/C2H/User/Bypass 通道");
    }
}

}  // namespace xdma_app
