@echo off
:: kill-mc.bat — Paksa tutup Minecraft Bedrock GDK
echo  Menutup Minecraft.Windows.exe...
taskkill /IM Minecraft.Windows.exe /F /FI "STATUS eq RUNNING" 2>nul
if errorlevel 1 (
    echo  [INFO] Minecraft tidak sedang berjalan.
) else (
    echo  [OK] Minecraft ditutup.
)
