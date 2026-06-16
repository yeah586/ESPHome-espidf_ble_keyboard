@echo off
rem cursor_saverestore.bat - save/restore the real Windows cursor position.
rem Usage: "cursor_saverestore.bat save"  and  "cursor_saverestore.bat restore".
rem Uses GetCursorPos/SetCursorPos (physical px, DPI-aware - matches cursorpos.bat
rem and mouse_goto). No BLE/ESP32 link; bind it to a Windows shortcut key and fire
rem the combo from the keyboard. See README "Saving and restoring the cursor".
set "BLEKB_MODE=%~1"
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (((Get-Content -LiteralPath '%~f0') | Select-Object -Skip 9) -join [Environment]::NewLine)"
exit /b
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Mouse {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
  public struct POINT { public int X; public int Y; }
  public static string Save() { POINT p; GetCursorPos(out p); return p.X + "," + p.Y; }
  public static void Restore(int x, int y) { SetCursorPos(x, y); }
}
"@
[void][Mouse]::SetProcessDPIAware()
$f = Join-Path $env:TEMP 'blekb_cursor.txt'
switch ($env:BLEKB_MODE) {
  'save'    { [Mouse]::Save() | Set-Content -LiteralPath $f; Write-Host ('saved ' + (Get-Content -LiteralPath $f)) -ForegroundColor Cyan }
  'restore' { if (Test-Path -LiteralPath $f) { $v = ((Get-Content -LiteralPath $f -Raw).Trim()) -split ','; [Mouse]::Restore([int]($v[0].Trim()), [int]($v[1].Trim())); Write-Host ('restored ' + $v[0] + ',' + $v[1]) -ForegroundColor Cyan } else { Write-Host 'no saved position yet' -ForegroundColor Yellow } }
  default   { Write-Host 'usage: cursor_saverestore.bat save | restore' -ForegroundColor Yellow }
}
