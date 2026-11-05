param(
    [string]$TfarMod = 'C:\Program Files (x86)\Steam\steamapps\workshop\content\107410\894678801',
    [string]$ArmaTools = 'C:\Program Files (x86)\Steam\steamapps\common\Arma 3 Tools',
    [string]$TfarSource = ''
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
if (!$TfarSource) { $TfarSource = Join-Path $project 'work\tfar-upstream' }
. (Join-Path $PSScriptRoot 'Pbo.ps1')
$output = Join-Path $project 'outputs\HelmetVoice-TFAR-R11'
$mod = Join-Path $output '@TFAR_DTR'
$work = Join-Path $project 'work'
New-Item -ItemType Directory -Force -Path $work | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'Visual Studio 2022 C++ Build Tools required' }

& $msbuild (Join-Path $TfarSource 'ts\src\task_force_radio_plugin.vcxproj') /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0 /nologo /v:minimal *> (Join-Path $work 'plugin-build-console.log')
if ($LASTEXITCODE) { throw 'Plugin build failed; see work/plugin-build-console.log' }
& $msbuild (Join-Path $project 'tests\death_trooper_tests.vcxproj') /p:Configuration=Release /p:Platform=x64 "/p:TfarSource=$TfarSource" /nologo /v:minimal
if ($LASTEXITCODE) { throw 'DSP test build failed' }
& (Join-Path $work 'tests\death_trooper_tests.exe') | Tee-Object -FilePath (Join-Path $work 'dsp-tests.log')
if ($LASTEXITCODE) { throw 'DSP tests failed' }
& $msbuild (Join-Path $project 'tests\helmet_voice_tests.vcxproj') /p:Configuration=Release /p:Platform=x64 "/p:TfarSource=$TfarSource" /nologo /v:minimal
if ($LASTEXITCODE) { throw 'Helmet speaker test build failed' }
& (Join-Path $work 'tests\helmet_voice_tests.exe') | Tee-Object -FilePath (Join-Path $work 'helmet-tests.log')
if ($LASTEXITCODE) { throw 'Helmet speaker tests failed' }

New-Item -ItemType Directory -Force -Path $output,$mod,(Join-Path $output 'Source'),(Join-Path $output 'Validation'),(Join-Path $work 'keys') | Out-Null
# Only read the Workshop installation. Write the customized copy to outputs.
Copy-Item -LiteralPath (Join-Path $TfarMod 'addons') -Destination $mod -Recurse -Force
Copy-Item -LiteralPath (Join-Path $TfarMod 'keys') -Destination $mod -Recurse -Force
Copy-Item -LiteralPath (Join-Path $TfarMod 'task_force_radio_pipe.dll'),(Join-Path $TfarMod 'task_force_radio_pipe_x64.dll'),(Join-Path $TfarMod 'LICENSE.md'),(Join-Path $TfarMod 'CONTRIBUTORS.md') -Destination $mod -Force
Copy-Item -LiteralPath (Join-Path $project 'src\mod.cpp') -Destination $mod -Force

$core = Read-Pbo (Join-Path $TfarMod 'addons\tfar_core.pbo')
$position = @($core.Entries | Where-Object { $_.Name.Replace('/','\') -eq 'functions\fnc_preparePositionCoordinates.sqf' })
if ($position.Count -ne 1 -or $position[0].Method -ne 0) { throw 'Unexpected TFAR POS entry' }
$position = $position[0]
$text = [Text.Encoding]::UTF8.GetString($position.Data)
if (($text.Split('%13~').Count -ne 2) -or ($text.Split('_isSpectating, _isEnemy').Count -ne 2)) { throw 'Unrecognized TFAR POS source; review version before patching' }
$text = $text.Replace('%13~', "%13`t%14~")
$text = $text.Replace('_isSpectating, _isEnemy', "_isSpectating, _isEnemy,`n    if (isNil `"DTR_fnc_getState`") then {`"DTR2:0`"} else {[_unit] call DTR_fnc_getState}")
$position.Data = [Text.Encoding]::UTF8.GetBytes($text)
$position.Original = $position.Data.Length
Write-Pbo $core (Join-Path $mod 'addons\tfar_core.pbo')

& (Join-Path $ArmaTools 'CfgConvert\CfgConvert.exe') -bin -dst (Join-Path $work 'dtr-config.bin') (Join-Path $project 'src\addons\dtr_core\config.cpp')
if ($LASTEXITCODE) { throw 'Arma config validation failed' }
New-PboFromFolder (Join-Path $project 'src\addons\dtr_core') 'dtr\core' (Join-Path $mod 'addons\dtr_core.pbo')

# Remove only obsolete signatures for the changed copied PBO, within output root.
$addonsPath = [IO.Path]::GetFullPath((Join-Path $mod 'addons'))
foreach ($signature in Get-ChildItem -LiteralPath $addonsPath -Filter 'tfar_core.pbo.*.bisign') {
    if (!$signature.FullName.StartsWith($addonsPath + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Signature path outside package' }
    Remove-Item -LiteralPath $signature.FullName
}
$keyBase = Join-Path $work 'keys\DTR_20261105_R11'
if (!(Test-Path -LiteralPath ($keyBase + '.biprivatekey'))) {
    Push-Location (Join-Path $work 'keys')
    try { & (Join-Path $ArmaTools 'DSSignFile\DSCreateKey.exe') 'DTR_20261105_R11' }
    finally { Pop-Location }
    if ($LASTEXITCODE) { throw 'Signing key generation failed' }
}
foreach ($name in @('tfar_core','dtr_core')) {
    & (Join-Path $ArmaTools 'DSSignFile\DSSignFile.exe') ($keyBase + '.biprivatekey') (Join-Path $addonsPath ($name + '.pbo'))
    if ($LASTEXITCODE) { throw "Signing failed: $name" }
}
Copy-Item -LiteralPath ($keyBase + '.bikey') -Destination (Join-Path $mod 'keys') -Force

$tsStage = Join-Path $work 'ts-package'
New-Item -ItemType Directory -Path (Join-Path $tsStage 'plugins') -Force | Out-Null
$originalZip = [IO.Compression.ZipFile]::OpenRead((Join-Path $TfarMod 'teamspeak\task_force_radio.ts3_plugin'))
try {
    foreach ($entry in $originalZip.Entries) {
        if (!$entry.FullName.StartsWith('plugins/radio-sounds/') -or $entry.FullName.EndsWith('/')) { continue }
        $target = [IO.Path]::GetFullPath((Join-Path $tsStage $entry.FullName))
        if (!$target.StartsWith([IO.Path]::GetFullPath($tsStage) + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe archive path' }
        New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
    }
} finally { $originalZip.Dispose() }
Copy-Item -LiteralPath (Join-Path $TfarSource 'ts\src\build\TFAR_win64.dll') -Destination (Join-Path $tsStage 'plugins\TFAR_win64.dll') -Force
@'
Name = Task Force Arrowhead Radio - Helmet Voice R11
Type = Plugin
Author = TFAR authors; DTR contributors
Version = 1.0.341-dtr.11
Platforms = win64
Description = TFAR Beta with subtle LLTE helmet speakers and separate Death Trooper scrambling. Windows 64-bit TeamSpeak 3 only. Replaces the stock TFAR plugin.
'@ | Set-Content -LiteralPath (Join-Path $tsStage 'package.ini') -Encoding utf8
New-Item -ItemType Directory -Force -Path (Join-Path $mod 'teamspeak') | Out-Null
$installer = Join-Path $mod 'teamspeak\task_force_radio_dtr.ts3_plugin'
if (Test-Path -LiteralPath $installer) { Remove-Item -LiteralPath $installer }
[IO.Compression.ZipFile]::CreateFromDirectory($tsStage, $installer)

Copy-Item -LiteralPath (Join-Path $project 'src'),(Join-Path $project 'tests'),(Join-Path $project 'tools') -Destination (Join-Path $output 'Source') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $project 'PLAN.md'),(Join-Path $project 'DEVELOPMENT_LOG.md') -Destination (Join-Path $output 'Source') -Force
Copy-Item -LiteralPath (Join-Path $work 'dsp-tests.log'),(Join-Path $work 'helmet-tests.log'),(Join-Path $work 'plugin-build-console.log') -Destination (Join-Path $output 'Validation') -Force
$null = & git -C $TfarSource add -N -- ts/src/DeathTrooperEffect.hpp ts/src/HelmetVoiceEffect.hpp ts/src/VoiceChannelConversion.hpp
& git -C $TfarSource diff --binary "--output=$(Join-Path $output 'Source\tfar-dtr.patch')" -- addons/core/functions/fnc_preparePositionCoordinates.sqf ts/src
if ($LASTEXITCODE) { throw 'Cannot write source patch' }
Write-Output "Package staged at $output"
