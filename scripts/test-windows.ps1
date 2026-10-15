param([ValidateSet("ui_smoke", "live_tempo_test", "system_volume_test", "volume_boost_test", "windows_audio_audit")][string]$TestName = "ui_smoke")
$ErrorActionPreference = 'Stop'
$root = '\\wsl.localhost\Ubuntu\home\rokdbest\metronome'
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = Join-Path $root ("build\" + $TestName + ".exe")
$start.WorkingDirectory = $root
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$process = [System.Diagnostics.Process]::Start($start)
$out = $process.StandardOutput.ReadToEndAsync()
$err = $process.StandardError.ReadToEndAsync()
$testTimeout = if ($TestName -eq "windows_audio_audit") { 120000 } else { 60000 }
if (-not $process.WaitForExit($testTimeout)) { $process.Kill(); throw 'The Pulse Windows test exceeded its timeout.' }
[System.IO.File]::WriteAllText((Join-Path $root ("build\windows-" + $TestName + ".log")), $out.Result + $err.Result)
Write-Output $out.Result
if ($err.Result) { Write-Output $err.Result }
if ($process.ExitCode -ne 0) { throw "Pulse UI test failed: $($process.ExitCode)" }
