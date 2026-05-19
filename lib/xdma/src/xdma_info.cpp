/**
 * @file xdma_info.cpp
 * @brief XDMA IP 寄存器解析与自动化 DMA 测试实现
 *
 * 寄存器位域解析逻辑移植自 driver/windows/exe/xdma_info/xdma_info.cpp
 * 自动测试流程移植自 driver/windows/exe/xdma_test/xdma_test.cpp
 */

#include <xdma/xdma_info.h>
#include <xdma/xdma_device.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <sstream>
#include <thread>
#include <vector>

namespace xdma {

namespace {

// -----------------------------------------------------------------------------
// 寄存器位操作工具（与官方 xdma_info 一致）
// -----------------------------------------------------------------------------

/** @return 第 n 位掩码 */
inline uint32_t bit(uint32_t n) { return (1u << n); }

/** @return x 的第 n 位是否为 1 */
inline bool is_bit_set(uint32_t x, uint32_t n) { return (x & bit(n)) == bit(n); }

/** @return 从第 n 位起长度为 l 的位掩码 */
inline uint32_t create_mask(uint32_t n, uint32_t l) { return ((1u << l) - 1u) << n; }

/** @return 提取 x 中 [n, n+l) 位域 */
inline uint32_t get_bits(uint32_t x, uint32_t n, uint32_t l) {
    return (x & create_mask(n, l)) >> n;
}

/**
 * @brief control 空间模块类型（ID 寄存器 bits[19:16]）
 * @see PG195 寄存器映射
 */
enum class ModuleKind {
    H2C = 0,
    C2H,
    IRQ,
    Config,
    H2C_SgDma,
    C2H_SgDma,
    SgDmaCommon,
};

/** 模块类型显示名 */
const char* module_name(ModuleKind m) {
    switch (m) {
        case ModuleKind::H2C: return "H2C";
        case ModuleKind::C2H: return "C2H";
        case ModuleKind::IRQ: return "IRQ";
        case ModuleKind::Config: return "Config";
        case ModuleKind::H2C_SgDma: return "H2C SGDMA";
        case ModuleKind::C2H_SgDma: return "C2H SGDMA";
        case ModuleKind::SgDmaCommon: return "SGDMA Common";
    }
    return "unknown";
}

/** 从模块 ID 寄存器解析模块种类 */
ModuleKind id_to_module(uint32_t id_reg) {
    return static_cast<ModuleKind>(get_bits(id_reg, 16, 4));
}

/** IP 版本号（ID 寄存器低 4 位）转 Vivado 版本字符串 */
std::string version_to_string(unsigned version) {
    switch (get_bits(version, 0, 4)) {
        case 1: return "2015.3/2015.4";
        case 2: return "2016.1";
        case 3: return "2016.2";
        case 4: return "2016.3";
        case 5: return "2016.4";
        case 6: return "2017.1";
        default: return "unknown";
    }
}

/**
 * @brief 通过 Device::read_control 批量读寄存器的辅助类
 */
class ControlReader {
public:
    explicit ControlReader(Device& dev) : dev_(dev) {}

    /** 连续读取 size 字节到 buffer */
    bool read_block(uint64_t addr, size_t size, void* buffer) {
        return dev_.read_control(addr, buffer, size);
    }

    /** 读单个 32 位寄存器；失败返回 0 */
    uint32_t read_reg(uint64_t addr) {
        uint32_t v = 0;
        if (!dev_.read_control(addr, &v, sizeof(v))) return 0;
        return v;
    }

private:
    Device& dev_;
};

// -----------------------------------------------------------------------------
// 各模块寄存器格式化输出（对应 xdma_info.cpp 各 print_*_module）
// -----------------------------------------------------------------------------

/** 解析 H2C/C2H 通道模块（每模块最多 4 路，间距 0x100） */
void append_channel_module(std::ostringstream& out, ControlReader& cr, long module_base) {
    std::array<uint32_t, 0xD4 / 4> regs{};

    for (unsigned i = 0; i < 4; ++i) {
        const long base = module_base + static_cast<long>(i) * 0x100;
        if (!cr.read_block(static_cast<uint64_t>(base), regs.size() * sizeof(uint32_t), regs.data()))
            continue;

        auto reg_at = [&](size_t off) { return regs[off / sizeof(uint32_t)]; };
        // 有效通道：ID 寄存器特定位域非零
        if ((reg_at(0x0) & 0x1cf00000u) == 0) continue;

        out << "  Channel " << i << ":\n";
        out << "    Channel ID:           " << get_bits(reg_at(0x0), 8, 4) << '\n';
        out << "    Version:              " << version_to_string(reg_at(0x0)) << '\n';
        out << "    Streaming:            " << (is_bit_set(reg_at(0x0), 15) ? "yes" : "no") << '\n';
        out << "    Running:              " << (is_bit_set(reg_at(0x4), 0) ? "yes" : "no") << '\n';
        out << "    Busy:                 " << (is_bit_set(reg_at(0x40), 0) ? "yes" : "no") << '\n';
        out << "    Completed Desc:       " << std::dec << reg_at(0x48) << '\n';
        out << "    Addr Alignment:       " << get_bits(reg_at(0x4C), 16, 7) << " bytes\n";
        out << "    Len Granularity:      " << get_bits(reg_at(0x4C), 8, 7) << " bytes\n";
        out << "    Addr bits:            " << get_bits(reg_at(0x4C), 0, 7) << " bits\n";
        out << std::hex;
    }
}

/** 解析 IRQ 模块 */
void append_irq_module(std::ostringstream& out, ControlReader& cr, long base) {
    std::array<uint32_t, 0xA8 / 4> regs{};
    if (!cr.read_block(static_cast<uint64_t>(base), regs.size() * sizeof(uint32_t), regs.data()))
        return;

    auto reg_at = [&](size_t off) { return regs[off / sizeof(uint32_t)]; };
    out << "  Version:                " << version_to_string(reg_at(0x0)) << '\n';
    out << std::hex;
    out << "  User IRQ en mask:       0x" << reg_at(0x4) << '\n';
    out << "  Chan IRQ en mask:       0x" << reg_at(0x10) << '\n';
    out << "  User IRQ:               0x" << reg_at(0x40) << '\n';
    out << "  Chan IRQ:               0x" << reg_at(0x44) << '\n';
    out << "  User IRQ pending:       0x" << reg_at(0x48) << '\n';
    out << "  Chan IRQ pending:       0x" << reg_at(0x4c) << '\n';
}

/** 解析 Config 模块（PCIe 能力、MPS/MRRS 等） */
void append_config_module(std::ostringstream& out, ControlReader& cr, long base) {
    std::array<uint32_t, 0x64 / 4> regs{};
    if (!cr.read_block(static_cast<uint64_t>(base), regs.size() * sizeof(uint32_t), regs.data()))
        return;

    auto reg_at = [&](size_t off) { return regs[off / sizeof(uint32_t)]; };
    out << "  Version:                " << version_to_string(reg_at(0x0)) << '\n';
    out << std::dec;
    out << "  PCIe bus/device/fn:     " << get_bits(reg_at(0x4), 8, 4) << '/'
        << get_bits(reg_at(0x4), 3, 5) << '/' << get_bits(reg_at(0x4), 0, 3) << '\n';
    out << "  PCIE MPS:               " << (1 << (7 + reg_at(0x8))) << " bytes\n";
    out << "  PCIE MRRS:              " << (1 << (7 + reg_at(0xC))) << " bytes\n";
    out << std::hex;
    out << "  System ID:              0x" << reg_at(0x10) << '\n';
    out << std::dec;
    out << "  MSI support:            " << (is_bit_set(reg_at(0x14), 0) ? "yes" : "no") << '\n';
    out << "  MSI-X support:          " << (is_bit_set(reg_at(0x14), 1) ? "yes" : "no") << '\n';
    out << "  PCIE Data Width:        " << (1 << (6 + reg_at(0x18))) << " bits\n";
}

/** 解析 H2C/C2H SGDMA 描述符通道模块 */
void append_sgdma_module(std::ostringstream& out, ControlReader& cr, long module_base) {
    std::array<uint32_t, 0x90 / 4> regs{};

    for (unsigned i = 0; i < 4; ++i) {
        const long base = module_base + static_cast<long>(i) * 0x100;
        if (!cr.read_block(static_cast<uint64_t>(base), regs.size() * sizeof(uint32_t), regs.data()))
            continue;

        auto reg_at = [&](size_t off) { return regs[off / sizeof(uint32_t)]; };
        // 有效通道：ID 寄存器特定位域非零
        if ((reg_at(0x0) & 0x1cf00000u) == 0) continue;

        out << "  Channel " << i << ":\n";
        out << "    Channel ID:           " << get_bits(reg_at(0x0), 8, 4) << '\n';
        out << "    Version:              " << version_to_string(reg_at(0x0)) << '\n';
        out << std::hex;
        out << "    Descr addr lo:        0x" << reg_at(0x80) << '\n';
        out << "    Descr addr hi:        0x" << reg_at(0x84) << '\n';
        out << std::dec;
        out << "    Adj Descriptors:      " << reg_at(0x88) << '\n';
        out << "    Descr credits:        " << reg_at(0x8C) << '\n';
    }
}

/** 解析 SGDMA 公共控制模块 */
void append_sgdma_common_module(std::ostringstream& out, ControlReader& cr, long base) {
    std::array<uint32_t, 0x24 / 4> regs{};
    if (!cr.read_block(static_cast<uint64_t>(base), regs.size() * sizeof(uint32_t), regs.data()))
        return;

    auto reg_at = [&](size_t off) { return regs[off / sizeof(uint32_t)]; };
    out << "  Version:                " << version_to_string(reg_at(0x0)) << '\n';
    out << std::hex;
    out << "  Halt H2C descr fetch:   0x" << get_bits(reg_at(0x10), 0, 4) << '\n';
    out << "  Halt C2H descr fetch:   0x" << get_bits(reg_at(0x10), 16, 4) << '\n';
    out << "  H2C descr credit:       0x" << get_bits(reg_at(0x20), 0, 4) << '\n';
    out << "  C2H descr credit:       0x" << get_bits(reg_at(0x20), 16, 4) << '\n';
}

/**
 * @brief 读取并格式化一个 4KB 模块块
 * @param base 模块基址（0、0x1000、0x2000 …）
 */
void append_block(std::ostringstream& out, ControlReader& cr, long base) {
    const uint32_t id = cr.read_reg(static_cast<uint64_t>(base));
    const auto mod = id_to_module(id);
    out << module_name(mod) << " Module @ 0x" << std::hex << base << std::dec << '\n';

    switch (mod) {
        case ModuleKind::H2C:
        case ModuleKind::C2H:
            append_channel_module(out, cr, base);
            break;
        case ModuleKind::IRQ:
            append_irq_module(out, cr, base);
            break;
        case ModuleKind::Config:
            append_config_module(out, cr, base);
            break;
        case ModuleKind::H2C_SgDma:
        case ModuleKind::C2H_SgDma:
            append_sgdma_module(out, cr, base);
            break;
        case ModuleKind::SgDmaCommon:
            append_sgdma_common_module(out, cr, base);
            break;
        default:
            break;
    }
    out << '\n';
}

/** 确保设备已打开且 control 通道可用 */
bool ensure_control(Device& device, std::string& err) {
    if (!device.is_open()) {
        err = "Device not opened";
        return false;
    }
    if (!device.is_channel_open(ChannelKind::Control, 0)) {
        if (!device.open_channel(ChannelKind::Control, 0)) {
            err = device.last_error();
            return false;
        }
    }
    return true;
}

}  // namespace

// =============================================================================
// 对外 API
// =============================================================================

IpInfoReport query_ip_info(Device& device) {
    IpInfoReport report;
    std::string err;
    if (!ensure_control(device, err)) {
        report.error = err;
        return report;
    }

    ControlReader cr(device);
    std::ostringstream out;
    out << "XDMA IP Status (xdma_info)\n";
    out << "Device: " << device.device_id() << "\n\n";

    // 官方示例扫描 7 个 4KB 对齐的模块区域
    for (long i = 0; i < 7; ++i) append_block(out, cr, i * 0x1000);

    report.text = out.str();
    report.ok = true;
    return report;
}

AutoTestReport run_auto_dma_test(Device& device, size_t transfer_bytes) {
    AutoTestReport report;

    if (!device.is_open()) {
        report.summary = "Device not opened";
        return report;
    }

    if (!device.is_channel_open(ChannelKind::Control, 0)) {
        if (!device.open_channel(ChannelKind::Control, 0)) {
            report.summary = device.last_error();
            return report;
        }
    }

    bool axi_st = false;
    if (!device.is_axi_streaming(&axi_st)) {
        report.summary = device.last_error();
        return report;
    }
    report.axi_streaming = axi_st;

    if (transfer_bytes == 0) transfer_bytes = 4096;
    transfer_bytes = (transfer_bytes + 3) & ~size_t(3);  // 对齐到 4 字节
    const size_t word_count = transfer_bytes / sizeof(uint32_t);

    // 测试图案：0, 1, 2, …（与官方 xdma_test 一致）
    std::vector<uint32_t> write_data(word_count);
    std::iota(write_data.begin(), write_data.end(), 0);

    int channels_found = 0;

    for (int ch = 0; ch < 4; ++ch) {
        ChannelTestResult tr;
        tr.channel = ch;

        const std::string h2c_path = Device::path_for(device.device_id(), ChannelKind::H2C, ch);
        const std::string c2h_path = Device::path_for(device.device_id(), ChannelKind::C2H, ch);
        if (!Device::probe_path(h2c_path) || !Device::probe_path(c2h_path)) {
            tr.present = false;
            tr.detail = "h2c/c2h node not found";
            report.channels.push_back(tr);
            continue;
        }
        tr.present = true;

        if (!device.open_channel(ChannelKind::H2C, ch) ||
            !device.open_channel(ChannelKind::C2H, ch)) {
            tr.passed = false;
            tr.detail = device.last_error();
            report.channels.push_back(tr);
            continue;
        }

        std::vector<uint32_t> read_data(word_count, 0);
        const uint64_t off = 0;

        if (axi_st) {
            // AXI-ST：并行写 H2C、读 C2H（需 FPGA 侧回环及时供给数据）
            std::thread h2c_thread([&] {
                device.write_h2c(ch, off, write_data.data(), write_data.size() * sizeof(uint32_t));
            });
            device.read_c2h(ch, off, read_data.data(), read_data.size() * sizeof(uint32_t));
            h2c_thread.join();
        } else {
            // AXI-MM：先写后读
            if (!device.write_h2c(ch, off, write_data.data(), write_data.size() * sizeof(uint32_t))) {
                tr.passed = false;
                tr.detail = "H2C: " + device.last_error();
                report.channels.push_back(tr);
                continue;
            }
            if (!device.read_c2h(ch, off, read_data.data(), read_data.size() * sizeof(uint32_t))) {
                tr.passed = false;
                tr.detail = "C2H: " + device.last_error();
                report.channels.push_back(tr);
                continue;
            }
        }

        tr.passed = (write_data == read_data);
        tr.detail = tr.passed ? "Data match OK" : "Data mismatch";
        if (tr.passed) ++channels_found;
        report.channels.push_back(tr);
    }

    if (channels_found == 0) {
        report.ok = false;
        report.summary = "Failure: no DMA channels passed";
    } else {
        // 存在的通道必须全部通过；不存在的通道不参与判定
        const bool all_pass = std::all_of(report.channels.begin(), report.channels.end(),
                                          [](const ChannelTestResult& c) {
                                              return !c.present || c.passed;
                                          });
        report.ok = all_pass;
        report.summary = all_pass ? "Success!" : "Failure: data mismatch on one or more channels";
    }

    return report;
}

}  // namespace xdma
