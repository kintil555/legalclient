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
    echo  [ERROR] Tidak ada .dll ditemukan. Usage: auto-inject.bat path\ke.dll
    pause & exit /b 1
)
:found_dll
for %%F in ("%DLL_PATH%") do set "DLL_PATH=%%~fF"
if not exist "%DLL_PATH%" ( echo  [ERROR] File tidak ada: %DLL_PATH% & pause & exit /b 1 )
echo  DLL : %DLL_PATH%
echo.

:: --- Tunggu Minecraft ---
echo  Menunggu Minecraft.Windows.exe...
:wait_mc
tasklist /FI "IMAGENAME eq Minecraft.Windows.exe" 2>nul | find /I "Minecraft.Windows.exe" >nul
if errorlevel 1 ( timeout /t 2 /nobreak >nul & goto :wait_mc )
echo  [OK] Minecraft ditemukan!
echo.
echo  Menunggu game selesai load (10 detik)...
timeout /t 10 /nobreak >nul

:: --- Generate inject.ps1 ke %TEMP% lalu jalankan ---
:: (Tidak bergantung pada file inject.ps1 di folder repo)
set "TMP_PS1=%TEMP%\lc_inject_%RANDOM%.ps1"

(
echo param^([string]$DllPath^)
echo Add-Type -MemberDefinition @"
echo [DllImport^("kernel32.dll", SetLastError=true^)] public static extern IntPtr OpenProcess^(int a, bool b, int pid^);
echo [DllImport^("kernel32.dll", SetLastError=true^)] public static extern IntPtr VirtualAllocEx^(IntPtr h, IntPtr addr, uint size, uint t, uint p^);
echo [DllImport^("kernel32.dll", SetLastError=true^)] public static extern bool WriteProcessMemory^(IntPtr h, IntPtr addr, byte[] buf, uint size, out uint w^);
echo [DllImport^("kernel32.dll", CharSet=CharSet.Ansi^)] public static extern IntPtr GetProcAddress^(IntPtr hMod, string name^);
echo [DllImport^("kernel32.dll", CharSet=CharSet.Unicode^)] public static extern IntPtr GetModuleHandleW^(string name^);
echo [DllImport^("kernel32.dll", SetLastError=true^)] public static extern IntPtr CreateRemoteThread^(IntPtr h, IntPtr a, uint s, IntPtr f, IntPtr p, uint c, out uint tid^);
echo [DllImport^("kernel32.dll"^)] public static extern uint WaitForSingleObject^(IntPtr h, uint ms^);
echo [DllImport^("kernel32.dll"^)] public static extern bool CloseHandle^(IntPtr h^);
echo "@ -Name 'LC' -Namespace 'Win32' -PassThru ^| Out-Null
echo $p = Get-Process -Name 'Minecraft.Windows' -EA SilentlyContinue
echo if ^(-not $p^) { Write-Host ' [ERROR] Proses tidak ditemukan.' -ForegroundColor Red; exit 1 }
echo Write-Host " PID: $($p.Id)" -ForegroundColor Cyan
echo $h = [Win32.LC]::OpenProcess^(0x1F0FFF, $false, $p.Id^)
echo if ^($h -eq [IntPtr]::Zero^) { Write-Host ' [ERROR] OpenProcess gagal. Jalankan sebagai Admin!' -ForegroundColor Red; exit 1 }
echo $bytes = [Text.Encoding]::Unicode.GetBytes^($DllPath + [char]0^)
echo $mem = [Win32.LC]::VirtualAllocEx^($h, [IntPtr]::Zero, $bytes.Length, 0x3000, 0x40^)
echo if ^($mem -eq [IntPtr]::Zero^) { Write-Host ' [ERROR] VirtualAllocEx gagal.' -ForegroundColor Red; [Win32.LC]::CloseHandle^($h^)^|Out-Null; exit 1 }
echo $w=0; [Win32.LC]::WriteProcessMemory^($h,$mem,$bytes,$bytes.Length,[ref]$w^)^|Out-Null
echo $ll = [Win32.LC]::GetProcAddress^([Win32.LC]::GetModuleHandleW^('kernel32.dll'^),'LoadLibraryW'^)
echo $tid=0; $ht=[Win32.LC]::CreateRemoteThread^($h,[IntPtr]::Zero,0,$ll,$mem,0,[ref]$tid^)
echo if ^($ht -eq [IntPtr]::Zero^) { Write-Host ' [ERROR] CreateRemoteThread gagal.' -ForegroundColor Red; [Win32.LC]::CloseHandle^($h^)^|Out-Null; exit 1 }
echo [Win32.LC]::WaitForSingleObject^($ht,5000^)^|Out-Null
echo [Win32.LC]::CloseHandle^($ht^)^|Out-Null; [Win32.LC]::CloseHandle^($h^)^|Out-Null
echo Write-Host ' [OK] Injeksi berhasil! Cek %APPDATA%\LegalClient\debug.log untuk status.' -ForegroundColor Green
) > "%TMP_PS1%"

echo  Menginjeksi DLL...
powershell -NoProfile -ExecutionPolicy Bypass -File "%TMP_PS1%" -DllPath "%DLL_PATH%"
set "INJECT_ERR=%errorlevel%"
del "%TMP_PS1%" 2>nul

if "%INJECT_ERR%" NEQ "0" (
    echo.
    echo  [GAGAL] Injeksi tidak berhasil.
    echo  Tips: Klik kanan auto-inject.bat lalu "Run as administrator"
    echo.
    pause & exit /b 1
)

echo.
echo  Cek log: %APPDATA%\LegalClient\debug.log
echo  Tekan L di game untuk buka menu.
echo.
pause
