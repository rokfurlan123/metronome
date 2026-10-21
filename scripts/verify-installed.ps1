param([switch]$FlatClicks)
$ErrorActionPreference = 'Stop'
$link = Join-Path ([Environment]::GetFolderPath('Programs')) 'Pulse Metronome.lnk'
$target = (New-Object -ComObject WScript.Shell).CreateShortcut($link).TargetPath
$app = Get-Process Pulse -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $target -and $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $app) { Start-Process -FilePath $link }
for ($i=0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 200
    $app = Get-Process Pulse -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $target -and $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($app) { break }
}
if (-not $app) { throw 'Installed Pulse window did not open.' }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class PulseVerify {
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
}
'@
$h = $app.MainWindowHandle
# Open the full editor before inspecting Practice tools (widget starts by default).
[void][PulseVerify]::SendMessage($h, 0x111, [IntPtr]133, [IntPtr]::Zero)
[void][PulseVerify]::SendMessage($h, 0x111, [IntPtr]128, [IntPtr]::Zero)
$checkbox = [PulseVerify]::GetDlgItem($h,129)
if (-not [PulseVerify]::IsWindowVisible($checkbox)) { throw 'Accent checkbox is not visible.' }
if ($FlatClicks -and [PulseVerify]::SendMessage($checkbox,240,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -eq 1) {
    [void][PulseVerify]::SendMessage($checkbox,245,[IntPtr]::Zero,[IntPtr]::Zero)
}
$accentsEnabled = [PulseVerify]::SendMessage($checkbox,240,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -eq 1
if ($FlatClicks -and $accentsEnabled) { throw 'Could not disable accents in the fixed window.' }
$volumeSlider = [PulseVerify]::GetDlgItem($h,130)
if (-not [PulseVerify]::IsWindowVisible($volumeSlider)) { throw "Windows volume slider is not visible." }
$boost = [PulseVerify]::GetDlgItem($h,131)
if (-not [PulseVerify]::IsWindowVisible($boost) -or [PulseVerify]::SendMessage($boost,1026,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -ne 400) { throw "400 percent metronome slider is not available." }
$sounds = [PulseVerify]::GetDlgItem($h,111)
if ([PulseVerify]::SendMessage($sounds,326,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -ne 4) { throw "Full click sound option is missing." }
$icon = [PulseVerify]::SendMessage($h,0x7F,[IntPtr]1,[IntPtr]::Zero)
if ($icon -eq [IntPtr]::Zero) {
    # WM_GETICON can fall back to the window class icon; inspect the executable too.
    Add-Type -AssemblyName System.Drawing
    $extracted = [System.Drawing.Icon]::ExtractAssociatedIcon($target)
    if (-not $extracted) { throw 'Executable icon is missing.' }
    $extracted.Dispose()
}
$entry = Get-StartApps | Where-Object { $_.Name -eq 'Pulse Metronome' }
$older = @(Get-Process Pulse -ErrorAction SilentlyContinue | Where-Object { $_.Path -ne $target -and $_.MainWindowHandle -ne 0 } | Select-Object Id,MainWindowTitle,Path)
[pscustomobject]@{AccentsEnabled=$accentsEnabled; OlderWindows=$older; Opened=$app.Path; Title=$app.MainWindowTitle; AccentCheckboxVisible=$true; WindowsVolumeVisible=$true; MetronomeBoostMaximum=400; SoundChoices=4; StartApps=$entry; IconPresent=$true} | ConvertTo-Json -Depth 4
if (-not $entry) { throw 'Start menu search registration not yet visible.' }
