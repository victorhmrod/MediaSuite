param([Parameter(Mandatory=$true)][string]$Installer)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$uninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{BF48B7AD-D1F1-45ED-AB6A-8DF807F66CAB}_is1'
foreach ($hive in 'HKCU', 'HKLM') {
    if (Test-Path "${hive}:\$uninstallKey") { throw 'Media Suite is already installed. Installer testing will not overwrite it.' }
}
$testRoot = Join-Path $repo ('build\installer-smoke-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$installDir = Join-Path $testRoot 'app'
$previousDataDir = $env:LOCALFLIX_DATA_DIR
$previousSettingsOrganization = $env:MEDIA_SUITE_SETTINGS_ORGANIZATION
$previousPath = $env:PATH
try {
    $install = Start-Process -FilePath $Installer -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', "/DIR=`"$installDir`"", "/LOG=`"$testRoot\install.log`"" -WindowStyle Hidden -PassThru -Wait
    if ($install.ExitCode -ne 0) { throw "Installer failed with exit code $($install.ExitCode)." }
    foreach ($file in 'MediaSuite.exe', 'worker\download-worker.exe', 'libmpv-2.dll', 'unins000.exe', 'LICENSE-AGPL.txt') {
        if (-not (Test-Path (Join-Path $installDir $file))) { throw "Installed package is missing $file." }
    }
    if (-not (Test-Path "HKCU:\$uninstallKey")) { throw 'The uninstall registry entry was not created.' }
    $env:LOCALFLIX_DATA_DIR = Join-Path $testRoot 'library'
    $env:MEDIA_SUITE_SETTINGS_ORGANIZATION = 'MediaSuiteInstallerTest'
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $run = Start-Process -FilePath "$installDir\MediaSuite.exe" -ArgumentList '--downloads', '--screenshot', "`"$testRoot\installed.png`"" -WindowStyle Hidden -PassThru -Wait
    if ($run.ExitCode -ne 0 -or -not (Test-Path "$testRoot\installed.png")) { throw 'The installed application failed its startup smoke test.' }
} finally {
    $env:LOCALFLIX_DATA_DIR = $previousDataDir
    $env:MEDIA_SUITE_SETTINGS_ORGANIZATION = $previousSettingsOrganization
    $env:PATH = $previousPath
    if (Test-Path "$installDir\unins000.exe") {
        $uninstall = Start-Process -FilePath "$installDir\unins000.exe" -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -WindowStyle Hidden -PassThru -Wait
        if ($uninstall.ExitCode -ne 0) { throw "Uninstaller failed with exit code $($uninstall.ExitCode)." }
    }
}
for ($attempt = 0; $attempt -lt 40 -and (Test-Path "$installDir\MediaSuite.exe"); $attempt++) { Start-Sleep -Milliseconds 250 }
if (Test-Path "$installDir\MediaSuite.exe") { throw 'Uninstall left the application executable behind.' }
if (Test-Path "HKCU:\$uninstallKey") { throw 'Uninstall left its registry entry behind.' }
Write-Host "Installer verified: install, startup, and uninstall. Logs: $testRoot"
