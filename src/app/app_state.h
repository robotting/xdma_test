/**
 * @file app_state.h
 * @brief XDMA 测试应用状态与设备/DMA 业务逻辑
 */

#pragma once

#include "app/dma_session.h"

#include <xdma/xdma_device.h>
#include <xdma/xdma_info.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace xdma_app {

enum class RwNode { Control, User, H2C, C2H };

class AppState {
public:
    AppState();

    xdma::Device device;
    std::vector<xdma::DeviceInfo> devices;
    std::vector<bool> devices_online;
    int selected_device = 0;
    char manual_device[64] = "xdma0";

    /** 可动态增删的 DMA 操作窗口列表 */
    std::vector<DmaSession> dma_sessions;
    int next_dma_session_id = 1;

    /** User BAR 使用独立偏移/长度（不绑定某个 DMA 窗口） */
    char user_offset_hex[32] = "0";
    int user_transfer_size = 4096;
    int user_pattern = 0;

    std::deque<std::string> log;

    bool auto_refresh_on_startup = true;
    bool devices_enumerated = false;
    bool logged_no_devices_ = false;

    bool dock_layout_built = false;
    std::vector<xdma::ChannelDesc> channels;

    std::string ip_info_text;
    bool ip_axi_st = false;

    RwNode rw_node = RwNode::Control;
    int rw_channel = 0;
    char rw_offset_hex[32] = "0";
    int rw_length = 256;
    char rw_write_hex[256] = "00 01 02 03";
    std::vector<uint8_t> rw_buf;
    bool rw_binary = false;

    int auto_test_size = 4096;
    xdma::AutoTestReport auto_report;

    void add_log(const std::string& msg);

    uint64_t parse_hex(const char* s) const;

    DmaSession& add_dma_session(const char* name = nullptr);
    DmaSession* find_dma_session(int id);
    void remove_dma_session(int id);
    void duplicate_dma_session(int id);

    void refresh_devices(bool manual = false);
    void refresh_channels();
    void refresh_ip_info();

    std::string resolve_device_id() const;
    void open_device();
    void close_device();
    void toggle_channel(const xdma::ChannelDesc& ch);

    bool rw_transfer(bool write);
    void run_auto_test();

    /** 打开设备后预打开全部 H2C/C2H，便于随时读写 */
    void open_all_dma_channels();
    /** 为会话打开当前 channel 的 H2C + C2H */
    bool ensure_dma_channels(DmaSession& session, bool quiet = false);
    /** 主循环每帧调用：执行勾选了的持续读写 */
    void tick_dma_sessions();

    void h2c_write(DmaSession& session, bool quiet = false);
    void c2h_read(DmaSession& session, bool quiet = false);
    void user_read_write(bool write);
    void benchmark(DmaSession& session, bool h2c);

    void fill_pattern(DmaSession& session, std::vector<uint8_t>& buf);
    void fill_user_pattern(std::vector<uint8_t>& buf);
    uint64_t session_offset(const DmaSession& session) const;
    uint64_t parse_rw_offset() const { return parse_hex(rw_offset_hex); }
    uint64_t user_offset() const { return parse_hex(user_offset_hex); }

private:
    void advance_session_offset(DmaSession& session);
    bool ensure_rw_channel();
    void merge_logical_device_ids();
};

}  // namespace xdma_app
