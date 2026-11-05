[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$standalone = Split-Path $PSScriptRoot -Parent
$project = Split-Path $standalone -Parent
$stage = Join-Path $project 'outputs\Task-force-Vanguard-TFAR-addon-R12'
$archive = $stage + '.zip'
$mod = Join-Path $stage '@TFV_TFAR_Addon'
if (Test-Path -LiteralPath $archive) { throw 'Preserve the sealed archive; use a new release for further changes.' }
if (!(Test-Path -LiteralPath (Join-Path $mod 'addons\tfv_core.pbo.TFV_20261105_R12.bisign'))) { throw 'Signed standalone PBO missing.' }

# Refresh reviewed text before calculating the final runtime manifest.
Copy-Item -LiteralPath (Join-Path $standalone 'README.md'),(Join-Path $standalone 'LICENSE.md') -Destination $mod
Copy-Item -LiteralPath (Join-Path $standalone 'README.md'),(Join-Path $standalone 'LICENSE.md'),(Join-Path $standalone 'PLAN.md'),(Join-Path $standalone 'DEVELOPMENT_LOG.md') -Destination $stage
$source = Join-Path $stage 'Source\standalone'
$extensions = @('.md','.ps1','.cpp','.hpp','.h','.cs','.vcxproj','.manifest','.json')
$sourceFiles = @(Get-ChildItem -LiteralPath $standalone -File -Recurse | Where-Object {
    $_.FullName -notmatch '\\work\\' -and ($_.Extension -in $extensions -or $_.Name -eq '$PBOPREFIX$')
})
foreach ($file in $sourceFiles) {
    $target = Join-Path $source $file.FullName.Substring($standalone.Length + 1)
    $null = New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent)
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
Copy-Item -LiteralPath (Join-Path $standalone 'arma\validation\source-checks.json') -Destination (Join-Path $stage 'Validation\arma-source-checks.json')

. (Join-Path $PSScriptRoot 'Pbo.ps1')
$pbo = Read-Pbo (Join-Path $mod 'addons\tfv_core.pbo')
if ($pbo.Properties.prefix -cne 'tfv\core') { throw 'Unexpected PBO prefix.' }
foreach ($entry in $pbo.Entries) {
    $original = [IO.File]::ReadAllBytes((Join-Path $standalone ('arma\addons\tfv_core\' + $entry.Name)))
    if ([Convert]::ToBase64String($original) -cne [Convert]::ToBase64String($entry.Data)) { throw "PBO source mismatch: $($entry.Name)" }
}
$runtimeFiles = @(Get-ChildItem -LiteralPath $mod -File -Recurse)
if (@($runtimeFiles | Where-Object { $_.Name -match '^(tfar_core\.pbo|TFAR_win64\.dll|task_force_radio_pipe.*\.dll)$' }).Count) { throw 'Stock TFAR collision.' }
if (@(Get-ChildItem -LiteralPath $stage -File -Recurse | Where-Object { $_.Extension -in @('.biprivatekey','.mp3','.wav','.exe') }).Count) { throw 'Private key, recording or unintended executable in addon archive.' }
$manifest = @($runtimeFiles | ForEach-Object { [ordered]@{Path=$_.FullName.Substring($mod.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash;Bytes=$_.Length} })
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $stage 'Validation\manifest.json') -Encoding UTF8
[ordered]@{
    timestamp=[DateTime]::UtcNow.ToString('o'); architecture='Standalone addon; no TFAR replacements'
    bridgeChecks=29; companionChecks=96; retainedDspChecks=115
    pboSourceEntries=$pbo.Entries.Count; sourceFiles=$sourceFiles.Count
    stockTfarFilesIncluded=$false; privateKeysOrRecordingsIncluded=$false
    liveMultiplayerAccepted=$false; guiAccepted=$false
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'Validation\package-checks.json') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$archive)
$check = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    if ($check.Entries.Count -ne @(Get-ChildItem -LiteralPath $stage -File -Recurse).Count) { throw 'Archive entry count mismatch.' }
    if (@($check.Entries | Group-Object FullName | Where-Object Count -gt 1).Count) { throw 'Duplicate archive path.' }
} finally { $check.Dispose() }
Get-Item -LiteralPath $archive | Select-Object Name,Length
Get-FileHash -LiteralPath $archive -Algorithm SHA256
