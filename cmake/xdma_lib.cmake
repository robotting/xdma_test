# =============================================================================
# libxdma — XDMA 用户态静态库
#
# 头文件:
#   xdma/xdma_device.h  — 设备枚举、通道、DMA/BAR 读写
#   xdma/xdma_info.h    — IP 寄存器报告、自动 DMA 测试
#
# 源文件:
#   lib/xdma/src/xdma_device.cpp
#   lib/xdma/src/xdma_info.cpp
# =============================================================================

set(XDMA_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/xdma")

add_library(xdma STATIC
    ${XDMA_LIB_DIR}/src/xdma_device.cpp
    ${XDMA_LIB_DIR}/src/xdma_info.cpp
)

target_include_directories(xdma
    PUBLIC
        ${XDMA_LIB_DIR}/include
)

target_compile_features(xdma PUBLIC cxx_std_17)

if(WIN32)
    target_link_libraries(xdma PUBLIC setupapi)
endif()

if(MSVC)
    target_compile_options(xdma PRIVATE /W4 /utf-8)
else()
    target_compile_options(xdma PRIVATE -Wall -Wextra)
endif()
