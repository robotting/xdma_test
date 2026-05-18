#include "xdma_device.h"

#include <imgui.h>
#include <imgui_internal.h>  // DockBuilder* (docking branch)
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace {

struct AppState {
    xdma::Device device;
    std::vector<xdma::DeviceInfo> devices;
    int selected_device = 0;
    char manual_device[64] = "xdma0";

    int dma_channel = 0;
    char offset_hex[32] = "0";
    int transfer_size = 4096;
    int pattern = 0;  // 0=increment, 1=random, 2=zeros, 3=0xAA

    std::vector<uint8_t> tx_buf;
    std::vector<uint8_t> rx_buf;
    std::deque<std::string> log;
    double last_h2c_mbps = 0.0;
    double last_c2h_mbps = 0.0;
    bool auto_refresh_devices = true;
    bool dock_layout_built = false;

    void add_log(const std::string& msg) {
        log.push_back(msg);
        if (log.size() > 500) log.pop_front();
    }

    uint64_t parse_offset() const {
        const char* s = offset_hex;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
        return std::strtoull(s, nullptr, 16);
    }

    void fill_pattern(std::vector<uint8_t>& buf) {
        buf.resize(static_cast<size_t>(transfer_size));
        switch (pattern) {
            case 0:
                for (size_t i = 0; i < buf.size(); ++i) buf[i] = static_cast<uint8_t>(i & 0xFF);
                break;
            case 1: {
                std::mt19937 rng{std::random_device{}()};
                std::uniform_int_distribution<int> dist(0, 255);
                for (auto& b : buf) b = static_cast<uint8_t>(dist(rng));
                break;
            }
            case 2:
                std::memset(buf.data(), 0, buf.size());
                break;
            case 3:
                std::memset(buf.data(), 0xAA, buf.size());
                break;
        }
    }

    bool ensure_channel(xdma::ChannelKind kind) {
        if (!device.is_open()) {
            add_log("[ERR] Open a device first");
            return false;
        }
        if (!device.open_channel(kind, dma_channel)) {
            add_log("[ERR] " + device.last_error());
            return false;
        }
        return true;
    }

    void refresh_devices() {
        devices = xdma::Device::enumerate();
        if (devices.empty()) {
            add_log("[INFO] No XDMA device found; try manual id (e.g. xdma0)");
        }
    }

    void open_device() {
        std::string id;
        if (!devices.empty() && selected_device >= 0 &&
            selected_device < static_cast<int>(devices.size()))
            id = devices[selected_device].id;
        else
            id = manual_device;

        if (!device.open(id)) {
            add_log("[ERR] open failed");
            return;
        }
        add_log("[OK] Opened device: " + id);
    }

    void h2c_write() {
        if (!ensure_channel(xdma::ChannelKind::H2C)) return;
        fill_pattern(tx_buf);
        const uint64_t off = parse_offset();
        const auto t0 = std::chrono::steady_clock::now();
        if (!device.write_h2c(dma_channel, off, tx_buf.data(), tx_buf.size())) {
            add_log("[ERR] H2C: " + device.last_error());
            return;
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        last_h2c_mbps = (tx_buf.size() / (1024.0 * 1024.0)) / sec;
        add_log("[OK] H2C write " + std::to_string(tx_buf.size()) + " B @ 0x" + offset_hex);
    }

    void c2h_read() {
        if (!ensure_channel(xdma::ChannelKind::C2H)) return;
        rx_buf.resize(static_cast<size_t>(transfer_size));
        const uint64_t off = parse_offset();
        const auto t0 = std::chrono::steady_clock::now();
        if (!device.read_c2h(dma_channel, off, rx_buf.data(), rx_buf.size())) {
            add_log("[ERR] C2H: " + device.last_error());
            return;
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        last_c2h_mbps = (rx_buf.size() / (1024.0 * 1024.0)) / sec;
        add_log("[OK] C2H read " + std::to_string(rx_buf.size()) + " B @ 0x" + offset_hex);
    }

    void user_read_write(bool write) {
        if (!ensure_channel(xdma::ChannelKind::User)) return;
        const uint64_t off = parse_offset();
        if (write) {
            fill_pattern(tx_buf);
            if (!device.write_user(off, tx_buf.data(), tx_buf.size()))
                add_log("[ERR] USER write: " + device.last_error());
            else
                add_log("[OK] USER write " + std::to_string(tx_buf.size()) + " B");
        } else {
            rx_buf.resize(static_cast<size_t>(transfer_size));
            if (!device.read_user(off, rx_buf.data(), rx_buf.size()))
                add_log("[ERR] USER read: " + device.last_error());
            else
                add_log("[OK] USER read " + std::to_string(rx_buf.size()) + " B");
        }
    }

    void benchmark(bool h2c) {
        if (!ensure_channel(h2c ? xdma::ChannelKind::H2C : xdma::ChannelKind::C2H)) return;
        const int iterations = 64;
        std::vector<uint8_t> buf(static_cast<size_t>(transfer_size));
        fill_pattern(buf);
        const uint64_t off = parse_offset();

        size_t total = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            size_t done = 0;
            bool ok = h2c ? device.write_h2c(dma_channel, off, buf.data(), buf.size(), &done)
                          : device.read_c2h(dma_channel, off, buf.data(), buf.size(), &done);
            if (!ok) {
                add_log(std::string("[ERR] bench ") + (h2c ? "H2C" : "C2H") + ": " + device.last_error());
                return;
            }
            total += done;
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        const double mbps = (total / (1024.0 * 1024.0)) / sec;
        if (h2c)
            last_h2c_mbps = mbps;
        else
            last_c2h_mbps = mbps;
        add_log(std::string("[BENCH] ") + (h2c ? "H2C" : "C2H") + " " + std::to_string(mbps) +
                " MB/s (" + std::to_string(iterations) + " x " + std::to_string(transfer_size) +
                " B)");
    }

    void draw_hex_preview(const char* title, const std::vector<uint8_t>& data, size_t max_rows = 8) {
        if (ImGui::TreeNode(title)) {
            const size_t row = 16;
            const size_t lines = std::min(max_rows, (data.size() + row - 1) / row);
            for (size_t l = 0; l < lines; ++l) {
                char line[128]{};
                int pos = 0;
                pos += std::snprintf(line + pos, sizeof(line) - pos, "%04zX: ", l * row);
                for (size_t c = 0; c < row && l * row + c < data.size(); ++c)
                    pos += std::snprintf(line + pos, sizeof(line) - pos, "%02X ",
                                         data[l * row + c]);
                ImGui::TextUnformatted(line);
            }
            if (data.size() > lines * row)
                ImGui::TextDisabled("... (%zu bytes total)", data.size());
            ImGui::TreePop();
        }
    }
};

void draw_device_panel(AppState& app) {
    if (ImGui::Button("Refresh list") || (app.auto_refresh_devices && app.devices.empty()))
        app.refresh_devices();

    ImGui::SameLine();
    ImGui::Checkbox("Auto refresh on start", &app.auto_refresh_devices);

    if (!app.devices.empty()) {
        std::vector<const char*> names;
        names.reserve(app.devices.size());
        for (const auto& d : app.devices) names.push_back(d.id.c_str());
        ImGui::Combo("Detected", &app.selected_device, names.data(),
                     static_cast<int>(names.size()));
    }

    ImGui::InputText("Manual device id", app.manual_device, sizeof(app.manual_device));
#ifdef _WIN32
    ImGui::TextDisabled("Paths: \\\\.\\<id>_h2c_0, _c2h_0, _user");
#else
    ImGui::TextDisabled("Paths: /dev/<id>_h2c_0, _c2h_0, _user");
#endif

    if (ImGui::Button("Open")) app.open_device();
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
        app.device.close();
        app.add_log("[OK] Device closed");
    }

    ImGui::Text("Status: %s", app.device.is_open() ? "OPEN" : "CLOSED");
}

void draw_dma_panel(AppState& app) {
    ImGui::SliderInt("Channel", &app.dma_channel, 0, 3);
    ImGui::InputText("Offset (hex)", app.offset_hex, sizeof(app.offset_hex));
    ImGui::InputInt("Size (bytes)", &app.transfer_size);
    app.transfer_size = std::clamp(app.transfer_size, 1, 16 * 1024 * 1024);

    const char* patterns[] = {"Increment", "Random", "Zeros", "0xAA"};
    ImGui::Combo("TX pattern", &app.pattern, patterns, IM_ARRAYSIZE(patterns));

    if (ImGui::Button("H2C Write")) app.h2c_write();
    ImGui::SameLine();
    if (ImGui::Button("C2H Read")) app.c2h_read();
    ImGui::SameLine();
    if (ImGui::Button("Bench H2C")) app.benchmark(true);
    ImGui::SameLine();
    if (ImGui::Button("Bench C2H")) app.benchmark(false);

    ImGui::Text("Last H2C: %.2f MB/s | Last C2H: %.2f MB/s", app.last_h2c_mbps, app.last_c2h_mbps);

    app.draw_hex_preview("TX buffer", app.tx_buf);
    app.draw_hex_preview("RX buffer", app.rx_buf);
}

void draw_user_panel(AppState& app) {
    if (ImGui::Button("USER Read")) app.user_read_write(false);
    ImGui::SameLine();
    if (ImGui::Button("USER Write")) app.user_read_write(true);
    ImGui::TextDisabled("Uses same offset/size as DMA panel");
}

void draw_log_panel(AppState& app) {
    if (ImGui::Button("Clear log")) app.log.clear();
    ImGui::BeginChild("log_scroll", ImVec2(0, 0), true);
    for (const auto& line : app.log) ImGui::TextUnformatted(line.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

void setup_default_dock_layout() {
    ImGuiID dockspace_id = ImGui::GetID("XDMA_DockSpace");
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.22f, nullptr, &dockspace_id);
    ImGuiID dock_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.28f, nullptr, &dockspace_id);
    ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.30f, nullptr, &dockspace_id);

    ImGui::DockBuilderDockWindow("Device", dock_left);
    ImGui::DockBuilderDockWindow("User BAR", dock_left);
    ImGui::DockBuilderDockWindow("Log", dock_bottom);
    ImGui::DockBuilderDockWindow("DMA Transfer", dock_right);
    ImGui::DockBuilderFinish(dockspace_id);
}

void draw_dockspace_host(AppState& app) {
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
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset layout")) app.dock_layout_built = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::Text("Xilinx PCIe DMA (XDMA) test GUI");
#ifdef _WIN32
            ImGui::Text("Install Xilinx/XDMA Windows driver first.");
#else
            ImGui::Text("Load xdma kernel module; access /dev/xdma* (often needs root).");
#endif
            ImGui::Text("Drag panel title bars to dock/undock.");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGuiID dockspace_id = ImGui::GetID("XDMA_DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!app.dock_layout_built) {
        setup_default_dock_layout();
        app.dock_layout_built = true;
    }

    ImGui::End();
}

void draw_panels(AppState& app) {
    if (ImGui::Begin("Device")) {
        draw_device_panel(app);
        ImGui::End();
    }

    if (ImGui::Begin("DMA Transfer")) {
        draw_dma_panel(app);
        ImGui::End();
    }

    if (ImGui::Begin("User BAR")) {
        draw_user_panel(app);
        ImGui::End();
    }

    if (ImGui::Begin("Log")) {
        draw_log_panel(app);
        ImGui::End();
    }
}

}  // namespace

int main() {
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "XDMA Test Tool", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    AppState app;
    if (app.auto_refresh_devices) app.refresh_devices();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_dockspace_host(app);
        draw_panels(app);

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
