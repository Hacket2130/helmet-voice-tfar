[CmdletBinding()]
param(
    [string]$PackagePath = (Join-Path $PSScriptRoot '..\..\outputs\Task-force-Vanguard-TFAR-addon-R12.zip'),
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedPackageSha256,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\outputs\Task-force-Vanguard-TFAR-addon-R12-Release')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$work = Join-Path $project ('work\r12i-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$null = New-Item -ItemType Directory -Force -Path $work
$package = Get-Item -LiteralPath $PackagePath
$actual = (Get-FileHash -LiteralPath $package.FullName -Algorithm SHA256).Hash
if ($actual -ne $ExpectedPackageSha256) { throw 'The package does not match the reviewed standalone R12 SHA256.' }
if ($package.Length -lt 32 -or $package.Length -gt 512MB) { throw 'Package size exceeds installer limits.' }
$manifest = [ordered]@{
    schemaVersion = 2
    product = 'task-force-vanguard-tfar-addon'
    releaseNumber = 12
    displayVersion = 'R12'
    pluginVersion = '0.12.0'
    packageUrl = 'https://github.com/Hacket2130/helmet-voice-tfar/releases/download/r12/Task-force-Vanguard-TFAR-addon-R12.zip'
    sha256 = $actual.ToLowerInvariant()
    packageBytes = $package.Length
    releasePageUrl = 'https://github.com/Hacket2130/helmet-voice-tfar/releases/tag/r12'
    notes = 'R12 is a standalone addon: keep the official TFAR Beta Arma mod and TeamSpeak plugin enabled. Add @TFV_TFAR_Addon and the separate Vanguard companion. CBA settings and optional ACE toggles control helmet effects. Migrating from R11 requires disabling @TFAR_DTR and restoring the official TFAR TeamSpeak plugin yourself. No TFAR files are replaced.'
}
$manifestPath = Join-Path $work 'update.json'
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x x64 compiler not found.' }
$common = @('/nologo','/langversion:5','/optimize+','/platform:x64','/warn:4','/warnaserror+',
    '/r:System.dll','/r:System.Core.dll','/r:System.Web.Extensions.dll',
    '/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll',
    "/resource:$($package.FullName),BundledRelease.zip", "/resource:$manifestPath,BundledManifest.json")
$setupStaged = Join-Path $work 'Task-force-Vanguard-TFAR-addon-Setup-R12.exe'
& $compiler @common '/target:winexe' '/r:System.Windows.Forms.dll' '/r:System.Drawing.dll' "/win32manifest:$(Join-Path $PSScriptRoot 'app.manifest')" "/out:$setupStaged" (Join-Path $PSScriptRoot 'Core.cs') (Join-Path $PSScriptRoot 'Program.cs')
if ($LASTEXITCODE -ne 0) { throw "Setup compilation failed: $LASTEXITCODE" }
$testExe = Join-Path $work 'Vanguard-Installer-Tests.exe'
& $compiler @common '/target:exe' "/out:$testExe" (Join-Path $PSScriptRoot 'Core.cs') (Join-Path $PSScriptRoot 'Tests.cs')
if ($LASTEXITCODE -ne 0) { throw "Test compilation failed: $LASTEXITCODE" }
$testRoot = Join-Path $project ('work\iv-' + [guid]::NewGuid().ToString('N').Substring(0,8))
Push-Location -LiteralPath $project
try {
    & $testExe $testRoot 2>&1 | Tee-Object -FilePath (Join-Path $work 'tests.txt')
    if ($LASTEXITCODE -ne 0) { throw "Standalone installer tests failed: $LASTEXITCODE" }
} finally { Pop-Location }
if ((Get-FileHash -LiteralPath $package.FullName).Hash -ne $actual) { throw 'Package changed during installer build.' }
$null = New-Item -ItemType Directory -Force -Path $OutputDirectory
$setupPath = Join-Path $OutputDirectory 'Task-force-Vanguard-TFAR-addon-Setup-R12.exe'
$packageOutput = Join-Path $OutputDirectory 'Task-force-Vanguard-TFAR-addon-R12.zip'
$pluginOutput = Join-Path $OutputDirectory 'Task-force-Vanguard-TFAR-addon-R12.ts3_plugin'
$feedOutput = Join-Path $OutputDirectory 'update.json'
foreach ($target in @($setupPath,$packageOutput,$pluginOutput,$feedOutput)) {
    if (Test-Path -LiteralPath $target) { throw "Refusing to overwrite an existing artifact: $target. Choose a fresh output directory." }
}
Copy-Item -LiteralPath $setupStaged -Destination $setupPath
Copy-Item -LiteralPath $package.FullName -Destination $packageOutput
Copy-Item -LiteralPath $manifestPath -Destination $feedOutput
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($package.FullName)
try {
    $entries = @($zip.Entries | Where-Object { $_.FullName.Replace('\','/') -eq '@TFV_TFAR_Addon/teamspeak/task_force_vanguard.ts3_plugin' })
    if ($entries.Count -ne 1) { throw 'Expected exactly one standalone companion package.' }
    $source = $entries[0].Open()
    try {
        $target = [IO.File]::Open($pluginOutput,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
        try { $source.CopyTo($target) } finally { $target.Dispose() }
    } finally { $source.Dispose() }
} finally { $zip.Dispose() }
$assets = @($setupPath,$pluginOutput,$packageOutput,$feedOutput)
$checksums = foreach ($asset in $assets) { '{0}  {1}' -f (Get-FileHash -LiteralPath $asset).Hash.ToLowerInvariant(), [IO.Path]::GetFileName($asset) }
[IO.File]::WriteAllLines((Join-Path $OutputDirectory 'SHA256SUMS.txt'),$checksums,(New-Object Text.UTF8Encoding($false)))
$record = foreach ($asset in $assets) {
    $item = Get-Item -LiteralPath $asset
    [pscustomobject]@{Name=$item.Name;Bytes=$item.Length;SHA256=(Get-FileHash -LiteralPath $asset).Hash}
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'artifacts.json') -Encoding UTF8
$record | Format-Table -AutoSize
Write-Output "Validation logs: $work"
