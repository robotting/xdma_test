#include "app/app_state.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <sstream>

namespace xdma_app {

AppState::AppState() {
    DmaSession session;
    session.id = next_dma_session_id++;
    std::snprintf(session.title, sizeof(session.title), "DMA #1");
    dma_sessions.push_back(std::move(session));
}

void AppState::add_log(const std::string& msg) {
    log.push_back(msg);
    if (log.size() > 500) log.pop_front();
}

uint64_t AppState::parse_hex(const char* s) const {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    return std::strtoull(s, nullptr, 16);
}

void AppState::fill_pattern(DmaSession& session, std::vector<uint8_t>& buf) {
    buf.resize(static_cast<size_t>(session.transfer_size));
    switch (session.pattern) {
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

void AppState::fill_user_pattern(std::vector<uint8_t>& buf) {
    buf.resize(static_cast<size_t>(user_transfer_size));
    const int pat = user_pattern;
    switch (pat) {
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

uint64_t AppState::session_offset(const DmaSession& session) const {
    return parse_hex(session.offset_hex);
}

DmaSession& AppState::add_dma_session(const char* name) {
    DmaSession session;
    session.id = next_dma_session_id++;
    session.open = true;
    if (name && name[0]) {
        std::snprintf(session.title, sizeof(session.title), "%s", name);
    } else {
        std::snprintf(session.title, sizeof(session.title), "DMA #%d", session.id);
    }
    dma_sessions.push_back(std::move(session));
    add_log(std::string("[OK] 新建 DMA 窗口: ") + dma_sessions.back().title);
    return dma_sessions.back();
}

DmaSession* AppState::find_dma_session(int id) {
    for (auto& s : dma_sessions) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

void AppState::remove_dma_session(int id) {
    const auto it = std::remove_if(dma_sessions.begin(), dma_sessions.end(),
                                   [id](const DmaSession& s) { return s.id == id; });
    if (it != dma_sessions.end()) {
        add_log("[OK] 关闭 DMA 窗口 #" + std::to_string(id));
        dma_sessions.erase(it, dma_sessions.end());
    }
}

void AppState::duplicate_dma_session(int id) {
    const DmaSession* src = find_dma_session(id);
    if (!src) return;
    DmaSession copy = *src;
    copy.id = next_dma_session_id++;
    copy.open = true;
    std::snprintf(copy.title, sizeof(copy.title), "%s (副本)", src->title);
    dma_sessions.push_back(std::move(copy));
    add_log(std::string("[OK] 复制 DMA 窗口: ") + dma_sessions.back().title);
}

void AppState::tick_dma_sessions() {
    if (!device.is_open()) return;

    const auto now = std::chrono::steady_clock::now();
    const auto interval_for = [](int ms) {
        return std::chrono::milliseconds(std::max(ms, 1));
    };

    for (auto& session : dma_sessions) {
        if (!session.stream_h2c && !session.stream_c2h) continue;
        if (!ensure_dma_channels(session, true)) continue;

        if (session.stream_h2c &&
            now - session.last_stream_h2c >= interval_for(session.stream_interval_ms)) {
            h2c_write(session, true);
            ++session.stream_h2c_count;
            if (session.stream_advance_offset) advance_session_offset(session);
            session.last_stream_h2c = now;
        }
        if (session.stream_c2h &&
            now - session.last_stream_c2h >= interval_for(session.stream_interval_ms)) {
            c2h_read(session, true);
            ++session.stream_c2h_count;
            if (session.stream_advance_offset) advance_session_offset(session);
            session.last_stream_c2h = now;
        }
    }
}

void AppState::merge_logical_device_ids() {
    for (int n = 0; n < 16; ++n) {
        std::ostringstream oss;
        oss << "xdma" << n;
        const std::string id = oss.str();
        const bool known = std::any_of(devices.begin(), devices.end(),
                                       [&](const xdma::DeviceInfo& d) { return d.id == id; });
        if (known) continue;
        const std::string user_path = xdma::Device::path_for(id, xdma::ChannelKind::User, 0);
        if (!xdma::Device::probe_path(user_path)) continue;
        xdma::DeviceInfo info;
        info.id = id;
        info.index = n;
        devices.push_back(std::move(info));
    }
}

void AppState::refresh_devices(bool manual) {
    devices = xdma::Device::enumerate();
    merge_logical_device_ids();

    devices_online.resize(devices.size());
    for (size_t i = 0; i < devices.size(); ++i) {
        const std::string user_path =
            xdma::Device::path_for(devices[i].id, xdma::ChannelKind::User, 0);
        devices_online[i] = xdma::Device::probe_path(user_path);
    }

    devices_enumerated = true;

    if (manual) {
        if (devices.empty()) {
            add_log("[INFO] 刷新：未发现 XDMA 设备（可手动输入设备 ID 后打开）");
        } else {
            const int online =
                static_cast<int>(std::count(devices_online.begin(), devices_online.end(), true));
            add_log("[INFO] 刷新：发现 " + std::to_string(devices.size()) + " 个设备，" +
                    std::to_string(online) + " 个在线");
        }
        logged_no_devices_ = devices.empty();
    } else if (devices.empty()) {
        if (!logged_no_devices_) {
            add_log("[INFO] 未发现 XDMA 设备；请点「刷新列表」或手动输入设备 ID");
            logged_no_devices_ = true;
        }
    } else {
        logged_no_devices_ = false;
    }

    if (selected_device >= static_cast<int>(devices.size()))
        selected_device = devices.empty() ? 0 : static_cast<int>(devices.size()) - 1;
}

std::string AppState::resolve_device_id() const {
    if (!devices.empty() && selected_device >= 0 &&
        selected_device < static_cast<int>(devices.size()))
        return devices[selected_device].id;
    return manual_device;
}

void AppState::refresh_channels() { channels = device.list_channels(); }

void AppState::open_device() {
    const std::string id = resolve_device_id();
    if (!device.open(id)) {
        add_log("[ERR] open failed");
        return;
    }
    open_all_dma_channels();
    for (auto& s : dma_sessions) s.prepared_channel = -1;
    refresh_channels();
    refresh_ip_info();
    add_log("[OK] Opened device: " + id + " (H2C/C2H 通道已就绪，可随时读写)");
}

void AppState::open_all_dma_channels() {
    if (!device.is_open()) return;
    for (int ch = 0; ch < 4; ++ch) {
        device.open_channel(xdma::ChannelKind::H2C, ch);
        device.open_channel(xdma::ChannelKind::C2H, ch);
    }
}

bool AppState::ensure_dma_channels(DmaSession& session, bool quiet) {
    if (!device.is_open()) {
        if (!quiet) add_log("[ERR] 请先打开设备");
        return false;
    }
    const bool ok_h2c = device.open_channel(xdma::ChannelKind::H2C, session.channel);
    const bool ok_c2h = device.open_channel(xdma::ChannelKind::C2H, session.channel);
    if (!ok_h2c || !ok_c2h) {
        if (!quiet) add_log("[ERR] " + device.last_error());
        return false;
    }
    session.prepared_channel = session.channel;
    return true;
}

void AppState::advance_session_offset(DmaSession& session) {
    const uint64_t off = session_offset(session) +
                         static_cast<uint64_t>(session.transfer_size);
    std::snprintf(session.offset_hex, sizeof(session.offset_hex), "0x%llX",
                  static_cast<unsigned long long>(off));
}

void AppState::refresh_ip_info() {
    ip_info_text.clear();
    ip_axi_st = false;
    if (!device.is_open()) return;
    const auto rep = xdma::query_ip_info(device);
    if (rep.ok) {
        ip_info_text = rep.text;
        device.is_axi_streaming(&ip_axi_st);
    } else {
        ip_info_text = rep.error.empty() ? "Failed to read IP info" : rep.error;
    }
}

bool AppState::ensure_rw_channel() {
    if (!device.is_open()) {
        add_log("[ERR] Open device first");
        return false;
    }
    xdma::ChannelKind kind = xdma::ChannelKind::User;
    int ch = 0;
    switch (rw_node) {
        case RwNode::Control: kind = xdma::ChannelKind::Control; break;
        case RwNode::User: kind = xdma::ChannelKind::User; break;
        case RwNode::H2C: kind = xdma::ChannelKind::H2C; ch = rw_channel; break;
        case RwNode::C2H: kind = xdma::ChannelKind::C2H; ch = rw_channel; break;
    }
    if (!device.open_channel(kind, ch)) {
        add_log("[ERR] " + device.last_error());
        return false;
    }
    return true;
}

bool AppState::rw_transfer(bool write) {
    if (!ensure_rw_channel()) return false;
    const uint64_t off = parse_rw_offset();
    const size_t len = static_cast<size_t>(std::clamp(rw_length, 1, 16 * 1024 * 1024));

    xdma::ChannelKind kind = xdma::ChannelKind::User;
    int ch = 0;
    switch (rw_node) {
        case RwNode::Control: kind = xdma::ChannelKind::Control; break;
        case RwNode::User: kind = xdma::ChannelKind::User; break;
        case RwNode::H2C: kind = xdma::ChannelKind::H2C; ch = rw_channel; break;
        case RwNode::C2H: kind = xdma::ChannelKind::C2H; ch = rw_channel; break;
    }

    if (write) {
        rw_buf.clear();
        std::istringstream iss(rw_write_hex);
        std::string tok;
        while (iss >> tok) {
            const uint32_t v = static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 0));
            rw_buf.push_back(static_cast<uint8_t>(v & 0xFF));
        }
        if (rw_buf.empty()) {
            rw_buf.resize(len);
            for (size_t i = 0; i < rw_buf.size(); ++i) rw_buf[i] = static_cast<uint8_t>(i & 0xFF);
        }
        bool ok = false;
        switch (kind) {
            case xdma::ChannelKind::Control:
                ok = device.write_control(off, rw_buf.data(), rw_buf.size());
                break;
            case xdma::ChannelKind::User:
                ok = device.write_user(off, rw_buf.data(), rw_buf.size());
                break;
            case xdma::ChannelKind::H2C:
                ok = device.write_h2c(ch, off, rw_buf.data(), rw_buf.size());
                break;
            default:
                add_log("[ERR] C2H is read-only");
                return false;
        }
        if (!ok)
            add_log("[ERR] write: " + device.last_error());
        else
            add_log("[OK] write " + std::to_string(rw_buf.size()) + " B @ 0x" + rw_offset_hex);
        return ok;
    }

    rw_buf.resize(len);
    bool ok = false;
    switch (kind) {
        case xdma::ChannelKind::Control:
            ok = device.read_control(off, rw_buf.data(), rw_buf.size());
            break;
        case xdma::ChannelKind::User:
            ok = device.read_user(off, rw_buf.data(), rw_buf.size());
            break;
        case xdma::ChannelKind::H2C:
            add_log("[ERR] H2C is write-only");
            return false;
        case xdma::ChannelKind::C2H:
            ok = device.read_c2h(ch, off, rw_buf.data(), rw_buf.size());
            break;
        default: break;
    }
    if (!ok)
        add_log("[ERR] read: " + device.last_error());
    else
        add_log("[OK] read " + std::to_string(rw_buf.size()) + " B @ 0x" + rw_offset_hex);
    return ok;
}

void AppState::run_auto_test() {
    if (!device.is_open()) {
        add_log("[ERR] Open device first");
        return;
    }
    add_log("[INFO] Running xdma_test (official sample flow)...");
    auto_report = xdma::run_auto_dma_test(device, static_cast<size_t>(auto_test_size));
    add_log(std::string("[") + (auto_report.ok ? "PASS" : "FAIL") + "] " + auto_report.summary);
    for (const auto& ch : auto_report.channels) {
        if (!ch.present) continue;
        add_log("  ch" + std::to_string(ch.channel) + ": " + (ch.passed ? "OK" : "FAIL") + " - " +
                ch.detail);
    }
}

void AppState::close_device() {
    device.close();
    channels.clear();
    add_log("[OK] Device closed");
}

void AppState::toggle_channel(const xdma::ChannelDesc& ch) {
    if (!device.is_open()) {
        add_log("[ERR] Open device first");
        return;
    }
    if (ch.is_open) {
        device.close_channel(ch.kind, ch.channel);
        add_log("[OK] Closed " + ch.name);
    } else {
        if (!device.open_channel(ch.kind, ch.channel))
            add_log("[ERR] " + ch.name + ": " + device.last_error());
        else
            add_log("[OK] Opened " + ch.name);
    }
    refresh_channels();
}

void AppState::h2c_write(DmaSession& session, bool quiet) {
    if (!ensure_dma_channels(session, quiet)) return;
    fill_pattern(session, session.tx_buf);
    const uint64_t off = session_offset(session);
    const auto t0 = std::chrono::steady_clock::now();
    if (!device.write_h2c(session.channel, off, session.tx_buf.data(), session.tx_buf.size())) {
        if (!quiet) add_log("[ERR] " + std::string(session.title) + " H2C: " + device.last_error());
        return;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    session.last_h2c_mbps = (session.tx_buf.size() / (1024.0 * 1024.0)) / sec;
    if (!quiet)
        add_log("[OK] " + std::string(session.title) + " H2C " + std::to_string(session.tx_buf.size()) +
                " B @ " + session.offset_hex);
}

void AppState::c2h_read(DmaSession& session, bool quiet) {
    if (!ensure_dma_channels(session, quiet)) return;
    session.rx_buf.resize(static_cast<size_t>(session.transfer_size));
    const uint64_t off = session_offset(session);
    const auto t0 = std::chrono::steady_clock::now();
    if (!device.read_c2h(session.channel, off, session.rx_buf.data(), session.rx_buf.size())) {
        if (!quiet) add_log("[ERR] " + std::string(session.title) + " C2H: " + device.last_error());
        return;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    session.last_c2h_mbps = (session.rx_buf.size() / (1024.0 * 1024.0)) / sec;
    if (!quiet)
        add_log("[OK] " + std::string(session.title) + " C2H " + std::to_string(session.rx_buf.size()) +
                " B @ " + session.offset_hex);
}

void AppState::user_read_write(bool write) {
    if (!device.is_open()) {
        add_log("[ERR] 请先打开设备");
        return;
    }
    if (!device.open_channel(xdma::ChannelKind::User, 0)) {
        add_log("[ERR] " + device.last_error());
        return;
    }
    const uint64_t off = user_offset();
    static std::vector<uint8_t> user_buf;
    if (write) {
        fill_user_pattern(user_buf);
        if (!device.write_user(off, user_buf.data(), user_buf.size()))
            add_log("[ERR] USER write: " + device.last_error());
        else
            add_log("[OK] USER write " + std::to_string(user_buf.size()) + " B");
    } else {
        user_buf.resize(static_cast<size_t>(user_transfer_size));
        if (!device.read_user(off, user_buf.data(), user_buf.size()))
            add_log("[ERR] USER read: " + device.last_error());
        else
            add_log("[OK] USER read " + std::to_string(user_buf.size()) + " B");
    }
}

void AppState::benchmark(DmaSession& session, bool h2c) {
    if (!ensure_dma_channels(session)) return;
    const int iterations = 64;
    std::vector<uint8_t> buf(static_cast<size_t>(session.transfer_size));
    fill_pattern(session, buf);
    const uint64_t off = session_offset(session);

    size_t total = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        size_t done = 0;
        bool ok = h2c ? device.write_h2c(session.channel, off, buf.data(), buf.size(), &done)
                      : device.read_c2h(session.channel, off, buf.data(), buf.size(), &done);
        if (!ok) {
            add_log(std::string("[ERR] ") + session.title + " bench " + (h2c ? "H2C" : "C2H") + ": " +
                    device.last_error());
            return;
        }
        total += done;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    const double mbps = (total / (1024.0 * 1024.0)) / sec;
    if (h2c)
        session.last_h2c_mbps = mbps;
    else
        session.last_c2h_mbps = mbps;
    add_log(std::string("[BENCH] ") + session.title + " " + (h2c ? "H2C" : "C2H") + " " +
            std::to_string(mbps) + " MB/s (" + std::to_string(iterations) + " x " +
            std::to_string(session.transfer_size) + " B)");
}

}  // namespace xdma_app
