# =============================================================================
# xdma_test — XDMA 测试 GUI（ImGui + GLFW）
# 依赖 libxdma 用户态库 + third_party/
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(xdma_test LANGUAGES CXX)

# XDMA 驱动（Windows 已实现 MSBuild 目标；Linux 源码在 driver/dma_ip_drivers）
option(XDMA_BUILD_DRIVER "编译 driver/ 下的 XDMA 驱动" ON)

# -----------------------------------------------------------------------------
# 语言标准
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(XDMA_BUILD_DRIVER)
    add_subdirectory(driver)
endif()

# -----------------------------------------------------------------------------
# libxdma — 用户态 XDMA 操作库
# -----------------------------------------------------------------------------
include("${CMAKE_SOURCE_DIR}/cmake/xdma_lib.cmake")

# -----------------------------------------------------------------------------
# 第三方库
# -----------------------------------------------------------------------------
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
set(GLFW_DIR "${THIRD_PARTY_DIR}/glfw")

# -----------------------------------------------------------------------------
# GLFW — 窗口与 OpenGL 上下文
# -----------------------------------------------------------------------------
if(NOT EXISTS "${GLFW_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing vendored GLFW at ${GLFW_DIR}")
endif()

# 关闭 GLFW 自带的示例/测试/文档，加快配置与编译
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Linux 显示后端（Wayland 开发环境默认 Wayland；可 -DXDMA_LINUX_X11=ON 切回纯 X11）
if(UNIX AND NOT APPLE)
    option(XDMA_LINUX_X11 "Linux: GLFW 使用 X11（关闭则用 Wayland）" OFF)
    if(XDMA_LINUX_X11)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    else()
        # GLFW Wayland 在 third_party/glfw/src/CMakeLists.txt 里会调用
        #   include(FindPkgConfig)
        #   pkg_check_modules(Wayland REQUIRED wayland-client ...)
        # 因此需要系统安装 pkg-config 及 Wayland 开发包，否则会报 FindPkgConfig 相关错误
        find_package(PkgConfig QUIET)
        if(NOT PkgConfig_FOUND)
            message(FATAL_ERROR
                "未找到 PkgConfig（pkg-config）。GLFW Wayland 后端依赖它查找系统库。\n"
                "  sudo apt install pkg-config cmake")
        endif()
        find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
        if(NOT WAYLAND_SCANNER_EXECUTABLE)
            message(FATAL_ERROR
                "未找到 wayland-scanner。\n"
                "  sudo apt install libwayland-dev")
        endif()
        pkg_check_modules(WAYLAND_DEPS QUIET
            wayland-client>=0.2.7
            wayland-cursor>=0.2.7
            wayland-egl>=0.2.7
            xkbcommon>=0.5.0)
        if(NOT WAYLAND_DEPS_FOUND)
            message(FATAL_ERROR
                "pkg-config 找不到 Wayland 库（wayland-client / xkbcommon 等）。\n"
                "  sudo apt install libwayland-dev libxkbcommon-dev libegl1-mesa-dev \\\n"
                "    libgl1-mesa-dev pkg-config")
        endif()
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    endif()
endif()

add_subdirectory("${GLFW_DIR}" "${CMAKE_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)

# -----------------------------------------------------------------------------
# Dear ImGui (docking) — 见 cmake/imgui.cmake
# 须在 GLFW 之后 include（imgui 后端依赖 glfw 目标）
# -----------------------------------------------------------------------------
include("${CMAKE_SOURCE_DIR}/cmake/imgui.cmake")

# -----------------------------------------------------------------------------
# 系统库
# -----------------------------------------------------------------------------
find_package(OpenGL REQUIRED)   # OpenGL32 (Win) / GL (Linux)
find_package(Threads REQUIRED)  # std::thread 等

# -----------------------------------------------------------------------------
# 测试程序（GUI）
# -----------------------------------------------------------------------------
include("${CMAKE_SOURCE_DIR}/cmake/gui_app.cmake")

add_executable(xdma_test ${XDMA_GUI_SOURCES})
target_include_directories(xdma_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(xdma_test PRIVATE xdma imgui_app OpenGL::GL Threads::Threads)

# Windows：设备枚举与 CreateFile 访问 XDMA 驱动（已由 xdma 库链接 setupapi）
if(UNIX AND NOT APPLE)
    if(XDMA_LINUX_X11)
        find_package(X11 REQUIRED)
        target_link_libraries(xdma_test PRIVATE X11::X11)
    endif()
    target_link_libraries(xdma_test PRIVATE dl m)
endif()

# -----------------------------------------------------------------------------
# 编译警告
# -----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(xdma_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(xdma_test PRIVATE -Wall -Wextra)
endif()

# -----------------------------------------------------------------------------
# 安装（可选）
# -----------------------------------------------------------------------------
install(TARGETS xdma xdma_test
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY lib/xdma/include/ DESTINATION include)
