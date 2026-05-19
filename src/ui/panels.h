#pragma once

namespace xdma_app {

class AppState;

void draw_device_panel(AppState& app);
struct DmaSession;
void draw_dma_session_panel(AppState& app, DmaSession& session);
void draw_user_panel(AppState& app);
void draw_ip_info_panel(AppState& app);
void draw_rw_panel(AppState& app);
void draw_auto_test_panel(AppState& app);
void draw_log_panel(AppState& app);

}  // namespace xdma_app
