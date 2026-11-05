param(
    [string]$ArmaTools = 'C:\Program Files (x86)\Steam\steamapps\common\Arma 3 Tools'
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Join-Path $PSScriptRoot 'addons\tfv_core'
$baselinePath = Join-Path $PSScriptRoot 'Preserved-Behavior-Baseline.json'
$baseline = Get-Content -LiteralPath $baselinePath -Raw | ConvertFrom-Json
$validationRoot = Join-Path $PSScriptRoot 'validation'
New-Item -ItemType Directory -Force -Path $validationRoot | Out-Null
$checks = [Collections.Generic.List[object]]::new()
function Assert-Source([string]$Name, [bool]$Pass) {
    $checks.Add([pscustomobject]@{ check = $Name; passed = $Pass })
    if (!$Pass) { throw "Source validation failed: $Name" }
}
function Get-CanonicalHash([string]$Text) {
    $normalized = [regex]::Replace($Text, '\r\n?', "`n")
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($algorithm.ComputeHash([Text.Encoding]::UTF8.GetBytes($normalized))).Replace('-', '').ToLowerInvariant() }
    finally { $algorithm.Dispose() }
}

# Preserving these functions and the CBA definition body protects the accepted
# sound profiles, LLTE inheritance matching and ACE self-action behavior. The
# pinned manifest ships with this source; the legacy replacement addon is not
# required. Hashes normalize line endings only, for portable source checkouts.
$preserved = @('getState','initAce','isHelmet','isSpeakerHelmet','rebuildHelmetClasses','setHelmetVoice','setScrambler')
foreach ($name in $preserved) {
    $relativePath = "functions/fnc_$name.sqf"
    $after = Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw
    Assert-Source "Preserved fnc_$name" ((Get-CanonicalHash $after) -ceq $baseline.functions.$relativePath)
}
$preInit = Get-Content -LiteralPath (Join-Path $sourceRoot 'XEH_preInit.sqf') -Raw
$bodyStart = $preInit.IndexOf('DTR_helmetCache = createHashMap;', [StringComparison]::Ordinal)
$definitionBody = if ($bodyStart -ge 0) { $preInit.Substring($bodyStart) } else { '' }
Assert-Source 'Preserved all existing CBA definitions and defaults' ((Get-CanonicalHash $definitionBody) -ceq $baseline.cbaDefinitionBodySha256)
Assert-Source 'Exactly 32 CBA settings' ([regex]::Matches($preInit, 'call CBA_settings_fnc_init;').Count -eq 32)

$config = Get-Content -LiteralPath (Join-Path $sourceRoot 'config.cpp') -Raw
$settingsPattern = '(?s)class DTR_Settings\s*\{.*\};\s*$'
Assert-Source 'Preserved exact LLTE helmet roots' ((Get-CanonicalHash ([regex]::Match($config, $settingsPattern).Value)) -ceq $baseline.llteSettingsClassSha256)
Assert-Source 'Own CfgPatches identifier' ($config -match 'class tfv_core\s*\{')
Assert-Source 'Original TFAR remains a dependency' ($config -match 'requiredAddons\[\].*"tfar_core"')
Assert-Source 'Own PBO prefix' ((Get-Content -LiteralPath (Join-Path $sourceRoot '$PBOPREFIX$') -Raw).Trim() -ceq 'tfv\core')
Assert-Source 'Own function path' ($config.Contains('file = "\tfv\core\functions"'))

$sync = Get-Content -LiteralPath (Join-Path $sourceRoot 'functions\fnc_syncSettings.sqf') -Raw
$pairPattern = '\["(DTR_\w+)",\s*(-?\d+(?:\.\d+)?|true|false)\]'
$newSchema = [regex]::Match($sync, '(?s)private _schema = \[(.*?)\];').Groups[1].Value
$oldPairs = @($baseline.protocolSettings)
$newPairs = @([regex]::Matches($newSchema, $pairPattern) | ForEach-Object { $_.Groups[1].Value + '=' + $_.Groups[2].Value })
Assert-Source 'Protocol has exactly 23 numeric settings' ($newPairs.Count -eq 23)
Assert-Source 'Protocol preserves setting order and defaults' (($oldPairs -join '|') -ceq ($newPairs -join '|'))

$allSqf = @(Get-ChildItem -LiteralPath $sourceRoot -Filter '*.sqf' -Recurse -File)
$source = ($allSqf | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
Assert-Source 'No assignments to TFAR functions or variables' ($source -notmatch '(?im)^\s*TFAR_\w+\s*=')
Assert-Source 'No stock TFAR custom settings transport' ($source -notmatch 'call\s+TFAR_fnc_setPluginSetting')
Assert-Source 'Only two local companion extension call sites' ([regex]::Matches($source, '\bcallExtension\b').Count -eq 2)
$sender = Get-Content -LiteralPath (Join-Path $sourceRoot 'functions\fnc_sendState.sqf') -Raw
$postInit = Get-Content -LiteralPath (Join-Path $sourceRoot 'XEH_postInit.sqf') -Raw
$diagnose = Get-Content -LiteralPath (Join-Path $sourceRoot 'functions\fnc_diagnose.sqf') -Raw
Assert-Source 'Sender excludes dedicated servers and headless clients' ($sender.Contains('if (!hasInterface) exitWith'))
Assert-Source 'Initialization excludes dedicated servers and headless clients' ($postInit.Contains('if (!hasInterface) exitWith'))
Assert-Source 'Diagnostic scopes extension call to interactive clients' ($diagnose -match '(?s)if \(hasInterface\) then \{.*callExtension "PING";\s*\};')
Assert-Source 'Heartbeat is 0.5 seconds' ($postInit.Contains('}, 0.5] call CBA_fnc_addPerFrameHandler'))
Assert-Source 'Mission token published by server' ($preInit -match '(?s)if \(isServer\) then \{.*publicVariable "TFV_missionToken";\s*\};')
Assert-Source 'State uses separate extension and protocol' ($sender.Contains('"tfv_helmet_voice" callExtension _request') -and $sender.Contains('["TFV12", _serverUID, _token, str _mode, _settings]'))
Assert-Source 'State success requires exact companion acceptance' ($sender.Contains('_response isEqualTo "OK"'))
Assert-Source 'Legacy replacement plugin rejected' ($sender.Contains('find "-dtr."'))
Assert-Source 'Legacy replacement core rejected' ($postInit.Contains('find "DTR2:"'))

# CfgConvert validates Arma config syntax. It does not execute SQF or establish
# multiplayer/audio acceptance; those remain explicit integration gates.
$cfgConvert = Join-Path $ArmaTools 'CfgConvert\CfgConvert.exe'
$compiledConfig = Join-Path $validationRoot 'tfv-config.bin'
try {
    & $cfgConvert -bin -dst $compiledConfig (Join-Path $sourceRoot 'config.cpp') 2>&1 |
        Tee-Object -FilePath (Join-Path $validationRoot 'cfgconvert.log')
    Assert-Source 'Arma CfgConvert accepted config.cpp' ($LASTEXITCODE -eq 0)
} finally {
    # A validation-only binary must never be bundled as source or as an addon.
    if (Test-Path -LiteralPath $compiledConfig -PathType Leaf) { Remove-Item -LiteralPath $compiledConfig }
}
$report = [pscustomobject]@{
    timestamp = [DateTime]::UtcNow.ToString('o')
    scope = 'Arma source preservation and standalone transport boundary; config compilation only'
    baselineManifestSha256 = (Get-FileHash -LiteralPath $baselinePath -Algorithm SHA256).Hash
    checks = $checks
    runtimeSqfTested = $false
    multiplayerAudioTested = $false
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $validationRoot 'source-checks.json') -Encoding UTF8
Write-Output "PASS: $($checks.Count) Arma source checks. SQF runtime and multiplayer/audio acceptance remain untested."
