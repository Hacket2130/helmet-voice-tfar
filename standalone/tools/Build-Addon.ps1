[CmdletBinding()]
param([string]$ArmaTools='C:\Program Files (x86)\Steam\steamapps\common\Arma 3 Tools')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$standalone=Split-Path $PSScriptRoot -Parent
$project=Split-Path $standalone -Parent
$work=Join-Path $project 'work\standalone-package'
$output=Join-Path $project 'outputs\Task-force-Vanguard-TFAR-addon-R12'
if(Test-Path -LiteralPath $output){throw 'R12 staging already exists; preserve it and choose a new candidate before rebuilding.'}
$null=New-Item -ItemType Directory -Force -Path $work
$msbuild='D:\DeveloperTools\VisualStudio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
if(!(Test-Path -LiteralPath $msbuild)){
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
if(!$msbuild){throw 'Visual Studio C++ Build Tools required.'}
. (Join-Path $PSScriptRoot 'Pbo.ps1')
$dspHashes=@{
    'DeathTrooperEffect.hpp'='44F3CC4CF71EA0599990360B3EE33BA454389EA988ADBFA449C57839D32DF1EB'
    'HelmetVoiceEffect.hpp'='601656B0BF67CF331D395D26E8A5604ACE9E7BF21A1F276B4B313EA5A753CEA5'
}
foreach($name in $dspHashes.Keys){if((Get-FileHash -LiteralPath (Join-Path $standalone ('native\dsp\'+$name))).Hash -ne $dspHashes[$name]){throw "Approved DSP changed: $name"}}
foreach($name in @('bridge','plugin','companion_tests','bridge_tests')){
    & $msbuild (Join-Path $standalone ('native\'+$name+'.vcxproj')) /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal 2>&1 | Tee-Object -FilePath (Join-Path $work ($name+'-build.txt'))
    if($LASTEXITCODE){throw "Native build failed: $name"}
}
$native=Join-Path $project 'work\standalone-native'
& (Join-Path $native 'bridge_tests.exe') (Join-Path $native 'tfv_helmet_voice_x64.dll') 2>&1 | Tee-Object -FilePath (Join-Path $work 'bridge-tests.txt')
if($LASTEXITCODE){throw 'Bridge tests failed'}
& (Join-Path $native 'companion_tests.exe') (Join-Path $native 'task_force_vanguard_win64.dll') (Join-Path $native 'tfv_helmet_voice_x64.dll') 2>&1 | Tee-Object -FilePath (Join-Path $work 'companion-tests.txt')
if($LASTEXITCODE){throw 'Companion tests failed'}
foreach($name in @('death_trooper_tests','helmet_voice_tests')){
    & $msbuild (Join-Path $standalone ('tests\'+$name+'.vcxproj')) /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal 2>&1 | Tee-Object -FilePath (Join-Path $work ($name+'-build.txt'))
    if($LASTEXITCODE){throw "DSP test build failed: $name"}
    & (Join-Path $project ('work\standalone-dsp\'+$name+'.exe')) 2>&1 | Tee-Object -FilePath (Join-Path $work ($name+'.txt'))
    if($LASTEXITCODE){throw "DSP tests failed: $name"}
}
$mod=Join-Path $output '@TFV_TFAR_Addon'
$null=New-Item -ItemType Directory -Force -Path $mod,(Join-Path $mod 'addons'),(Join-Path $mod 'keys'),(Join-Path $mod 'teamspeak'),(Join-Path $output 'Validation')
Copy-Item -LiteralPath (Join-Path $standalone 'arma\mod.cpp'),(Join-Path $standalone 'LICENSE.md'),(Join-Path $standalone 'README.md') -Destination $mod
Copy-Item -LiteralPath (Join-Path $native 'tfv_helmet_voice_x64.dll') -Destination $mod
& (Join-Path $ArmaTools 'CfgConvert\CfgConvert.exe') -bin -dst (Join-Path $work 'tfv-config.bin') (Join-Path $standalone 'arma\addons\tfv_core\config.cpp')
if($LASTEXITCODE){throw 'Config conversion failed'}
$pbo=Join-Path $mod 'addons\tfv_core.pbo'
New-PboFromFolder (Join-Path $standalone 'arma\addons\tfv_core') 'tfv\core' $pbo
$keyRoot=Join-Path $project 'work\keys'
$null=New-Item -ItemType Directory -Force -Path $keyRoot
$keyBase=Join-Path $keyRoot 'TFV_20261105_R12'
if(!(Test-Path -LiteralPath ($keyBase+'.biprivatekey'))){
    Push-Location -LiteralPath $keyRoot
    try{& (Join-Path $ArmaTools 'DSSignFile\DSCreateKey.exe') 'TFV_20261105_R12'; if($LASTEXITCODE){throw 'Key creation failed'}}finally{Pop-Location}
}
& (Join-Path $ArmaTools 'DSSignFile\DSSignFile.exe') ($keyBase+'.biprivatekey') $pbo
if($LASTEXITCODE){throw 'PBO signing failed'}
Copy-Item -LiteralPath ($keyBase+'.bikey') -Destination (Join-Path $mod 'keys')
& (Join-Path $ArmaTools 'DSSignFile\DSCheckSignatures.exe') (Join-Path $mod 'addons') (Join-Path $mod 'keys') 2>&1 | Tee-Object -FilePath (Join-Path $work 'signatures.txt')
if($LASTEXITCODE){throw 'PBO signature check failed'}
$ts=Join-Path $work ('ts-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$null=New-Item -ItemType Directory -Force -Path (Join-Path $ts 'plugins')
Copy-Item -LiteralPath (Join-Path $native 'task_force_vanguard_win64.dll') -Destination (Join-Path $ts 'plugins')
@'
Name = Task force Vanguard TFAR addon
Type = Plugin
Author = Vanguard contributors
Version = 0.12.0
Platforms = win64
Description = Standalone helmet voice companion. Keep the official TFAR Beta TeamSpeak plugin enabled alongside this plugin. Does not replace TFAR. Requires matching Vanguard Arma addon, CBA and stock TFAR Beta.
'@ | Set-Content -LiteralPath (Join-Path $ts 'package.ini') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($ts,(Join-Path $mod 'teamspeak\task_force_vanguard.ts3_plugin'))
$manifest=@(Get-ChildItem -LiteralPath $mod -File -Recurse | ForEach-Object {[ordered]@{Path=$_.FullName.Substring($mod.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash;Bytes=$_.Length}})
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'Validation\manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $standalone 'README.md'),(Join-Path $standalone 'PLAN.md'),(Join-Path $standalone 'DEVELOPMENT_LOG.md') -Destination $output
Get-ChildItem -LiteralPath $work -File -Filter '*.txt' | Copy-Item -Destination (Join-Path $output 'Validation')
if(@(Get-ChildItem -LiteralPath $mod -File -Recurse | Where-Object {$_.Name -match '^(tfar_core\.pbo|TFAR_win64\.dll|task_force_radio_pipe.*\.dll)$'}).Count){throw 'Stock TFAR file collision'}
Write-Output "Standalone package staged: $output"
Write-Output 'Review source/docs and seal the ZIP separately; no live installation performed.'
