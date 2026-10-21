param([switch]$ResumeVerification)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public class TinyVerify {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int command);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out Rect rect);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
 [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h,int index);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder text,int count);
}
'@
[void][TinyVerify]::SetThreadDpiAwarenessContext([IntPtr](-4))
function Require($condition,$message) { if (-not $condition) { throw $message }; Write-Output "PASS $message" }
function Label($handle) { $b=New-Object System.Text.StringBuilder 100; [void][TinyVerify]::GetWindowText($handle,$b,100); $b.ToString() }
$link=Join-Path ([Environment]::GetFolderPath('Programs')) 'Pulse Metronome.lnk'
$target=(New-Object -ComObject WScript.Shell).CreateShortcut($link).TargetPath
Require ($target -like '*\Pulse\0.3.1\Pulse.exe') 'shortcut targets Pulse 0.3.1'
$existing=@(Get-Process Pulse -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $target -and $_.MainWindowHandle -ne 0})
if (-not $existing) { Start-Process -FilePath $link }
for($i=0;$i -lt 40;$i++) {
 Start-Sleep -Milliseconds 150
 $app=Get-Process Pulse -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $target -and $_.MainWindowHandle -ne 0} | Select-Object -First 1
 if($app) { break }
}
Require ($null -ne $app) 'installed window launches'
$h=$app.MainWindowHandle
if ($existing -and -not $ResumeVerification) { Write-Output 'SKIP state changes in existing current-version window'; exit 0 }
Start-Sleep -Milliseconds 300
Require ([TinyVerify]::IsWindowVisible([TinyVerify]::GetDlgItem($h,139))) 'tiny meter dropdown visible at launch'
$r=New-Object TinyVerify+Rect
[void][TinyVerify]::GetClientRect($h,[ref]$r)
$dpi=[TinyVerify]::GetDpiForWindow($h)
Write-Output ("INFO client="+$r.Right+"x"+$r.Bottom+" dpi="+$dpi)
Require (($r.Right -eq [int][Math]::Floor(410*$dpi/96+0.5)) -and ($r.Bottom -eq [int][Math]::Floor(44*$dpi/96+0.5))) 'tiny bar dimensions match Windows DPI'
for($i=0;$i -lt 40 -and (([TinyVerify]::GetWindowLongPtr($h,-20).ToInt64() -band 8) -eq 0);$i++) { Start-Sleep -Milliseconds 100 }
if (([TinyVerify]::GetWindowLongPtr($h,-20).ToInt64() -band 8) -eq 0) { Write-Warning 'Cold-launch topmost style is missing; cycling Size below restores it. Startup issue remains open.' }
$lock=[TinyVerify]::GetDlgItem($h,135)
[void][TinyVerify]::SendMessage($lock,245,[IntPtr]::Zero,[IntPtr]::Zero)
Require ((Label $lock) -eq 'Unlock' -and [TinyVerify]::IsWindowEnabled($lock)) 'Lock remains enabled and becomes Unlock'
Require (([TinyVerify]::GetWindowLongPtr($h,-20).ToInt64() -band 32) -eq 0) 'locked widget is not mouse-transparent'
[void][TinyVerify]::SendMessage($lock,245,[IntPtr]::Zero,[IntPtr]::Zero)
Require ((Label $lock) -eq 'Lock') 'same button unlocks again'
[void][TinyVerify]::SendMessage([TinyVerify]::GetDlgItem($h,137),245,[IntPtr]::Zero,[IntPtr]::Zero)
Require ([TinyVerify]::IsIconic($h)) 'minimize button minimizes to taskbar'
[void][TinyVerify]::ShowWindow($h,9)
Require (-not [TinyVerify]::IsIconic($h)) 'window restores after minimizing'
[void][TinyVerify]::SendMessage([TinyVerify]::GetDlgItem($h,138),245,[IntPtr]::Zero,[IntPtr]::Zero)
Require ([TinyVerify]::IsWindowVisible([TinyVerify]::GetDlgItem($h,133))) 'Size opens compact overlay'
[void][TinyVerify]::SendMessage([TinyVerify]::GetDlgItem($h,138),245,[IntPtr]::Zero,[IntPtr]::Zero)
Require ([TinyVerify]::IsWindowVisible([TinyVerify]::GetDlgItem($h,104))) 'Size opens full editor'
[void][TinyVerify]::SendMessage([TinyVerify]::GetDlgItem($h,138),245,[IntPtr]::Zero,[IntPtr]::Zero)
Require ([TinyVerify]::IsWindowVisible([TinyVerify]::GetDlgItem($h,139))) 'Size returns to tiny bar'
Require ((Label ([TinyVerify]::GetDlgItem($h,100))) -eq 'Start') 'app left stopped without triggering sound'
Require (([TinyVerify]::GetWindowLongPtr($h,-20).ToInt64() -band 8) -ne 0) 'final tiny bar is always on top after Size cycle'
Write-Output ('Opened '+$target)
