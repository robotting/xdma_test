/**
 * @file dma_session.h
 * @brief 单个 DMA 操作窗口的状态（可创建多个独立实例）
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

namespace xdma_app {

/** 一个可停靠的 DMA 传输窗口的独立配置与缓冲区 */
struct DmaSession {
    int id = 0;
    char title[64]{};       ///< ImGui 窗口标题，如 "DMA #1"
    bool open = true;       ///< false 时下一帧移除该窗口

    int channel = 0;
    char offset_hex[32] = "0";
    int transfer_size = 4096;
    int pattern = 0;  ///< 0=递增 1=随机 2=全0 3=0xAA

    /** 持续传输：设备打开后按间隔自动 H2C 写 / C2H 读 */
    bool stream_h2c = false;
    bool stream_c2h = false;
    int stream_interval_ms = 50;
    bool stream_advance_offset = false;
    uint64_t stream_h2c_count = 0;
    uint64_t stream_c2h_count = 0;

    std::vector<uint8_t> tx_buf;
    std::vector<uint8_t> rx_buf;
    double last_h2c_mbps = 0.0;
    double last_c2h_mbps = 0.0;

    int prepared_channel = -1;  ///< 已预打开 H2C/C2H 的通道号
    std::chrono::steady_clock::time_point last_stream_h2c{};
    std::chrono::steady_clock::time_point last_stream_c2h{};
};

}  // namespace xdma_app
