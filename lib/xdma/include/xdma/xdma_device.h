/**
 * @file xdma_device.h
 * @brief Xilinx XDMA 用户态设备访问库（跨 Windows / Linux）
 *
 * 封装驱动暴露的设备节点，提供：
 * - 设备枚举与逻辑打开/关闭
 * - 各通道（H2C、C2H、User、Bypass、Control）句柄管理
 * - 带偏移的 DMA 与 BAR 读写
 *
 * 设备节点命名约定：
 * - Windows: \\.\<device_id>_h2c_0、\\.\<device_id>_user 等
 * - Linux:   /dev/<device_id>_h2c_0、/dev/<device_id>_user 等
 *
 * 对应驱动示例：driver/windows/exe/xdma_rw、xdma_test
 */

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

/**
 * @brief 设备通道类型
 *
 * 与 XDMA 驱动创建的字符设备/符号链接一一对应。
 */
enum class ChannelKind {
    H2C,      ///< Host-to-Card，主机写 FPGA（DMA 写）
    C2H,      ///< Card-to-Host，主机读 FPGA（DMA 读）
    User,     ///< 用户逻辑 BAR（AXI-Lite 等）
    Bypass,   ///< Bypass BAR
    Control,  ///< XDMA IP 控制寄存器空间（状态/配置）
};

/** @brief 枚举到的设备摘要信息 */
struct DeviceInfo {
    std::string id;   ///< 逻辑设备 ID，如 "xdma0" 或 Windows 实例 ID
    int index = 0;    ///< 设备序号（xdma0 -> 0）
};

/** @brief 通道节点在系统中的存在状态 */
enum class ChannelPresence {
    Unknown,  ///< 未探测
    Present,  ///< 节点存在且可访问
    Absent,   ///< 节点不存在
};

/** @brief 单个通道的描述（用于 UI 列表展示） */
struct ChannelDesc {
    ChannelKind kind = ChannelKind::H2C;
    int channel = 0;                         ///< DMA 通道号 0–3；User/Bypass/Control 为 0
    std::string name;                        ///< 显示名，如 "h2c_0"
    std::string path;                        ///< 完整设备路径
    ChannelPresence presence = ChannelPresence::Unknown;
    bool is_open = false;                    ///< 当前 Device 对象是否已打开该通道句柄
};

/**
 * @class Device
 * @brief XDMA 用户态访问入口
 *
 * 使用流程：
 * 1. enumerate() 或手动指定 device_id
 * 2. open(device_id) — 仅记录逻辑设备，不打开内核句柄
 * 3. open_channel() — 按需打开 h2c/c2h/user/control 等
 * 4. read_* / write_* 进行传输
 * 5. close() — 关闭所有通道并释放逻辑设备
 *
 * 线程安全：未做同步，多线程访问需调用方加锁。
 */
class Device {
public:
    Device() = default;
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&& other) noexcept;
    Device& operator=(Device&& other) noexcept;

    /**
     * @brief 枚举本机 XDMA 设备
     * @return 设备列表；Windows 优先 SetupAPI 匹配 XDMA，否则探测 xdma0..xdma15
     */
    static std::vector<DeviceInfo> enumerate();

    /**
     * @brief 打开逻辑设备（不打开任何通道句柄）
     * @param device_id 如 "xdma0"
     * @return 成功返回 true（仅设置内部 ID）
     */
    bool open(const std::string& device_id);

    /** @brief 关闭所有通道并清除 device_id */
    void close();

    /** @brief 是否已通过 open() 绑定设备 ID */
    bool is_open() const;

    /** @brief 当前绑定的设备 ID */
    const std::string& device_id() const { return device_id_; }

    /**
     * @brief 构造某通道的设备节点路径（不访问硬件）
     * @param device_id 逻辑设备名
     * @param kind 通道类型
     * @param channel H2C/C2H 通道号 0–3，其余类型忽略
     */
    static std::string path_for(const std::string& device_id, ChannelKind kind, int channel = 0);

    /**
     * @brief 探测路径是否存在且可打开
     * @param path path_for() 生成的路径
     */
    static bool probe_path(const std::string& path);

    /**
     * @brief 列出当前设备全部标准通道及存在/打开状态
     * @return 空列表若设备未 open()
     */
    std::vector<ChannelDesc> list_channels() const;

    /** @brief 关闭 h2c/c2h/user/bypass/control 全部句柄 */
    void close_all_channels();

    /**
     * @brief 对存在的通道逐个 open_channel()
     * @return 成功打开的通道数量
     */
    int open_all_present_channels();

    /**
     * @brief 关闭当前已打开的所有通道
     * @return 关闭的通道数量
     */
    int close_all_open_channels();

    /**
     * @brief 打开指定通道内核句柄
     * @param kind 通道类型
     * @param channel H2C/C2H 为 0–3
     * @return 失败时查看 last_error()
     */
    bool open_channel(ChannelKind kind, int channel = 0);

    /** @brief 关闭指定通道句柄（未打开则无操作） */
    void close_channel(ChannelKind kind, int channel = 0);

    /** @brief 指定通道句柄是否已打开 */
    bool is_channel_open(ChannelKind kind, int channel = 0) const;

    /**
     * @brief H2C DMA 写（主机 -> FPGA）
     * @param channel 0–3
     * @param offset 设备地址空间偏移（字节）
     * @param data 源缓冲区
     * @param size 字节数
     * @param out_bytes 可选，实际传输字节数
     */
    bool write_h2c(int channel, uint64_t offset, const void* data, size_t size,
                   size_t* out_bytes = nullptr);

    /**
     * @brief C2H DMA 读（FPGA -> 主机）
     * @param channel 0–3
     * @param offset 设备地址空间偏移（字节）
     * @param data 目标缓冲区
     * @param size 字节数
     * @param out_bytes 可选，实际传输字节数
     */
    bool read_c2h(int channel, uint64_t offset, void* data, size_t size, size_t* out_bytes = nullptr);

    /**
     * @brief 写 User BAR
     * @note 需先 open_channel(ChannelKind::User)
     */
    bool write_user(uint64_t offset, const void* data, size_t size, size_t* out_bytes = nullptr);

    /** @brief 读 User BAR */
    bool read_user(uint64_t offset, void* data, size_t size, size_t* out_bytes = nullptr);

    /**
     * @brief 写 Control 寄存器空间
     * @note 用于访问 XDMA IP 配置/状态寄存器，见 PG195
     */
    bool write_control(uint64_t offset, const void* data, size_t size, size_t* out_bytes = nullptr);

    /** @brief 读 Control 寄存器空间 */
    bool read_control(uint64_t offset, void* data, size_t size, size_t* out_bytes = nullptr);

    /**
     * @brief 判断 IP 是否为 AXI-Stream 模式
     *
     * 读取 control 空间偏移 0 的寄存器 bit15：
     * - true  : AXI-ST（流式），官方 xdma_test 使用并行 H2C/C2H
     * - false : AXI-MM（存储映射），先写后读
     *
     * @param out 可选，输出是否为流式
     * @return 读寄存器成功为 true
     */
    bool is_axi_streaming(bool* out = nullptr);

    /** @brief 最近一次失败的操作描述（含 Win32/errno 码） */
    std::string last_error() const { return last_error_; }

private:
#ifdef _WIN32
    using Handle = HANDLE;
    static constexpr Handle kInvalid = INVALID_HANDLE_VALUE;
#else
    using Handle = int;
    static constexpr Handle kInvalid = -1;
#endif

    /** @brief 获取已打开通道的句柄，未打开返回 kInvalid */
    Handle channel_handle(ChannelKind kind, int channel) const;

    /**
     * @brief 底层读写：定位 offset 后一次 ReadFile/WriteFile（或 read/write）
     * @param write true=写，false=读
     * @return 仅当传输字节数等于 size 时返回 true
     */
    bool transfer(Handle h, uint64_t offset, void* buf, size_t size, bool write, size_t* out_bytes);

    /** @brief 基于当前 device_id_ 生成通道路径 */
    std::string make_path(ChannelKind kind, int channel) const;

    std::string device_id_;   ///< 逻辑设备名
    std::string last_error_;  ///< 最后一次错误信息

    Handle h2c_[4]{};              ///< H2C 通道 0–3 句柄
    Handle c2h_[4]{};              ///< C2H 通道 0–3 句柄
    Handle user_ = kInvalid;       ///< User BAR 句柄
    Handle bypass_ = kInvalid;     ///< Bypass BAR 句柄
    Handle control_ = kInvalid;    ///< Control 寄存器句柄
};

}  // namespace xdma
