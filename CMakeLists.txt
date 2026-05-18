# =============================================================================
# xdma_test — XDMA 测试工具（ImGui + GLFW + OpenGL）
# 依赖已内置在 third_party/，无需额外下载脚本
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(xdma_test LANGUAGES CXX)

# -----------------------------------------------------------------------------
# 语言标准
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# 第三方库路径（源码随仓库提供）
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

# Linux：仅用 X11，不启用 Wayland（否则需要 wayland-scanner、libwayland-dev 等）
# 若需 Wayland：安装上述包后改为 GLFW_BUILD_WAYLAND=ON，并删除 build 目录重新 cmake
if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
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
# 主程序
# -----------------------------------------------------------------------------
add_executable(xdma_test
    src/main.cpp          # ImGui 界面与测试逻辑
    src/xdma_device.cpp   # XDMA 设备读写封装
)
target_include_directories(xdma_test PRIVATE src)
target_link_libraries(xdma_test PRIVATE imgui_app OpenGL::GL Threads::Threads)

# Windows：设备枚举与 CreateFile 访问 XDMA 驱动
if(WIN32)
    target_link_libraries(xdma_test PRIVATE setupapi)
endif()

# Linux：GLFW 在 X11 下需要 X11 及常见扩展库
if(UNIX AND NOT APPLE)
    find_package(X11)
    if(X11_FOUND)
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
install(TARGETS xdma_test RUNTIME DESTINATION bin)
