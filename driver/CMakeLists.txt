# =============================================================================
# XDMA 驱动（与 GUI 应用 xdma_test 分离）
# =============================================================================

# --- Windows：KMDF 驱动 + libxdma（MSBuild / WDK）---
if(WIN32)
    include("${CMAKE_SOURCE_DIR}/cmake/windows_xdma_driver.cmake")
endif()

# --- Linux：内核模块源码（make + 内核头文件，后续集成）---
if(UNIX AND NOT APPLE)
    set(XDMA_LINUX_SRC "${CMAKE_SOURCE_DIR}/driver/dma_ip_drivers/XDMA/linux-kernel/xdma")
    if(EXISTS "${XDMA_LINUX_SRC}/Makefile")
        message(STATUS "XDMA Linux driver sources: ${XDMA_LINUX_SRC}")
        message(STATUS "  Manual build: cd ${XDMA_LINUX_SRC} && make")
        # TODO: 对接内核树 out-of-tree 编译
    else()
        message(WARNING "Linux XDMA driver sources not found at ${XDMA_LINUX_SRC}")
    endif()
endif()
