@echo off
setlocal EnableDelayedExpansion

echo.
echo  =============================================
echo        LegalClient Auto Injector
echo        Minecraft Bedrock GDK Edition
echo  =============================================
echo.

:: --- Tentukan path DLL ---
set "DLL_PATH=%~1"
if "%DLL_PATH%"=="" (
    for %%F in ("%~dp0*.dll") do ( set "DLL_PATH=%%F" & goto :found_dll )
    for /r "%~dp0build" %%F in (BedrockUtilityClient.dll) do ( set "DLL_PATH=%%F" & goto :found_dll )
    for /r "%~dp0build\Release" %%F in (BedrockUtilityClient.dll) do ( set "DLL_PATH=%%F" & goto :found_dll )
    echo  [ERROR] Tidak ada .dll ditemukan.
    echo  Usage  : auto-inject.bat path\ke\BedrockUtilityClient.dll
    echo  Atau taruh .dll di folder yang sama dengan .bat ini.
    pause & exit /b 1
)
:found_dll
for %%F in ("%DLL_PATH%") do set "DLL_PATH=%%~fF"
if not exist "%DLL_PATH%" (
    echo  [ERROR] File tidak ada: %DLL_PATH%
    pause & exit /b 1
)
echo  DLL : %DLL_PATH%
echo.

:: --- Cari inject.ps1 di folder yang sama ---
set "PS1_PATH=%~dp0inject.ps1"
if not exist "%PS1_PATH%" (
    echo  [ERROR] inject.ps1 tidak ditemukan di folder yang sama dengan .bat ini.
    echo  Pastikan inject.ps1 ada di: %~dp0
    pause & exit /b 1
)

:: --- Tunggu Minecraft.Windows.exe ---
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

:: --- Inject ---
echo  Menginjeksi DLL...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1_PATH%" -DllPath "%DLL_PATH%"
set "INJECT_ERR=%errorlevel%"

if "%INJECT_ERR%" NEQ "0" (
    echo.
    echo  [GAGAL] Injeksi tidak berhasil ^(exit code: %INJECT_ERR%^).
    echo  Tips  : Klik kanan auto-inject.bat ^> "Run as administrator"
    echo  Log   : %APPDATA%\LegalClient\debug.log
    echo.
    pause & exit /b 1
)

echo.
echo  Cek log : %APPDATA%\LegalClient\debug.log
echo  Buka menu: tekan L di dalam game.
echo.
pause
