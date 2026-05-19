/**
 * @file xdma_info.h
 * @brief XDMA IP 状态查询与自动化 DMA 测试
 *
 * 功能对应 Windows 驱动包示例程序：
 * - xdma_info.exe：解析 control 寄存器并生成可读报告
 * - xdma_test.exe：全通道 DMA 写读回环与数据校验
 *
 * 寄存器布局参见 Xilinx PG195（PCIe DMA IP）。
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xdma {

class Device;

/**
 * @brief IP 信息查询结果（xdma_info 风格）
 */
struct IpInfoReport {
    std::string text;   ///< 多行文本报告，可直接显示在日志/UI
    bool ok = false;    ///< 是否成功完成读取与解析
    std::string error;  ///< 失败原因（ok==false 时有效）
};

/**
 * @brief 读取 XDMA IP 全部模块状态并格式化为文本
 *
 * 自动打开 control 通道（若尚未打开），依次读取 7 个 4KB 模块块：
 * H2C、C2H、IRQ、Config、H2C SGDMA、C2H SGDMA、SGDMA Common。
 *
 * @param device 已 open() 的设备对象
 * @return IpInfoReport；失败时 ok=false 且 error 有说明
 */
IpInfoReport query_ip_info(Device& device);

/**
 * @brief 单路 DMA 通道测试结果
 */
struct ChannelTestResult {
    int channel = 0;       ///< 通道号 0–3
    bool present = false;  ///< h2c_N 与 c2h_N 节点是否均存在
    bool passed = false;   ///< 数据校验是否通过
    std::string detail;    ///< 结果说明或错误信息
};

/**
 * @brief 自动 DMA 测试总报告（xdma_test 风格）
 */
struct AutoTestReport {
    bool ok = false;              ///< 全部存在通道是否均通过
    bool axi_streaming = false;   ///< 检测到的接口模式（AXI-ST / AXI-MM）
    std::vector<ChannelTestResult> channels;  ///< 每路通道结果
    std::string summary;          ///< 总结语，如 "Success!" 或失败原因
};

/**
 * @brief 运行官方 xdma_test 等效流程
 *
 * 步骤：
 * 1. 读 control 判断 AXI-ST / AXI-MM
 * 2. 对通道 0–3：若 h2c/c2h 存在，写入递增 32 位数据并读回比较
 * 3. AXI-MM：顺序 H2C 写再 C2H 读；AXI-ST：H2C 与 C2H 并行（需 FPGA 回环）
 *
 * @param device 已 open() 的设备
 * @param transfer_bytes 单次传输字节数，默认 4096（官方 BRAM 示例上限）
 * @return AutoTestReport
 */
AutoTestReport run_auto_dma_test(Device& device, size_t transfer_bytes = 4096);

}  // namespace xdma
