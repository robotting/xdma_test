#include "ui/dock.h"

#include "app/app_state.h"
#include "app/dma_session.h"
#include "ui/panels.h"
#include "ui/window_titles.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace xdma_app {

void setup_default_dock_layout(AppState& app) {
    ImGuiID dockspace_id = ImGui::GetID("XDMA_DockSpace");
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.22f, nullptr, &dockspace_id);
    ImGuiID dock_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.28f, nullptr, &dockspace_id);
    ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.30f, nullptr, &dockspace_id);

    ImGuiID dock_center = dockspace_id;
    ImGuiID dock_center_bottom =
        ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.45f, nullptr, &dock_center);

    ImGui::DockBuilderDockWindow(windows::kDevice, dock_left);
    ImGui::DockBuilderDockWindow(windows::kIpInfo, dock_left);
    ImGui::DockBuilderDockWindow(windows::kLog, dock_bottom);
    if (!app.dma_sessions.empty())
        ImGui::DockBuilderDockWindow(app.dma_sessions.front().title, dock_right);
    ImGui::DockBuilderDockWindow(windows::kRw, dock_center);
    ImGui::DockBuilderDockWindow(windows::kAutoTest, dock_center_bottom);
    ImGui::DockBuilderDockWindow(windows::kUserBar, dock_center_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
}

void draw_dockspace_host(AppState& app) {
    static bool show_about = false;

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGuiWindowFlags host_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    host_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove;
    host_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("XDMA_DockHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("DMA")) {
            if (ImGui::MenuItem("新建 DMA 窗口")) app.add_dma_session();
            ImGui::Separator();
            for (const auto& session : app.dma_sessions)
                ImGui::MenuItem(session.title, nullptr, session.open);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset layout")) app.dock_layout_built = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("关于...")) show_about = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (show_about) {
        ImGui::SetNextWindowSize(ImVec2(420, 280), ImGuiCond_FirstUseEver);
        ImGui::Begin("关于 XDMA 测试工具", &show_about);
        ImGui::Text("Xilinx PCIe DMA (XDMA) 测试 GUI");
        ImGui::Separator();
        ImGui::Text("参考 Windows 驱动示例:");
        ImGui::BulletText("xdma_info  -> IP 信息面板");
        ImGui::BulletText("xdma_rw    -> 读写工具面板");
        ImGui::BulletText("xdma_test  -> 自动测试面板");
        ImGui::BulletText("DMA 菜单 -> 可添加多个独立 DMA 操作窗口");
#ifdef _WIN32
        ImGui::Text("请先安装 XDMA Windows 驱动。");
#else
        ImGui::Text("加载 xdma 内核模块; 访问 /dev/xdma*");
#endif
        ImGui::End();
    }

    ImGuiID dockspace_id = ImGui::GetID("XDMA_DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!app.dock_layout_built) {
        setup_default_dock_layout(app);
        app.dock_layout_built = true;
    }

    ImGui::End();
}

void draw_all_panels(AppState& app) {
    ImGui::Begin(windows::kDevice);
    draw_device_panel(app);
    ImGui::End();

    for (size_t i = 0; i < app.dma_sessions.size();) {
        DmaSession& session = app.dma_sessions[i];
        if (ImGui::Begin(session.title, &session.open)) {
            if (ImGui::Button("复制窗口")) app.duplicate_dma_session(session.id);
            draw_dma_session_panel(app, session);
        }
        ImGui::End();
        if (!session.open) {
            app.remove_dma_session(session.id);
            if (app.dma_sessions.empty()) app.add_dma_session();
            continue;
        }
        ++i;
    }

    ImGui::Begin(windows::kIpInfo);
    draw_ip_info_panel(app);
    ImGui::End();

    ImGui::Begin(windows::kRw);
    draw_rw_panel(app);
    ImGui::End();

    ImGui::Begin(windows::kAutoTest);
    draw_auto_test_panel(app);
    ImGui::End();

    ImGui::Begin(windows::kUserBar);
    draw_user_panel(app);
    ImGui::End();

    ImGui::Begin(windows::kLog);
    draw_log_panel(app);
    ImGui::End();
}

}  // namespace xdma_app
