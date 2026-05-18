#include "xdma_device.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")
#else
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace xdma {

namespace {

const char* suffix(ChannelKind k) {
    switch (k) {
        case ChannelKind::H2C: return "h2c";
        case ChannelKind::C2H: return "c2h";
        case ChannelKind::User: return "user";
        case ChannelKind::Bypass: return "bypass";
    }
    return "";
}

#ifdef _WIN32
std::string wide_to_utf8(const wchar_t* w) {
    if (!w || !*w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

void set_error(std::string& err, const std::string& msg) {
    err = msg;
    DWORD code = GetLastError();
    if (code != 0) {
        std::ostringstream oss;
        oss << msg << " (Win32=" << code << ")";
        err = oss.str();
    }
}
#else
void set_error(std::string& err, const std::string& msg) {
    if (errno != 0)
        err = msg + " (" + std::string(std::strerror(errno)) + ")";
    else
        err = msg;
}

bool path_exists(const std::string& path) { return access(path.c_str(), F_OK) == 0; }
#endif

}  // namespace

Device::~Device() { close(); }

Device::Device(Device&& other) noexcept { *this = std::move(other); }

Device& Device::operator=(Device&& other) noexcept {
    if (this != &other) {
        close();
        device_id_ = std::move(other.device_id_);
        last_error_ = std::move(other.last_error_);
        std::memcpy(h2c_, other.h2c_, sizeof(h2c_));
        std::memcpy(c2h_, other.c2h_, sizeof(c2h_));
        user_ = other.user_;
        bypass_ = other.bypass_;
        std::memset(other.h2c_, 0, sizeof(other.h2c_));
        std::memset(other.c2h_, 0, sizeof(other.c2h_));
        other.user_ = kInvalid;
        other.bypass_ = kInvalid;
    }
    return *this;
}

std::vector<DeviceInfo> Device::enumerate() {
    std::vector<DeviceInfo> list;

#ifdef _WIN32
    HDEVINFO dev_info =
        SetupDiGetClassDevsW(nullptr, L"PCI", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (dev_info != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA dev_data{};
        dev_data.cbSize = sizeof(dev_data);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_data); ++i) {
            wchar_t instance_id[256]{};
            if (!SetupDiGetDeviceInstanceIdW(dev_info, &dev_data, instance_id,
                                             static_cast<DWORD>(std::size(instance_id)), nullptr))
                continue;

            std::string id = wide_to_utf8(instance_id);
            if (id.find("XDMA") == std::string::npos && id.find("xdma") == std::string::npos)
                continue;

            DeviceInfo info;
            info.id = id;
            list.push_back(info);
        }
        SetupDiDestroyDeviceInfoList(dev_info);
    }
#endif

    auto probe = [&](int n) {
        std::ostringstream oss;
        oss << "xdma" << n;
        const std::string id = oss.str();
        Device temp;
        temp.device_id_ = id;
        const std::string user_path = temp.make_path(ChannelKind::User, 0);
#ifdef _WIN32
        const std::wstring wpath = utf8_to_wide("\\\\.\\" + id + "_user");
        HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            DeviceInfo info;
            info.id = id;
            info.index = n;
            list.push_back(info);
        }
#else
        if (path_exists(user_path)) {
            DeviceInfo info;
            info.id = id;
            info.index = n;
            list.push_back(info);
        }
#endif
    };

    if (list.empty()) {
        for (int n = 0; n < 16; ++n) probe(n);
    }

#ifndef _WIN32
    // Scan /dev for xdma*_user nodes (covers non-standard naming)
    DIR* dir = opendir("/dev");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            const std::string name = ent->d_name;
            if (name.size() > 5 && name.compare(name.size() - 5, 5, "_user") == 0 &&
                name.rfind("xdma", 0) == 0) {
                const std::string id = name.substr(0, name.size() - 5);
                const bool exists = std::any_of(list.begin(), list.end(),
                                                [&](const DeviceInfo& d) { return d.id == id; });
                if (!exists) {
                    DeviceInfo info;
                    info.id = id;
                    list.push_back(info);
                }
            }
        }
        closedir(dir);
    }
#endif

    return list;
}

bool Device::open(const std::string& device_id) {
    close();
    device_id_ = device_id;
    last_error_.clear();
    return true;
}

void Device::close() {
    for (int i = 0; i < 4; ++i) {
        close_channel(ChannelKind::H2C, i);
        close_channel(ChannelKind::C2H, i);
    }
    close_channel(ChannelKind::User, 0);
    close_channel(ChannelKind::Bypass, 0);
    device_id_.clear();
}

bool Device::is_open() const { return !device_id_.empty(); }

std::string Device::make_path(ChannelKind kind, int channel) const {
    std::ostringstream oss;
#ifdef _WIN32
    oss << "\\\\.\\" << device_id_;
#else
    oss << "/dev/" << device_id_;
#endif
    if (kind != ChannelKind::User && kind != ChannelKind::Bypass)
        oss << "_" << suffix(kind) << "_" << channel;
    else
        oss << "_" << suffix(kind);
    return oss.str();
}

Device::Handle Device::channel_handle(ChannelKind kind, int channel) const {
    switch (kind) {
        case ChannelKind::H2C:
            if (channel >= 0 && channel < 4) return h2c_[channel];
            break;
        case ChannelKind::C2H:
            if (channel >= 0 && channel < 4) return c2h_[channel];
            break;
        case ChannelKind::User: return user_;
        case ChannelKind::Bypass: return bypass_;
    }
    return kInvalid;
}

bool Device::open_channel(ChannelKind kind, int channel) {
    if (!is_open()) {
        last_error_ = "Device not opened";
        return false;
    }
    if (is_channel_open(kind, channel)) return true;

    const std::string path = make_path(kind, channel);

#ifdef _WIN32
    const std::wstring wpath = utf8_to_wide(path);
    DWORD access = GENERIC_READ | GENERIC_WRITE;
    if (kind == ChannelKind::H2C) access = GENERIC_WRITE;
    if (kind == ChannelKind::C2H) access = GENERIC_READ;

    HANDLE h = CreateFileW(wpath.c_str(), access, 0, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        set_error(last_error_, "CreateFile failed: " + path);
        return false;
    }

    switch (kind) {
        case ChannelKind::H2C: h2c_[channel] = h; break;
        case ChannelKind::C2H: c2h_[channel] = h; break;
        case ChannelKind::User: user_ = h; break;
        case ChannelKind::Bypass: bypass_ = h; break;
    }
    return true;
#else
    int flags = O_RDWR;
    if (kind == ChannelKind::H2C) flags = O_WRONLY;
    if (kind == ChannelKind::C2H) flags = O_RDONLY;

    int fd = open(path.c_str(), flags);
    if (fd < 0) {
        // Some kernels expose channels as read-write only
        fd = open(path.c_str(), O_RDWR);
    }
    if (fd < 0) {
        set_error(last_error_, "open failed: " + path);
        return false;
    }

    switch (kind) {
        case ChannelKind::H2C: h2c_[channel] = fd; break;
        case ChannelKind::C2H: c2h_[channel] = fd; break;
        case ChannelKind::User: user_ = fd; break;
        case ChannelKind::Bypass: bypass_ = fd; break;
    }
    return true;
#endif
}

void Device::close_channel(ChannelKind kind, int channel) {
    Handle* target = nullptr;
    switch (kind) {
        case ChannelKind::H2C:
            if (channel >= 0 && channel < 4) target = &h2c_[channel];
            break;
        case ChannelKind::C2H:
            if (channel >= 0 && channel < 4) target = &c2h_[channel];
            break;
        case ChannelKind::User: target = &user_; break;
        case ChannelKind::Bypass: target = &bypass_; break;
    }
    if (!target || *target == kInvalid) return;

#ifdef _WIN32
    CloseHandle(*target);
#else
    close(*target);
#endif
    *target = kInvalid;
}

bool Device::is_channel_open(ChannelKind kind, int channel) const {
    return channel_handle(kind, channel) != kInvalid;
}

bool Device::transfer(Handle h, uint64_t offset, void* buf, size_t size, bool write,
                      size_t* out_bytes) {
    if (h == kInvalid) {
        last_error_ = "Channel not open";
        return false;
    }

#ifdef _WIN32
    LARGE_INTEGER li{};
    li.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) {
        set_error(last_error_, "SetFilePointerEx failed");
        return false;
    }

    DWORD done = 0;
    BOOL ok = write ? WriteFile(h, buf, static_cast<DWORD>(size), &done, nullptr)
                    : ReadFile(h, buf, static_cast<DWORD>(size), &done, nullptr);
    if (!ok) {
        set_error(last_error_, write ? "WriteFile failed" : "ReadFile failed");
        return false;
    }
    if (out_bytes) *out_bytes = done;
    if (done != size) {
        last_error_ = "Short transfer";
        return false;
    }
    return true;
#else
    const off_t pos = lseek(h, static_cast<off_t>(offset), SEEK_SET);
    if (pos < 0) {
        set_error(last_error_, "lseek failed");
        return false;
    }

    size_t total = 0;
    auto* ptr = static_cast<uint8_t*>(buf);
    while (total < size) {
        const ssize_t n =
            write ? ::write(h, ptr + total, size - total) : ::read(h, ptr + total, size - total);
        if (n < 0) {
            set_error(last_error_, write ? "write failed" : "read failed");
            return false;
        }
        if (n == 0) {
            last_error_ = "Short transfer";
            return false;
        }
        total += static_cast<size_t>(n);
    }
    if (out_bytes) *out_bytes = total;
    return true;
#endif
}

bool Device::write_h2c(int channel, uint64_t offset, const void* data, size_t size,
                       size_t* out_bytes) {
    return transfer(h2c_[channel], offset, const_cast<void*>(data), size, true, out_bytes);
}

bool Device::read_c2h(int channel, uint64_t offset, void* data, size_t size, size_t* out_bytes) {
    return transfer(c2h_[channel], offset, data, size, false, out_bytes);
}

bool Device::write_user(uint64_t offset, const void* data, size_t size, size_t* out_bytes) {
    return transfer(user_, offset, const_cast<void*>(data), size, true, out_bytes);
}

bool Device::read_user(uint64_t offset, void* data, size_t size, size_t* out_bytes) {
    return transfer(user_, offset, data, size, false, out_bytes);
}

}  // namespace xdma
