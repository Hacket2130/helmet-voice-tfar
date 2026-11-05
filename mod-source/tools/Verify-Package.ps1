param([string]$TfarMod = 'C:\Program Files (x86)\Steam\steamapps\workshop\content\107410\894678801')
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'Pbo.ps1')
$package = Join-Path $project 'outputs\HelmetVoice-TFAR-R11'
$mod = Join-Path $package '@TFAR_DTR'
$original = Read-Pbo (Join-Path $TfarMod 'addons\tfar_core.pbo')
$patched = Read-Pbo (Join-Path $mod 'addons\tfar_core.pbo')
if ($original.Entries.Count -ne $patched.Entries.Count) { throw 'Core entry count changed' }
$changes = @()
for ($i=0; $i -lt $original.Entries.Count; $i++) {
    $a=$original.Entries[$i]; $b=$patched.Entries[$i]
    if ($a.Name -cne $b.Name -or $a.Method -ne $b.Method) { throw 'Core archive layout changed' }
    if ([Convert]::ToBase64String($a.Data) -cne [Convert]::ToBase64String($b.Data)) { $changes += $a.Name }
}
if ($changes.Count -ne 1 -or $changes[0] -ne 'functions\fnc_preparePositionCoordinates.sqf') { throw "Unexpected core changes: $changes" }
Write-Output 'PASS exactly one TFAR core entry changed; other source/assets preserved'
$pos = [Text.Encoding]::UTF8.GetString(($patched.Entries | Where-Object Name -eq $changes[0]).Data)
if (!$pos.Contains('%14~') -or !$pos.Contains('DTR_fnc_getState') -or !$pos.Contains('DTR2:0')) { throw 'Helmet state absent from POS' }
Write-Output 'PASS tagged helmet/scrambler state in packed POS function'
foreach ($pbo in Get-ChildItem (Join-Path $TfarMod 'addons') -Filter '*.pbo' | Where-Object Name -ne 'tfar_core.pbo') {
    if ((Get-FileHash $pbo.FullName).Hash -ne (Get-FileHash (Join-Path $mod "addons\$($pbo.Name)")).Hash) { throw "Changed unrelated PBO: $($pbo.Name)" }
}
Write-Output 'PASS every other TFAR PBO is byte-identical to installed Beta'
$addon = Read-Pbo (Join-Path $mod 'addons\dtr_core.pbo')
if ($addon.Properties['prefix'] -ne 'dtr\core') { throw 'DTR prefix mismatch' }
$config = [Text.Encoding]::UTF8.GetString(($addon.Entries | Where-Object Name -eq 'config.cpp').Data)
if (!$config.Contains('JMSLLTE_Deathtrooper_helmet')) { throw 'Verified LLTE helmet absent' }
foreach ($name in @('isHelmet','isSpeakerHelmet','getState','rebuildHelmetClasses','setScrambler','setHelmetVoice','initAce','syncSettings','diagnose')) {
    if (!($addon.Entries | Where-Object Name -eq "functions\fnc_$name.sqf")) { throw "Missing function $name" }
}
Write-Output 'PASS addon prefix, LLTE mapping, ACE and CBA function paths'
$preInit = [Text.Encoding]::UTF8.GetString(($addon.Entries | Where-Object Name -eq 'XEH_preInit.sqf').Data)
$postInit = [Text.Encoding]::UTF8.GetString(($addon.Entries | Where-Object Name -eq 'XEH_postInit.sqf').Data)
$sync = [Text.Encoding]::UTF8.GetString(($addon.Entries | Where-Object Name -eq 'functions\fnc_syncSettings.sqf').Data)
if (!$preInit.Contains('[-12, 0, -6, 1]') -or !$preInit.Contains('[80, 1200, 150, 0]') -or
    !$sync.Contains('"deathTrooperPitch"') -or !$sync.Contains('["DTR_pitch", -6]') -or
    !$postInit.Contains('_capability isEqualTo "DTR11"')) { throw 'R11 sound settings or capability guard absent' }
Write-Output 'PASS minus-six pitch default, high-pass default and R11 capability guard'
if (!$preInit.Contains('[8, 48, 32, 0]') -or !$preInit.Contains('[0, 1, 0.75, 2]') -or
    !$sync.Contains('"deathTrooperAmpDrive"') -or !$sync.Contains('["DTR_ampDrive", 32]') -or
    !$sync.Contains('"deathTrooperMetallic"') -or !$sync.Contains('["DTR_metallic", 0.75]')) { throw 'R11 amplifier/metallic settings absent' }
Write-Output 'PASS configurable amp drive and metallic character defaults packed'
if (!$preInit.Contains('[0, 1, 0.25, 2]') -or !$sync.Contains('"deathTrooperScreech"') -or
    !$sync.Contains('["DTR_screech", 0.25]')) { throw 'R11 high-frequency screech setting absent' }
Write-Output 'PASS high-frequency screech control and default packed'
if (!$preInit.Contains('[0, 1, 0.65, 2]') -or !$sync.Contains('"deathTrooperStatic"') -or
    !$sync.Contains('["DTR_staticAmount", 0.65]')) { throw 'R11 speech static setting absent' }
Write-Output 'PASS speech-static amount control and default packed'
if (!$preInit.Contains('[[8, 12, 16], ["8-bit", "12-bit", "16-bit (bypass)"], 0]') -or
    !$sync.Contains('["DTR_bitDepth", 8]') -or !$sync.Contains('["DTR_ringAmount", 0.12]') -or
    !$sync.Contains('["DTR_hardGate", true]') -or !$sync.Contains('["DTR_gateThreshold", -48]') -or
    !$sync.Contains('["DTR_gateHold", 1]')) { throw 'R11 digital/squelch preset incomplete' }
Write-Output 'PASS bit-depth choices, modulation and hard-squelch defaults packed'
if (!$preInit.Contains('[1800, 12000, 3200, 0]') -or !$preInit.Contains('[0, 0.6, 0.22, 2]') -or
    !$preInit.Contains('[0, 0.6, 0.25, 2]') -or !$preInit.Contains('[0.05, 1.5, 0.35, 2]') -or
    !$sync.Contains('"deathTrooperLowPass"') -or !$sync.Contains('["DTR_lowPass", 3200]') -or
    !$sync.Contains('"deathTrooperTailLength"') -or !$sync.Contains('["DTR_tailLength", 0.22]') -or
    !$sync.Contains('["DTR_reverbMix", 0.25]') -or !$sync.Contains('["DTR_reverbDecay", 0.35]') -or
    ([regex]::Matches($preInit, 'call CBA_settings_fnc_init;').Count -ne 32)) { throw 'R11 dark tone/tail settings or setting count incomplete' }
Write-Output 'PASS thirty-two CBA controls including preserved Death Trooper tone and tail defaults'
$standardDefaults = [ordered]@{HighPass='623'; LowPass='3100'; Drive='2.4'; Mix='1.0'; ResonanceFrequency='2000'; ResonanceGain='3'; ClarityFrequency='2500'; ClarityGain='2'}
foreach ($suffix in $standardDefaults.Keys) {
    $expected = '["DTR_helmet' + $suffix + '", ' + $standardDefaults[$suffix] + ']'
    if (!$sync.Contains(('"helmetVoice' + $suffix + '"')) -or !$sync.Contains($expected)) { throw "Standard helmet setting missing: $suffix" }
}
if (!$preInit.Contains('[80, 1200, 623, 0]') -or !$preInit.Contains('[1200, 10000, 3100, 0]') -or
    !$preInit.Contains('[1, 6, 2.4, 2]') -or !$preInit.Contains('[0, 1, 1.0, 2]') -or
    !$preInit.Contains('[500, 4000, 2000, 0]') -or !$preInit.Contains('[0, 9, 3, 1]') -or
    !$preInit.Contains('[1000, 4000, 2500, 0]') -or !$preInit.Contains('[0, 6, 2, 1]')) { throw 'Helmet band-pass/resonance/clarity CBA defaults missing' }
foreach ($obsolete in @('helmetClickMode','helmetClickVolume','helmetNoiseThreshold','helmetStaticAmount')) {
    if ($preInit.Contains($obsolete) -or $sync.Contains($obsolete)) { throw "Obsolete click/classifier setting present: $obsolete" }
}
if (([regex]::Matches($sync, 'call TFAR_fnc_setPluginSetting;').Count -ne 23)) { throw 'Unexpected synchronized DSP setting count' }
Write-Output 'PASS helmet band-pass, 2 kHz resonance and 2.5 kHz clarity; static/click/classifier controls removed'
$state = [Text.Encoding]::UTF8.GetString(($addon.Entries | Where-Object Name -eq 'functions\fnc_getState.sqf').Data)
$matcher = [Text.Encoding]::UTF8.GetString(($addon.Entries | Where-Object Name -eq 'functions\fnc_isSpeakerHelmet.sqf').Data)
$ace = [Text.Encoding]::UTF8.GetString(($addon.Entries | Where-Object Name -eq 'functions\fnc_initAce.sqf').Data)
if (!$state.Contains('DTR2:4') -or !$state.Contains('DTR2:1') -or !$state.Contains('DTR2:3') -or
    !$matcher.Contains('if ([_unit] call DTR_fnc_isHelmet) exitWith {false}') -or
    !$ace.Contains('DTR_fnc_setHelmetVoice') -or !$ace.Contains('DTR_fnc_setScrambler')) { throw 'Separate DT/standard helmet routing or ACE actions missing' }
Write-Output 'PASS separate ordinary helmet profile, DT precedence and both ACE toggle paths packed'
$roots=@('Stormtrooper_dirtyW','Stormtrooper_S','Stormtrooper_com_S','Clonetrooper_P3','Stormtrooper','Stormtrooper_shadow','Stormtrooper_mimban','Stormtrooper_mimban_H','Stormtrooper_dirty_H','Shoretrooper','Tanktrooper','Snowtrooper','Snowtflamer','EmpTiePilot','EmpTiePilotTech','Scouttrooper')
foreach ($root in $roots) { if (!$config.Contains(('"JMSLLTE_' + $root + '_helmet"'))) { throw "Missing LLTE root: $root" } }
Write-Output 'PASS sixteen verified LLTE full-helmet roots packed alongside separate DT mapping'
$zip = [IO.Compression.ZipFile]::OpenRead((Join-Path $mod 'teamspeak\task_force_radio_dtr.ts3_plugin'))
try {
    $dlls = @($zip.Entries | Where-Object FullName -like '*.dll')
    if ($dlls.Count -ne 1 -or $dlls[0].FullName -ne 'plugins/TFAR_win64.dll') { throw 'Unexpected plugin DLLs' }
    $stream = $dlls[0].Open()
    try { $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)) } finally { $stream.Dispose() }
    if ($hash -ne (Get-FileHash (Join-Path $project 'work\tfar-upstream\ts\src\build\TFAR_win64.dll')).Hash) { throw 'Stale packaged DLL' }
    if (@($zip.Entries | Where-Object FullName -like 'plugins/radio-sounds/*.wav').Count -lt 10) { throw 'Radio sound assets absent' }
} finally { $zip.Dispose() }
Write-Output 'PASS installer contains current x64 DLL and TFAR radio sounds'
if (Get-ChildItem -LiteralPath $package -Recurse -Filter '*.biprivatekey') { throw 'Private signing key leaked into package' }
Write-Output 'PASS no private signing key in deliverable'
$manifest = foreach ($file in Get-ChildItem -LiteralPath $mod -Recurse -File) {
    [pscustomobject]@{Path=$file.FullName.Substring($mod.Length+1); SHA256=(Get-FileHash $file.FullName).Hash; Bytes=$file.Length}
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $package 'Validation\manifest.json') -Encoding utf8
