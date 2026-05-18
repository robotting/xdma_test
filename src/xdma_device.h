#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace xdma {

enum class ChannelKind { H2C, C2H, User, Bypass };

struct DeviceInfo {
    std::string id;       // e.g. "xdma0"
    int index = 0;
};

class Device {
public:
    Device() = default;
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&& other) noexcept;
    Device& operator=(Device&& other) noexcept;

    static std::vector<DeviceInfo> enumerate();

    bool open(const std::string& device_id);
    void close();
    bool is_open() const;

    bool open_channel(ChannelKind kind, int channel = 0);
    void close_channel(ChannelKind kind, int channel = 0);
    bool is_channel_open(ChannelKind kind, int channel = 0) const;

    bool write_h2c(int channel, uint64_t offset, const void* data, size_t size, size_t* out_bytes = nullptr);
    bool read_c2h(int channel, uint64_t offset, void* data, size_t size, size_t* out_bytes = nullptr);

    bool write_user(uint64_t offset, const void* data, size_t size, size_t* out_bytes = nullptr);
    bool read_user(uint64_t offset, void* data, size_t size, size_t* out_bytes = nullptr);

    std::string last_error() const { return last_error_; }

private:
#ifdef _WIN32
    using Handle = HANDLE;
    static constexpr Handle kInvalid = INVALID_HANDLE_VALUE;
#else
    using Handle = int;
    static constexpr Handle kInvalid = -1;
#endif

    Handle channel_handle(ChannelKind kind, int channel) const;
    bool transfer(Handle h, uint64_t offset, void* buf, size_t size, bool write, size_t* out_bytes);
    std::string make_path(ChannelKind kind, int channel) const;

    std::string device_id_;
    std::string last_error_;

    Handle h2c_[4]{};
    Handle c2h_[4]{};
    Handle user_ = kInvalid;
    Handle bypass_ = kInvalid;
};

}  // namespace xdma
