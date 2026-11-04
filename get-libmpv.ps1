# Downloads the pinned LGPL build of libmpv and puts libmpv-2.dll next to meowplayer.exe.
# Run in PowerShell:  powershell -ExecutionPolicy Bypass -File get-libmpv.ps1
# Needs 7-Zip (winget install 7zip.7zip).
$ErrorActionPreference = "Stop"
$url  = "https://github.com/zhongfly/mpv-winbuild/releases/download/2026-11-01-add72571ea/mpv-dev-lgpl-x86_64-20261101-git-add72571ea.7z"
$sha  = "a05f80aa36faecc25af840dca3a24af0cd32fa42a114bf7ea91dc21a80376f6c"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tmp  = Join-Path $env:TEMP "libmpv-meow"
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$archive = Join-Path $tmp "libmpv.7z"

Write-Host "Downloading libmpv (about 29 MB)..."
Invoke-WebRequest -Uri $url -OutFile $archive

$hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLower()
if ($hash -ne $sha) { throw "Checksum mismatch! Expected $sha but got $hash. Not installing." }
Write-Host "Checksum OK."

$sevenZip = @("$env:ProgramFiles\7-Zip\7z.exe", "${env:ProgramFiles(x86)}\7-Zip\7z.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sevenZip) { $sevenZip = (Get-Command 7z -ErrorAction SilentlyContinue).Source }
if (-not $sevenZip) { throw "7-Zip not found. Install it with: winget install 7zip.7zip" }

& $sevenZip x -y "-o$tmp\x" $archive "libmpv-2.dll" | Out-Null
Copy-Item "$tmp\x\libmpv-2.dll" (Join-Path $here "libmpv-2.dll") -Force
Write-Host "Done: libmpv-2.dll is now next to meowplayer.exe"
