@echo off
rem cursorpos.bat - live Windows cursor position (physical px) for mouse_goto calibration.
rem Double-click to run, Ctrl+C to stop. See README "Calibrating with the Position Finder".
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (((Get-Content -LiteralPath '%~f0') | Select-Object -Skip 5) -join [Environment]::NewLine)"
exit /b
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Mouse {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  public struct POINT { public int X; public int Y; }
  public static string Get() { POINT p; GetCursorPos(out p); return p.X + "," + p.Y; }
}
"@
[void][Mouse]::SetProcessDPIAware()
Write-Host "Cursor position (physical px) - fire mouse_goto, read the value. Ctrl+C to stop." -ForegroundColor Cyan
while ($true) { Write-Host ("`r" + [Mouse]::Get() + "        ") -NoNewline; Start-Sleep -Milliseconds 100 }
