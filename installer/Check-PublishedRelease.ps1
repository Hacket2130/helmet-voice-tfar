[CmdletBinding()]
param([string]$SetupPath = (Join-Path $PSScriptRoot '..\outputs\Task-force-Vanguard-TFAR-addon-R11\Task-force-Vanguard-TFAR-addon-Setup-R11.exe'))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($PSVersionTable.PSEdition -ne 'Desktop') {
    throw 'Use Windows PowerShell 5.1 for this .NET Framework smoke check: powershell.exe -NoProfile -File installer\Check-PublishedRelease.ps1'
}
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$setup = (Get-Item -LiteralPath $SetupPath).FullName
$testRoot = Join-Path $project ('work\net-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$assembly = [Reflection.Assembly]::LoadFrom($setup)
$core = $assembly.GetType('HelmetVoiceSetup.InstallerCore', $true)
$bundled = $core.GetMethod('LoadBundledManifest').Invoke($null, @())
$latest = $core.GetMethod('GetLatest').Invoke($null, @())
if ($latest.releaseNumber -ne $bundled.releaseNumber -or $latest.sha256 -ne $bundled.sha256 -or $latest.packageBytes -ne $bundled.packageBytes) {
    throw 'Published feed does not match this setup build. Do not silently change a published release.'
}
$installed = $core.GetMethod('InstallOnline').Invoke($null, @($testRoot, $latest, $null))
$readback = $core.GetMethod('ReadState').Invoke($null, @($testRoot))
if ($installed.ReleaseNumber -ne $latest.releaseNumber -or $readback.ReleaseNumber -ne $latest.releaseNumber) { throw 'Online install/readback disagrees with feed.' }
$report = [ordered]@{
    CheckedUtc = [DateTime]::UtcNow.ToString('o')
    Feed = 'https://github.com/Hacket2130/helmet-voice-tfar/releases/latest/download/update.json'
    Revision = $latest.releaseNumber
    PackageSHA256 = $latest.sha256
    PackageBytes = $latest.packageBytes
    NetworkInstall = 'PASS'
    VerifiedReadback = 'PASS'
    TestRoot = $testRoot
    LiveTeamSpeakOrArmaChanged = $false
}
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $project 'work\vanguard-installer-build\published-check.json') -Encoding UTF8
$report | ConvertTo-Json
