#include "ui/panels.h"

#include "app/app_state.h"
#include "ui/hex_view.h"

#include <imgui.h>
#include <xdma/xdma_device.h>

#include <algorithm>
#include <cstdio>

namespace xdma_app {

void draw_dma_session_panel(AppState& app, DmaSession& session) {
    if (session.channel != session.prepared_channel) app.ensure_dma_channels(session, true);

    const bool dev_open = app.device.is_open();
    if (dev_open) {
        const bool h2c_ok =
            app.device.is_channel_open(xdma::ChannelKind::H2C, session.channel);
        const bool c2h_ok =
            app.device.is_channel_open(xdma::ChannelKind::C2H, session.channel);
        ImGui::TextColored(h2c_ok ? ImVec4(0.3f, 0.9f, 0.4f, 1.f) : ImVec4(0.9f, 0.5f, 0.3f, 1.f),
                           "H2C %s", h2c_ok ? "就绪" : "未开");
        ImGui::SameLine();
        ImGui::TextColored(c2h_ok ? ImVec4(0.3f, 0.9f, 0.4f, 1.f) : ImVec4(0.9f, 0.5f, 0.3f, 1.f),
                           "C2H %s", c2h_ok ? "就绪" : "未开");
    } else {
        ImGui::TextDisabled("请先打开设备");
    }

    ImGui::TextDisabled("窗口 #%d", session.id);
    ImGui::SliderInt("Channel", &session.channel, 0, 3);
    ImGui::InputText("Offset (hex)", session.offset_hex, sizeof(session.offset_hex));
    ImGui::InputInt("Size (bytes)", &session.transfer_size);
    session.transfer_size = std::clamp(session.transfer_size, 1, 16 * 1024 * 1024);

    const char* patterns[] = {"Increment", "Random", "Zeros", "0xAA"};
    ImGui::Combo("TX pattern", &session.pattern, patterns, IM_ARRAYSIZE(patterns));

    ImGui::Separator();
    ImGui::TextUnformatted("单次传输");
    const bool busy = session.stream_h2c || session.stream_c2h;
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("H2C Write")) app.h2c_write(session);
    ImGui::SameLine();
    if (ImGui::Button("C2H Read")) app.c2h_read(session);
    ImGui::SameLine();
    if (ImGui::Button("Bench H2C")) app.benchmark(session, true);
    ImGui::SameLine();
    if (ImGui::Button("Bench C2H")) app.benchmark(session, false);
    if (busy) ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextUnformatted("持续读写 (设备打开后一直可用)");
    if (ImGui::Checkbox("持续 H2C 写", &session.stream_h2c)) {
        if (session.stream_h2c)
            session.last_stream_h2c = std::chrono::steady_clock::now();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("持续 C2H 读", &session.stream_c2h)) {
        if (session.stream_c2h)
            session.last_stream_c2h = std::chrono::steady_clock::now();
    }
    ImGui::InputInt("间隔 (ms)", &session.stream_interval_ms);
    session.stream_interval_ms = std::clamp(session.stream_interval_ms, 1, 60000);
    ImGui::Checkbox("每次递增偏移", &session.stream_advance_offset);
    if (ImGui::Button("清零计数")) {
        session.stream_h2c_count = 0;
        session.stream_c2h_count = 0;
    }

    ImGui::Text("Last H2C: %.2f MB/s | Last C2H: %.2f MB/s", session.last_h2c_mbps,
                session.last_c2h_mbps);
    ImGui::Text("持续: 写 %llu 次 | 读 %llu 次", session.stream_h2c_count,
                session.stream_c2h_count);

    draw_hex_preview("TX buffer", session.tx_buf);
    draw_hex_preview("RX buffer", session.rx_buf);
}

}  // namespace xdma_app
