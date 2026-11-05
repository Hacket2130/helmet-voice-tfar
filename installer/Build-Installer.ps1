[CmdletBinding()]
param(
    [string]$PackagePath = (Join-Path $PSScriptRoot '..\outputs\HelmetVoice-TFAR-R11.zip'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\outputs\Task-force-Vanguard-TFAR-addon-R11'),
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$project = Split-Path $PSScriptRoot -Parent
$work = Join-Path $project 'work\vanguard-installer-build'
$null = New-Item -ItemType Directory -Force -Path $work, $OutputDirectory
$package = Get-Item -LiteralPath $PackagePath
$expected = '711223E98346B869C3F544BB8CE2CF04E9563436EC62C4B1F15482A13F6919AC'
$actual = (Get-FileHash -LiteralPath $package.FullName -Algorithm SHA256).Hash
if ($actual -ne $expected -or $package.Length -ne 54021360) {
    throw 'The bundled package does not match the immutable approved R11 release.'
}
$manifest = [ordered]@{
    schemaVersion = 1
    product = 'helmet-voice-tfar'
    releaseNumber = 11
    displayVersion = 'R11'
    pluginVersion = '1.0.341-dtr.11'
    packageUrl = 'https://github.com/Hacket2130/helmet-voice-tfar/releases/download/r11/Task-force-Vanguard-TFAR-addon-R11.zip'
    sha256 = $actual.ToLowerInvariant()
    packageBytes = $package.Length
    releasePageUrl = 'https://github.com/Hacket2130/helmet-voice-tfar/releases/tag/r11'
    notes = 'Task force Vanguard TFAR addon — R11: clearer sealed-helmet speech, 623-3100 Hz band, 2 kHz resonance, +2 dB at 2500 Hz and mild distortion. No standard-helmet clicks or static. Death Trooper garble and metallic tail retained. Standard-helmet sample approved; live multiplayer acceptance remains open.'
}
$manifestPath = Join-Path $OutputDirectory 'update.json'
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x x64 compiler not found.' }
$common = @('/nologo','/langversion:5','/optimize+','/platform:x64','/warn:4','/warnaserror+',
    '/r:System.dll','/r:System.Core.dll','/r:System.Web.Extensions.dll',
    '/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll',
    "/resource:$($package.FullName),BundledRelease.zip", "/resource:$manifestPath,BundledManifest.json")
$setupPath = Join-Path $OutputDirectory 'Task-force-Vanguard-TFAR-addon-Setup-R11.exe'
& $compiler @common '/target:winexe' '/r:System.Windows.Forms.dll' '/r:System.Drawing.dll' "/win32manifest:$(Join-Path $PSScriptRoot 'app.manifest')" "/out:$setupPath" (Join-Path $PSScriptRoot 'Core.cs') (Join-Path $PSScriptRoot 'Program.cs')
if ($LASTEXITCODE -ne 0) { throw "Setup compilation failed: $LASTEXITCODE" }
if (-not $SkipTests) {
    $testExe = Join-Path $work 'HelmetVoice-Installer-Tests.exe'
    & $compiler @common '/target:exe' "/out:$testExe" (Join-Path $PSScriptRoot 'Core.cs') (Join-Path $PSScriptRoot 'Tests.cs')
    if ($LASTEXITCODE -ne 0) { throw "Test compilation failed: $LASTEXITCODE" }
    $testRoot = Join-Path $project ('work\it-' + [guid]::NewGuid().ToString('N').Substring(0,8))
    Push-Location -LiteralPath $project
    try {
        & $testExe $testRoot 2>&1 | Tee-Object -FilePath (Join-Path $work 'tests.txt')
        if ($LASTEXITCODE -ne 0) { throw "Installer tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
}
$pluginSource = Join-Path $project 'outputs\HelmetVoice-TFAR-R11\@TFAR_DTR\teamspeak\task_force_radio_dtr.ts3_plugin'
$pluginOutput = Join-Path $OutputDirectory 'Task-force-Vanguard-TFAR-addon-R11.ts3_plugin'
Copy-Item -LiteralPath $pluginSource -Destination $pluginOutput -Force
if ((Get-FileHash -LiteralPath $pluginSource).Hash -ne (Get-FileHash -LiteralPath $pluginOutput).Hash) { throw 'Plugin copy differs.' }
$packageOutput = Join-Path $OutputDirectory 'Task-force-Vanguard-TFAR-addon-R11.zip'
Copy-Item -LiteralPath $package.FullName -Destination $packageOutput -Force
if ((Get-FileHash -LiteralPath $packageOutput).Hash -ne $expected) { throw 'Branded package copy differs from approved R11.' }
$assets = @($setupPath, $pluginOutput, $packageOutput, $manifestPath)
$checksums = foreach ($asset in $assets) { '{0}  {1}' -f (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant(), [IO.Path]::GetFileName($asset) }
[IO.File]::WriteAllLines((Join-Path $OutputDirectory 'SHA256SUMS.txt'), $checksums, (New-Object Text.UTF8Encoding($false)))
$record = foreach ($asset in $assets) {
    $item = Get-Item -LiteralPath $asset
    [pscustomobject]@{ Name=$item.Name; Bytes=$item.Length; SHA256=(Get-FileHash -LiteralPath $asset).Hash }
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'artifacts.json') -Encoding UTF8
$record | Format-Table -AutoSize
