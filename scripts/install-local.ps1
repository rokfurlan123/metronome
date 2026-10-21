$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'dist\Pulse.exe'
$folder = Join-Path $env:LOCALAPPDATA 'Programs\Pulse\0.3.1'
$target = Join-Path $folder 'Pulse.exe'
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
New-Item -ItemType Directory -Path $folder -Force | Out-Null
if ((Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $sourceHash) {
    $running = Get-Process -Name Pulse -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $target }
    if ($running) { throw 'Close the installed Pulse window before updating it. Your current practice has not been interrupted.' }
}
if (-not (Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $sourceHash) {
    Copy-Item -LiteralPath $source -Destination $target -Force
}
# Trust only this locally built application, without changing Windows security settings.
Unblock-File -LiteralPath $target
if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Installed executable hash does not match the build.' }
$examples = Join-Path $folder 'examples'
New-Item -ItemType Directory -Path $examples -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $root 'examples') -Filter '*.pulse' | ForEach-Object {
    $destination = Join-Path $examples $_.Name
    if (-not (Test-Path -LiteralPath $destination)) { Copy-Item -LiteralPath $_.FullName -Destination $destination }
}
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Pulse.lnk'
$startMenuPath = Join-Path ([Environment]::GetFolderPath('Programs')) 'Pulse Metronome.lnk'
$shell = New-Object -ComObject WScript.Shell
foreach ($linkPath in @($shortcutPath, $startMenuPath)) {
    $shortcut = $shell.CreateShortcut($linkPath)
    $shortcut.TargetPath = $target
    $shortcut.Arguments = ''
    $shortcut.WorkingDirectory = $folder
    $shortcut.Description = 'Pulse Metronome — tempo, rhythm and mixed meters'
    $shortcut.IconLocation = "$target,0"
    $shortcut.Save()
    Unblock-File -LiteralPath $linkPath
    $verified = $shell.CreateShortcut($linkPath)
    if ($verified.TargetPath -ne $target -or $verified.Arguments -or $verified.IconLocation -ne "$target,0") { throw "Shortcut verification failed: $linkPath" }
}
$zone = Get-Item -LiteralPath $target -Stream Zone.Identifier -ErrorAction SilentlyContinue
if ($zone) { throw 'The installed executable still has a download-zone marker.' }
[pscustomobject]@{Installed=$target; Shortcut=$shortcutPath; StartMenu=$startMenuPath; SHA256=$sourceHash; DownloadZone='None'; LaunchesFrom='Local Windows disk'} | ConvertTo-Json
