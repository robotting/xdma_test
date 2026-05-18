# =============================================================================
# Dear ImGui — 静态库 imgui_app
#
# 要求：
#   1. 已 add_subdirectory(glfw)，且存在目标 glfw
#   2. third_party/imgui 为官方 docking 分支（含 IMGUI_HAS_DOCK）
#
# 提供：imgui_app（PUBLIC 链接 glfw，含 GLFW + OpenGL3 后端）
# =============================================================================

if(NOT TARGET glfw)
    message(FATAL_ERROR "cmake/imgui.cmake: add_subdirectory(glfw) before including this file")
endif()

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")

if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "Missing vendored Dear ImGui at ${IMGUI_DIR}")
endif()

# 确认是 docking 分支，与 main.cpp 中 DockSpace / Viewports 功能一致
file(READ "${IMGUI_DIR}/imgui.h" _imgui_header LIMIT 4096)
if(NOT _imgui_header MATCHES "IMGUI_HAS_DOCK")
    message(FATAL_ERROR "third_party/imgui must be the docking branch (IMGUI_HAS_DOCK missing)")
endif()

# 仅编译运行时所需源文件（不含 demo、其它后端）
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_app STATIC ${IMGUI_SOURCES})

target_include_directories(imgui_app
    PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

# 后端头文件依赖 GLFW；PUBLIC 以便链接 imgui_app 的目标自动获得 glfw
target_link_libraries(imgui_app PUBLIC glfw)

target_compile_features(imgui_app PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(imgui_app PRIVATE /W3 /utf-8)
else()
    target_compile_options(imgui_app PRIVATE -Wall -Wextra)
endif()
