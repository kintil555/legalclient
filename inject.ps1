# inject.ps1 — LegalClient DLL Injector (GDK Edition)
# Dipanggil oleh auto-inject.bat, jangan jalankan langsung.

param(
    [Parameter(Mandatory=$true)]
    [string]$DllPath
)

Add-Type -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr OpenProcess(int dwAccess, bool bInherit, int pid);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr VirtualAllocEx(IntPtr hProc, IntPtr addr, uint size, uint allocType, uint protect);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool WriteProcessMemory(IntPtr hProc, IntPtr addr, byte[] buf, uint size, out uint written);

[DllImport("kernel32.dll", CharSet=CharSet.Ansi)]
public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

[DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
public static extern IntPtr GetModuleHandleW(string moduleName);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr CreateRemoteThread(IntPtr hProc, IntPtr attr, uint stackSize, IntPtr startAddr, IntPtr param, uint flags, out uint threadId);

[DllImport("kernel32.dll")]
public static extern uint WaitForSingleObject(IntPtr handle, uint ms);

[DllImport("kernel32.dll")]
public static extern bool CloseHandle(IntPtr handle);
'@ -Name 'Injector' -Namespace 'Win32' -PassThru | Out-Null

$proc = Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue
if (-not $proc) {
    Write-Host ' [ERROR] Minecraft.Windows tidak ditemukan.' -ForegroundColor Red
    exit 1
}

Write-Host " [INFO] Target PID: $($proc.Id)" -ForegroundColor Cyan

$hProc = [Win32.Injector]::OpenProcess(0x1F0FFF, $false, $proc.Id)
if ($hProc -eq [IntPtr]::Zero) {
    Write-Host ' [ERROR] OpenProcess gagal. Jalankan sebagai Administrator!' -ForegroundColor Red
    exit 1
}

$bytes = [System.Text.Encoding]::Unicode.GetBytes($DllPath + [char]0)

$mem = [Win32.Injector]::VirtualAllocEx($hProc, [IntPtr]::Zero, $bytes.Length, 0x3000, 0x40)
if ($mem -eq [IntPtr]::Zero) {
    Write-Host ' [ERROR] VirtualAllocEx gagal.' -ForegroundColor Red
    [Win32.Injector]::CloseHandle($hProc) | Out-Null
    exit 1
}

$written = 0
[Win32.Injector]::WriteProcessMemory($hProc, $mem, $bytes, $bytes.Length, [ref]$written) | Out-Null

$loadLib = [Win32.Injector]::GetProcAddress(
    [Win32.Injector]::GetModuleHandleW('kernel32.dll'),
    'LoadLibraryW'
)

$tid = 0
$hThread = [Win32.Injector]::CreateRemoteThread($hProc, [IntPtr]::Zero, 0, $loadLib, $mem, 0, [ref]$tid)
if ($hThread -eq [IntPtr]::Zero) {
    Write-Host ' [ERROR] CreateRemoteThread gagal.' -ForegroundColor Red
    [Win32.Injector]::CloseHandle($hProc) | Out-Null
    exit 1
}

[Win32.Injector]::WaitForSingleObject($hThread, 5000) | Out-Null
[Win32.Injector]::CloseHandle($hThread) | Out-Null
[Win32.Injector]::CloseHandle($hProc)  | Out-Null

Write-Host ' [OK] Injeksi berhasil! Tunggu overlay muncul di game.' -ForegroundColor Green
