param([switch]$SkipPackage, [switch]$SkipTest)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = (Select-String -LiteralPath "$repo\CMakeLists.txt" -Pattern 'project\(MediaSuite VERSION ([0-9.]+)').Matches.Groups[1].Value
if (-not $SkipPackage) {
    & "$PSScriptRoot\package-suite.ps1"
    if ($LASTEXITCODE -ne 0) { throw 'Application packaging failed.' }
}
$compiler = $env:MEDIA_SUITE_ISCC
if (-not $compiler) { $compiler = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path -LiteralPath $compiler)) { $compiler = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Inno Setup 6 was not found. Set MEDIA_SUITE_ISCC to ISCC.exe.' }
$package = Join-Path $repo "dist\MediaSuite-$version"
& $compiler /Qp "/DAppVersion=$version" "/DSourceDir=$package" "$repo\installer\MediaSuite.iss"
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
$installer = Join-Path $repo "dist\MediaSuite-$version-setup.exe"
$portable = Join-Path $repo "dist\MediaSuite-$version-win64.zip"
$checksums = foreach ($asset in $installer, $portable) {
    "$((Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant())  $(Split-Path -Leaf $asset)"
}
$checksums | Set-Content -LiteralPath "$repo\dist\SHA256SUMS.txt" -Encoding ascii
if (-not $SkipTest) {
    & "$PSScriptRoot\test-suite-installer.ps1" -Installer $installer
}
Write-Host "Release assets ready in $repo\dist."
