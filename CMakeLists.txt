cmake_minimum_required(VERSION 3.20)
project(BedrockUtilityClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies -----------------------------------------------------
# Resolved via vcpkg manifest mode (see vcpkg.json):
#   - nlohmann-json
#   - minhook
# ImGui is vendored manually - fetched by CI into third_party/imgui,
# or clone it yourself locally before building.
find_package(nlohmann_json CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

file(GLOB_RECURSE CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/Client/*.cpp
)

add_library(BedrockUtilityClient SHARED
    ${CLIENT_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(BedrockUtilityClient PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/Client
)

target_link_libraries(BedrockUtilityClient PRIVATE
    nlohmann_json::nlohmann_json
    minhook::minhook
    d3d11
    d3dcompiler
    dxgi
)

target_compile_definitions(BedrockUtilityClient PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ----- JANGAN SET /ENTRY:DllMain -----
# MSVC DLL otomatis pakai _DllMainCRTStartup sebagai entry point asli.
# _DllMainCRTStartup init CRT dulu (malloc, memcpy, exceptions, dll)
# LALU memanggil DllMain kita. Kalau /ENTRY di-override, _DllMainCRTStartup
# di-skip -> 90+ LNK2019 error untuk semua simbol CRT.
# Biarkan linker pakai default DLL entry point.
