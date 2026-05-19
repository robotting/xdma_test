# =============================================================================
# Windows XDMA 内核驱动 — MSBuild 编译（需安装 WDK + VS）
# 源码：driver/windows/
# =============================================================================

set(XDMA_WIN_DRIVER_ROOT "${CMAKE_SOURCE_DIR}/driver/windows")
set(XDMA_WIN_LIB_PROJ "${XDMA_WIN_DRIVER_ROOT}/libxdma/libxdma.vcxproj")
set(XDMA_WIN_SYS_PROJ "${XDMA_WIN_DRIVER_ROOT}/sys/XDMA_Driver.vcxproj")
set(XDMA_WIN_TOOL_RW_PROJ "${XDMA_WIN_DRIVER_ROOT}/exe/xdma_rw/xdma_rw.vcxproj")
set(XDMA_WIN_TOOL_INFO_PROJ "${XDMA_WIN_DRIVER_ROOT}/exe/xdma_info/xdma_info.vcxproj")

if(NOT EXISTS "${XDMA_WIN_LIB_PROJ}" OR NOT EXISTS "${XDMA_WIN_SYS_PROJ}")
    message(FATAL_ERROR "Missing Windows XDMA driver projects under ${XDMA_WIN_DRIVER_ROOT}")
endif()

# WDK：内核头 ntddk.h + 构建 props（旧版在 build/ 根目录，新版在 build/<ver>/ 下）
file(GLOB _wdk_ntddk
    "C:/Program Files (x86)/Windows Kits/10/Include/*/km/ntddk.h"
    "C:/Program Files/Windows Kits/10/Include/*/km/ntddk.h")
file(GLOB _wdk_props
    "C:/Program Files (x86)/Windows Kits/10/build/WindowsKernelModeDriver10.0.props"
    "C:/Program Files/Windows Kits/10/build/*/WindowsDriver.KernelMode.props"
    "C:/Program Files/Windows Kits/10/build/*/x64/WindowsKernelModeDriver/*.props")
if(NOT _wdk_ntddk)
    message(WARNING
        "未检测到 WDK 内核头文件 (ntddk.h)。无法编译 xdma_win_driver。\n"
        "  https://learn.microsoft.com/windows-hardware/drivers/download-the-wdk")
    return()
endif()
list(GET _wdk_ntddk 0 _wdk_ntddk_path)
if(_wdk_props)
    list(GET _wdk_props 0 _wdk_props_path)
else()
    set(_wdk_props_path "(headers only)")
endif()
message(STATUS "WDK detected: ${_wdk_ntddk_path}")
message(STATUS "WDK build:  ${_wdk_props_path}")

if(NOT MSBUILD_EXECUTABLE)
    set(_pf86 "C:/Program Files (x86)")
    file(GLOB _msbuild_candidates
        "$ENV{ProgramFiles}/Microsoft Visual Studio/*/Professional/MSBuild/*/Bin/amd64/MSBuild.exe"
        "$ENV{ProgramFiles}/Microsoft Visual Studio/*/Enterprise/MSBuild/*/Bin/amd64/MSBuild.exe"
        "$ENV{ProgramFiles}/Microsoft Visual Studio/*/Community/MSBuild/*/Bin/amd64/MSBuild.exe"
        "${_pf86}/Microsoft Visual Studio/*/Professional/MSBuild/*/Bin/amd64/MSBuild.exe"
    )
    if(_msbuild_candidates)
        list(GET _msbuild_candidates 0 MSBUILD_EXECUTABLE)
    endif()
    find_program(MSBUILD_EXECUTABLE NAMES msbuild MSBuild)
endif()

if(NOT MSBUILD_EXECUTABLE)
    message(WARNING "未找到 MSBuild，跳过 xdma_win_driver 目标")
    return()
endif()

set(_xdma_drv_cfg "Win10_Release")
set(_xdma_drv_plat "x64")
set(_xdma_tool_cfg "Debug")
set(_xdma_tool_plat "x64")
set(_msb_common "/p:SpectreMitigation=false")

add_custom_target(xdma_win_driver
    COMMAND "${MSBUILD_EXECUTABLE}"
        "${XDMA_WIN_LIB_PROJ}" /m /t:Build
        /p:Configuration=${_xdma_drv_cfg} /p:Platform=${_xdma_drv_plat} ${_msb_common}
    COMMAND "${MSBUILD_EXECUTABLE}"
        "${XDMA_WIN_SYS_PROJ}" /m /t:Build
        /p:Configuration=${_xdma_drv_cfg} /p:Platform=${_xdma_drv_plat} ${_msb_common}
    WORKING_DIRECTORY "${XDMA_WIN_DRIVER_ROOT}"
    USES_TERMINAL
    COMMENT "Building XDMA Windows driver (${_xdma_drv_cfg}|${_xdma_drv_plat})"
)

add_custom_target(xdma_win_tools
    COMMAND "${MSBUILD_EXECUTABLE}"
        "${XDMA_WIN_TOOL_RW_PROJ}" /m /t:Build
        /p:Configuration=${_xdma_tool_cfg} /p:Platform=${_xdma_tool_plat} ${_msb_common}
    COMMAND "${MSBUILD_EXECUTABLE}"
        "${XDMA_WIN_TOOL_INFO_PROJ}" /m /t:Build
        /p:Configuration=${_xdma_tool_cfg} /p:Platform=${_xdma_tool_plat} ${_msb_common}
    WORKING_DIRECTORY "${XDMA_WIN_DRIVER_ROOT}"
    USES_TERMINAL
    COMMENT "Building XDMA Windows sample tools"
)

message(STATUS "XDMA Windows driver root: ${XDMA_WIN_DRIVER_ROOT}")
message(STATUS "  cmake --build build --target xdma_win_driver")
message(STATUS "  Output: ${XDMA_WIN_DRIVER_ROOT}/build/x64/XDMA_Driver/")
