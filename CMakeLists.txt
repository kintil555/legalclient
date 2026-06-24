cmake_minimum_required(VERSION 3.20)
project(BedrockUtilityClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies via vcpkg --------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)

# --- ImGui -------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

# --- Kiero (DX11+DX12 hook helper) ------------------------------------------
set(KIERO_DIR ${CMAKE_SOURCE_DIR}/third_party/kiero)
set(KIERO_SOURCES ${KIERO_DIR}/kiero.cpp)

# --- Client sources ----------------------------------------------------------
file(GLOB_RECURSE CLIENT_SOURCES ${CMAKE_SOURCE_DIR}/Client/*.cpp)

add_library(BedrockUtilityClient SHARED
    ${CLIENT_SOURCES}
    ${IMGUI_SOURCES}
    ${KIERO_SOURCES}
)

target_include_directories(BedrockUtilityClient PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${KIERO_DIR}
    ${CMAKE_SOURCE_DIR}/Client
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(BedrockUtilityClient PRIVATE
    nlohmann_json::nlohmann_json
    minhook::minhook
    d3d11
    d3d12
    d3dcompiler
    dxgi
    dxguid
)

target_compile_definitions(BedrockUtilityClient PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
