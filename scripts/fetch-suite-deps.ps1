$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$archiveName = 'mpv-dev-x86_64-20261013-git-9d144e57b6.7z'
$expectedHash = '10860837bc04491862c1e66a95dc1af1d7886d69757ce447a09e1e49ae993b52'
$sevenZip = (Get-Command 7z.exe -ErrorAction SilentlyContinue).Source
if (-not $sevenZip -and (Test-Path "$env:ProgramFiles\7-Zip\7z.exe")) {
    $sevenZip = "$env:ProgramFiles\7-Zip\7z.exe"
}
if (-not $sevenZip) { throw 'Instale o 7-Zip para extrair o libmpv.' }
New-Item -ItemType Directory -Force "$repo\build\deps","$repo\third_party\libmpv" | Out-Null
$archive = Join-Path $repo "build\deps\$archiveName"
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri "https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20261013/$archiveName" -OutFile $archive -UseBasicParsing
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'SHA-256 do libmpv não confere. O arquivo não foi extraído.'
}
& $sevenZip x $archive "-o$repo\third_party\libmpv" -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Falha ao extrair o libmpv.' }
if (-not (Test-Path "$repo\third_party\libmpv\libmpv-2.dll") -or
    -not (Test-Path "$repo\third_party\libmpv\include\mpv\client.h")) {
    throw 'O pacote extraído não contém a DLL e os headers esperados.'
}
Write-Host 'libmpv pronto em third_party\libmpv.'
