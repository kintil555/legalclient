@echo off
setlocal EnableDelayedExpansion

:: ============================================================
:: auto-inject.bat — LegalClient Auto Injector (GDK Edition)
:: Inject BedrockUtilityClient.dll ke Minecraft.Windows.exe
::
:: Usage:
::   auto-inject.bat              (cari DLL otomatis di folder ini)
::   auto-inject.bat path\to.dll  (inject DLL tertentu)
:: ============================================================

echo.
echo  =============================================
echo        LegalClient Auto Injector
echo        Minecraft Bedrock GDK Edition
echo  =============================================
echo.

:: --- Tentukan path DLL ---
set "DLL_PATH=%~1"

if "%DLL_PATH%"=="" (
    for %%F in ("%~dp0*.dll") do (
        set "DLL_PATH=%%F"
        goto :found_dll
    )
    for /r "%~dp0build" %%F in (BedrockUtilityClient.dll) do (
        set "DLL_PATH=%%F"
        goto :found_dll
    )
    echo  [ERROR] Tidak ada .dll ditemukan di folder ini.
    echo  Usage: auto-inject.bat path\to\BedrockUtilityClient.dll
    echo.
    pause
    exit /b 1
)

:found_dll
for %%F in ("%DLL_PATH%") do set "DLL_PATH=%%~fF"

if not exist "%DLL_PATH%" (
    echo  [ERROR] File tidak ditemukan: %DLL_PATH%
    pause
    exit /b 1
)

echo  DLL    : %DLL_PATH%
echo.

:: --- Tunggu Minecraft berjalan ---
echo  Menunggu Minecraft.Windows.exe...
:wait_mc
tasklist /FI "IMAGENAME eq Minecraft.Windows.exe" 2>nul | find /I "Minecraft.Windows.exe" >nul
if errorlevel 1 (
    timeout /t 2 /nobreak >nul
    goto :wait_mc
)

echo  [OK] Minecraft ditemukan!
echo.
echo  Menunggu game selesai load (10 detik)...
timeout /t 10 /nobreak >nul

:: --- Inject via inject.ps1 ---
echo  Menginjeksi DLL...
echo.

set "PS1_PATH=%~dp0inject.ps1"
if not exist "%PS1_PATH%" (
    echo  [ERROR] inject.ps1 tidak ditemukan di: %PS1_PATH%
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1_PATH%" -DllPath "%DLL_PATH%"

if errorlevel 1 (
    echo.
    echo  [GAGAL] Injeksi tidak berhasil.
    echo  Tips: Klik kanan auto-inject.bat lalu "Run as administrator".
    echo.
    pause
    exit /b 1
)

echo.
echo  Selesai! Tekan L di game untuk buka menu.
echo.
pause
