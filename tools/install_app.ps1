# Install the freshly built ZXKB.exe for D0k and keep a copy of this version for fallback.
#   pwsh tools/install_app.ps1
# The version comes from app/zxkb/res/resource.h (ZXKB_VERSION_STR). Each version is kept once, in
# Documents\Keyboard Layouts\ZXKB\versions\ZXKB_v<version>.exe; a different build under a kept number is refused.
# ZXKB is restarted through its Startup-folder shortcut, as Windows starts it.
$ErrorActionPreference = 'Stop'
$repo    = Split-Path $PSScriptRoot -Parent
$build   = Join-Path $repo 'build\ZXKB.exe'
$header  = Get-Content (Join-Path $repo 'app\zxkb\res\resource.h') -Raw
if ($header -notmatch '#define ZXKB_VERSION_STR\s+"([0-9.]+)"') { throw 'ZXKB_VERSION_STR not found' }
$version = $Matches[1]
$dir     = Join-Path $env:USERPROFILE 'Documents\Keyboard Layouts\ZXKB'
$keep    = Join-Path $dir "versions\ZXKB_v$version.exe"
New-Item -ItemType Directory -Force (Split-Path $keep) | Out-Null

# Refuse a stale build (e.g. the link failed because ZXKB.exe was running): the exe must carry this version
$built = (Get-Item $build).VersionInfo.ProductVersion
if ($built -ne $version) { throw "build\ZXKB.exe is version '$built', not ${version}: build it again (quit any ZXKB running from build\ first)" }

if (Test-Path $keep) {
    if ((Get-FileHash $keep).Hash -ne (Get-FileHash $build).Hash) {
        throw "ZXKB $version is already kept with different contents: raise ZXKB_VERSION_STR in resource.h"
    }
    "ZXKB $version is already kept, unchanged"
} else {
    Copy-Item $build $keep
    "kept: $keep"
}

Get-Process ZXKB -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1
Copy-Item $build (Join-Path $dir 'ZXKB.exe') -Force
# dfu-util writes keyboard firmware for ZXKB (GPL-2.0-or-later, from QMK MSYS); one static exe, no DLLs
$dfu = 'C:\QMK_MSYS\opt\qmk\bin\dfu-util.exe'
if ((Test-Path $dfu) -and -not (Test-Path (Join-Path $dir 'dfu-util.exe'))) { Copy-Item $dfu $dir; 'dfu-util.exe copied' }
$link = Join-Path ([Environment]::GetFolderPath('Startup')) 'ZXKB.lnk'
Start-Process explorer.exe -ArgumentList "`"$link`""
"installed ZXKB $version"
