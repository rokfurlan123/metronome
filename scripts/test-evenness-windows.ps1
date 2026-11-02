param([ValidatePattern('^build/[A-Za-z0-9_-]+$')][string]$OutputDirectory='build/native-evenness', [switch]$Live)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$start=New-Object System.Diagnostics.ProcessStartInfo
$start.FileName=Join-Path $root 'build\windows_evenness_audit.exe'
$start.WorkingDirectory=$root
$start.Arguments=$OutputDirectory + $(if($Live){' --live-only'}else{''})
$start.UseShellExecute=$false
$start.CreateNoWindow=$true
$start.RedirectStandardOutput=$true
$start.RedirectStandardError=$true
Write-Output 'Testing Pulse audio only, at reduced test-session volume. No microphone/system-wide capture or master-volume changes.'
$p=[System.Diagnostics.Process]::Start($start)
$out=$p.StandardOutput.ReadToEndAsync();$err=$p.StandardError.ReadToEndAsync()
if(-not $p.WaitForExit(240000)){$p.Kill();throw 'This audit process exceeded its timeout.'}
[System.IO.File]::WriteAllText((Join-Path $root ($OutputDirectory+'.log')),$out.Result+$err.Result)
Write-Output $out.Result
if($err.Result){Write-Output $err.Result}
if($p.ExitCode -ne 0){throw "Pulse audio capture failed: $($p.ExitCode)"}
