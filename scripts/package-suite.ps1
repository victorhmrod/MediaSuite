param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $SkipBuild) {
    & "$PSScriptRoot\build-suite.cmd"
    if ($LASTEXITCODE -ne 0) { throw 'Falha na compilação.' }
}
$qtDir = $env:LOCALFLIX_QT_DIR
if (-not $qtDir) { $qtDir = 'D:\Qt\6.7.3\msvc2019_64' }
$version = (Select-String -LiteralPath "$repo\CMakeLists.txt" -Pattern 'project\(MediaSuite VERSION ([0-9.]+)').Matches.Groups[1].Value
$output = Join-Path $repo "dist\MediaSuite-$version"
New-Item -ItemType Directory -Force $output | Out-Null
Copy-Item -LiteralPath "$repo\build\suite\MediaSuite.exe","$repo\build\suite\libmpv-2.dll" -Destination $output
& "$qtDir\bin\windeployqt.exe" --release --no-translations --compiler-runtime "$output\MediaSuite.exe"
if ($LASTEXITCODE -ne 0) { throw 'Falha ao incluir o Qt.' }
python -m PyInstaller --noconfirm --clean --onedir --console --name download-worker `
    --collect-all yt_dlp --collect-all yt_dlp_ejs `
    --distpath "$repo\build\suite-worker-dist" --workpath "$repo\build\suite-worker" `
    --specpath "$repo\build" "$repo\suite\download_worker.py"
if ($LASTEXITCODE -ne 0) { throw 'Falha ao empacotar o mecanismo de downloads.' }
New-Item -ItemType Directory -Force "$output\worker" | Out-Null
Copy-Item -Path "$repo\build\suite-worker-dist\download-worker\*" -Destination "$output\worker" -Recurse -Force
foreach ($tool in @('ffmpeg.exe', 'ffprobe.exe')) {
    $command = Get-Command $tool -ErrorAction SilentlyContinue
    if (-not $command) { continue }
    $toolFile = Get-Item -LiteralPath $command.Source
    if ($toolFile.LinkType) { $toolFile = $toolFile.ResolveLinkTarget($true) }
    Copy-Item -LiteralPath $toolFile.FullName -Destination "$output\$tool" -Force
    $toolLicense = Join-Path $toolFile.Directory.Parent.FullName 'LICENSE'
    if (Test-Path -LiteralPath $toolLicense) { Copy-Item -LiteralPath $toolLicense -Destination "$output\LICENSE-FFmpeg.txt" -Force }
}
Copy-Item -LiteralPath "$repo\LICENSE" -Destination "$output\LICENSE-AGPL.txt"
Copy-Item -LiteralPath "$repo\suite\vendor\localflix\LICENSE" -Destination "$output\LICENSE-LocalFlix.txt"
Copy-Item -LiteralPath "$repo\suite\vendor\magnify\LICENSE" -Destination "$output\LICENSE-MagnifyFactory.txt"
Copy-Item -LiteralPath "$repo\THIRD_PARTY_NOTICES.md","$repo\suite\README.md" -Destination $output
Compress-Archive -LiteralPath $output -DestinationPath "$repo\dist\MediaSuite-$version-win64.zip" -Force
Write-Host "Pronto: $output\MediaSuite.exe"
