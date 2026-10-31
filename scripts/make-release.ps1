<#
.SYNOPSIS
  Builds every release asset into dist\:
    YTDownloader-<version>-setup.exe     Windows installer (Inno Setup, per user)
    YTDownloader-<version>-portable.exe  single-file executable, no installation
    SHA256SUMS.txt

.DESCRIPTION
  1. install/upgrade the Python build dependencies and generate the icon
  2. PyInstaller: a folder build for the installer (fast start) and a one-file portable build
  3. compile installer\YTDownloader.iss with Inno Setup 6 (winget install JRSoftware.InnoSetup;
     override the compiler with $env:YTDL_ISCC)
  4. smoke test the portable exe, then test the installer for real: silent per-user install into a
     temporary folder, start the installed exe, silent uninstall, check that everything was removed.

.PARAMETER SkipDeps
  Do not run pip.
.PARAMETER SkipTest
  Do not run the smoke and install/uninstall tests.
#>
param([switch]$SkipDeps, [switch]$SkipTest)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$app = Join-Path $root 'yt_downloader.py'
$version = [regex]::Match((Get-Content $app -Raw), '(?m)^APP_VERSION = "([0-9.]+)"').Groups[1].Value
if (-not $version) { throw 'APP_VERSION not found in yt_downloader.py' }
$icon = Join-Path $root 'assets\icon.ico'
$work = Join-Path $root 'build\pyinstaller'
$dist = Join-Path $root 'dist'
$setup = Join-Path $dist "YTDownloader-$version-setup.exe"
$portable = Join-Path $dist "YTDownloader-$version-portable.exe"
$appDir = Join-Path $work 'dist\YT Downloader'
Write-Host "== YT Downloader $version"

function Find-Iscc {
    if ($env:YTDL_ISCC -and (Test-Path $env:YTDL_ISCC)) { return $env:YTDL_ISCC }
    foreach ($p in "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") {
        if (Test-Path $p) { return $p }
    }
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw 'Inno Setup 6 (ISCC.exe) not found. Install it (winget install JRSoftware.InnoSetup) or set YTDL_ISCC.'
}
$iscc = Find-Iscc

function Invoke-Checked([string]$what, [scriptblock]$block) {
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" }
}

if (-not $SkipDeps) {
    Write-Host '== installing build dependencies'
    Invoke-Checked 'pip' { python -m pip install -U --quiet --disable-pip-version-check -r (Join-Path $root 'requirements.txt') pyinstaller pillow }
}
Invoke-Checked 'icon' { python (Join-Path $root 'tools\make_icon.py') }

$common = @('--noconfirm', '--clean', '--windowed', '--icon', $icon, '--add-data', "$icon;assets",
            '--collect-data', 'yt_dlp_ejs', '--exclude-module', 'tkinter',
            '--workpath', (Join-Path $work 'work'), '--specpath', $work)

Write-Host '== PyInstaller: folder build (installer)'
Invoke-Checked 'PyInstaller (folder)' { python -m PyInstaller @common --onedir --name 'YT Downloader' --distpath (Join-Path $work 'dist') $app }
Write-Host '== PyInstaller: portable one-file build'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Invoke-Checked 'PyInstaller (portable)' { python -m PyInstaller @common --onefile --name "YTDownloader-$version-portable" --distpath $dist $app }

Write-Host "== compiling the installer ($iscc)"
if (Test-Path $setup) { Remove-Item -Force $setup }
Invoke-Checked 'Inno Setup' { & $iscc /Qp "/DAppVersion=$version" "/DSourceDir=$appDir" (Join-Path $root 'installer\YTDownloader.iss') | Out-Host }
if (-not (Test-Path $setup)) { throw 'Inno Setup did not produce the installer' }

$sums = foreach ($f in $setup, $portable) { "$((Get-FileHash $f -Algorithm SHA256).Hash.ToLower())  $(Split-Path -Leaf $f)" }
Set-Content -Path (Join-Path $dist 'SHA256SUMS.txt') -Value $sums -Encoding ascii
foreach ($f in $setup, $portable) { Write-Host ("   {0} ({1:N1} MB)" -f (Split-Path -Leaf $f), ((Get-Item $f).Length / 1MB)) }

if ($SkipTest) { return }

Write-Host '== smoke test: portable exe'
$run = Start-Process -FilePath $portable -ArgumentList '--smoke-test' -PassThru -Wait
if ($run.ExitCode -ne 0) { throw "portable exe exited with $($run.ExitCode)" }
Write-Host '   portable exe starts and exits cleanly'

Write-Host '== testing the installer (silent, per user, temporary folder)'
$appId = [regex]::Match((Get-Content (Join-Path $root 'installer\YTDownloader.iss') -Raw), '(?m)^AppId=\{(\{[0-9A-Fa-f-]+\})').Groups[1].Value
if (-not $appId) { throw 'could not read AppId from installer\YTDownloader.iss' }
$keyPath = "Software\Microsoft\Windows\CurrentVersion\Uninstall\${appId}_is1"
# The test installs under the real AppId, so it must not run over a genuine installation.
foreach ($hive in 'HKCU', 'HKLM') {
    if (Test-Path "${hive}:\$keyPath") { throw "YT Downloader is already installed on this PC ($hive). Uninstall it first, or run with -SkipTest." }
}
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("ytdl-install-test-" + [guid]::NewGuid().ToString('N'))
$installDir = Join-Path $tmp 'app'
$log = Join-Path $tmp 'install.log'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$installed = $false
try {
    $p = Start-Process -FilePath $setup -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', "/DIR=`"$installDir`"", "/LOG=`"$log`"" -PassThru -Wait
    $installed = $true
    if ($p.ExitCode -ne 0) { throw "installer exited with $($p.ExitCode); see $log" }
    foreach ($f in 'YT Downloader.exe', 'unins000.exe', 'LICENSE', 'THIRD_PARTY_NOTICES.md', '_internal\assets\icon.ico') {
        if (-not (Test-Path (Join-Path $installDir $f))) { throw "installed folder is missing $f" }
    }
    $key = Get-Item "HKCU:\$keyPath" -ErrorAction SilentlyContinue
    if (-not $key -or $key.GetValue('DisplayName') -notlike 'YT Downloader*') { throw 'no YT Downloader entry in Apps & features' }
    Write-Host "   installed OK: $($key.GetValue('DisplayName')), uninstall entry registered"

    $run = Start-Process -FilePath (Join-Path $installDir 'YT Downloader.exe') -ArgumentList '--smoke-test' -PassThru -Wait
    if ($run.ExitCode -ne 0) { throw "installed application exited with $($run.ExitCode)" }
    Write-Host '   installed application starts and exits cleanly'

    $u = Start-Process -FilePath (Join-Path $installDir 'unins000.exe') -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -PassThru -Wait
    if ($u.ExitCode -ne 0) { throw "uninstaller exited with $($u.ExitCode)" }
    for ($i = 0; $i -lt 40 -and (Test-Path (Join-Path $installDir 'YT Downloader.exe')); $i++) { Start-Sleep -Milliseconds 250 }
    if (Test-Path (Join-Path $installDir 'YT Downloader.exe')) { throw 'uninstall left the executable behind' }
    if (Test-Path "HKCU:\$keyPath") { throw 'uninstall left its registry entry behind' }
    Write-Host '   uninstall OK: program and registry entry removed'
} finally {
    # a failed test must not leave a half-installed copy (and its Apps & features entry) behind
    $uninstaller = Join-Path $installDir 'unins000.exe'
    if ($installed -and (Test-Path $uninstaller)) {
        Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait | Out-Null
    }
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
Write-Host 'Release build and tests passed.'
