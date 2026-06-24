@echo off
setlocal EnableDelayedExpansion

:: ============================================================
:: build.bat — LegalClient Local Build Script
:: Usage: build.bat [R|D]   R=Release (default), D=Debug
::        build.bat R -f    Force reconfigure CMake
:: ============================================================

echo.
echo  =============================================
echo          LegalClient Build Script
echo  =============================================
echo.

:: --- Build type ---
set "BUILD_TYPE=Release"
if /I "%~1"=="D" set "BUILD_TYPE=Debug"
if /I "%~1"=="d" set "BUILD_TYPE=Debug"
if "%~1"=="" (
    set /p "choice= Build type? [R=Release / D=Debug, default R]: "
    if /I "!choice!"=="D" set "BUILD_TYPE=Debug"
)
echo  Build type : %BUILD_TYPE%

:: --- Cari vcvarsall.bat (VS 2022 / 2019) ---
set "VCVARS="
for %%E in (Enterprise Professional Community BuildTools) do (
    if not defined VCVARS (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
)
for %%E in (Enterprise Professional Community BuildTools) do (
    if not defined VCVARS (
        if exist "C:\Program Files\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
)
if not defined VCVARS (
    echo  [ERROR] Visual Studio tidak ditemukan! Install VS 2019/2022 dengan C++ tools.
    pause & exit /b 1
)
echo  MSVC       : %VCVARS%
call "%VCVARS%" x64 >nul 2>&1

:: --- Cari Ninja ---
where ninja >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] ninja tidak ditemukan. Install via: choco install ninja
    pause & exit /b 1
)

:: --- Cari CMake ---
where cmake >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] cmake tidak ditemukan. Install dari https://cmake.org
    pause & exit /b 1
)

echo.
cmake --version | findstr "cmake version"
ninja --version
echo.

:: --- vcpkg ---
set "VCPKG_DIR=%~dp0vcpkg"
if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo  Bootstrap vcpkg...
    if not exist "%VCPKG_DIR%" git clone https://github.com/microsoft/vcpkg.git "%VCPKG_DIR%"
    call "%VCPKG_DIR%\bootstrap-vcpkg.bat" >nul
)

:: --- ImGui (vendored) ---
set "IMGUI_DIR=%~dp0third_party\imgui"
if not exist "%IMGUI_DIR%\imgui.h" (
    echo  Mengunduh ImGui v1.90.9...
    git clone --branch v1.90.9 --depth 1 https://github.com/ocornut/imgui.git "%IMGUI_DIR%"
)

:: --- Build dir ---
set "BUILD_DIR=%~dp0build"
set "FORCE_CONFIG=0"
if /I "%~2"=="-f" set "FORCE_CONFIG=1"
if /I "%~2"=="-F" set "FORCE_CONFIG=1"
if not exist "%BUILD_DIR%\CMakeCache.txt" set "FORCE_CONFIG=1"
if not exist "%BUILD_DIR%\build.ninja"    set "FORCE_CONFIG=1"

if "%FORCE_CONFIG%"=="1" (
    echo  Konfigurasi CMake...
    cmake -S "%~dp0" -B "%BUILD_DIR%" ^
        -G "Ninja" ^
        -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" ^
        -DVCPKG_TARGET_TRIPLET=x64-windows ^
        -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    if errorlevel 1 (
        echo  [ERROR] CMake configuration gagal!
        pause & exit /b 1
    )
) else (
    echo  Pakai konfigurasi CMake sebelumnya. (gunakan -f untuk reconfigure)
)

:: --- Build ---
echo.
echo  Building dengan %NUMBER_OF_PROCESSORS% core...
cmake --build "%BUILD_DIR%" --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo.
    echo  [ERROR] Build gagal!
    pause & exit /b 1
)

:: --- Cari output DLL ---
echo.
for /r "%BUILD_DIR%" %%F in (BedrockUtilityClient.dll) do (
    echo  [OK] DLL siap: %%F
    echo.
    echo  Untuk inject: auto-inject.bat "%%F"
    echo.
)

pause
