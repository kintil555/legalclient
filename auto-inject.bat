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
    :: Cari DLL di folder yang sama dengan .bat ini
    for %%F in ("%~dp0*.dll") do (
        set "DLL_PATH=%%F"
        goto :found_dll
    )
    :: Cari di subfolder build
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
:: Resolusi ke absolute path
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

:: --- Inject via PowerShell ---
echo  Menginjeksi DLL...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$dllPath = '%DLL_PATH%';" ^
    "$proc = Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue;" ^
    "if (-not $proc) { Write-Host ' [ERROR] Minecraft.Windows tidak ditemukan.' -ForegroundColor Red; exit 1; }" ^
    "$hProc = [System.Runtime.InteropServices.Marshal]::GetIUnknownForObject($proc);" ^
    "$k32 = Add-Type -MemberDefinition @'" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern IntPtr OpenProcess(int a, bool b, int c);" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern IntPtr VirtualAllocEx(IntPtr h, IntPtr a, uint s, uint t, uint p);" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern bool WriteProcessMemory(IntPtr h, IntPtr a, byte[] b, uint s, out uint w);" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern IntPtr GetProcAddress(IntPtr m, string p);" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern IntPtr GetModuleHandle(string m);" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern IntPtr CreateRemoteThread(IntPtr h, IntPtr a, uint s, IntPtr f, IntPtr p, uint c, out uint id);" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern uint WaitForSingleObject(IntPtr h, uint ms);" ^
    "[DllImport(\"\"kernel32.dll\"\")] public static extern bool CloseHandle(IntPtr h);" ^
    "'@ -Name 'K32' -Namespace 'Win32' -PassThru;" ^
    "$hProc = [Win32.K32]::OpenProcess(0x1F0FFF, $false, $proc.Id);" ^
    "if ($hProc -eq [IntPtr]::Zero) { Write-Host ' [ERROR] Gagal OpenProcess. Jalankan sebagai Administrator!' -ForegroundColor Red; exit 1; }" ^
    "$bytes = [System.Text.Encoding]::Unicode.GetBytes($dllPath + [char]0);" ^
    "$mem = [Win32.K32]::VirtualAllocEx($hProc, [IntPtr]::Zero, $bytes.Length, 0x3000, 0x40);" ^
    "if ($mem -eq [IntPtr]::Zero) { Write-Host ' [ERROR] VirtualAllocEx gagal.' -ForegroundColor Red; exit 1; }" ^
    "$written = 0; [Win32.K32]::WriteProcessMemory($hProc, $mem, $bytes, $bytes.Length, [ref]$written) | Out-Null;" ^
    "$loadLib = [Win32.K32]::GetProcAddress([Win32.K32]::GetModuleHandle('kernel32.dll'), 'LoadLibraryW');" ^
    "$tid = 0; $hThread = [Win32.K32]::CreateRemoteThread($hProc, [IntPtr]::Zero, 0, $loadLib, $mem, 0, [ref]$tid);" ^
    "if ($hThread -eq [IntPtr]::Zero) { Write-Host ' [ERROR] CreateRemoteThread gagal.' -ForegroundColor Red; exit 1; }" ^
    "[Win32.K32]::WaitForSingleObject($hThread, 5000) | Out-Null;" ^
    "[Win32.K32]::CloseHandle($hThread) | Out-Null;" ^
    "[Win32.K32]::CloseHandle($hProc) | Out-Null;" ^
    "Write-Host ' [OK] Injeksi berhasil! Tunggu overlay muncul di game.' -ForegroundColor Green;"

if errorlevel 1 (
    echo.
    echo  [GAGAL] Injeksi tidak berhasil.
    echo  Tips: Coba jalankan .bat ini sebagai Administrator.
    echo.
    pause
    exit /b 1
)

echo.
echo  Selesai! Tekan L di game untuk buka menu.
echo.
pause
