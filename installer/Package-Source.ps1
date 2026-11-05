[CmdletBinding()]
param([string]$OutputDirectory = (Join-Path $PSScriptRoot '..\outputs\Task-force-Vanguard-TFAR-addon-R11'))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$project = Split-Path $PSScriptRoot -Parent
$stage = Join-Path $project ('work\isrc-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$null = New-Item -ItemType Directory -Path (Join-Path $stage 'installer') -Force
$names = @('Core.cs','Program.cs','Tests.cs','app.manifest','Build-Installer.ps1','Check-PublishedRelease.ps1','Package-Source.ps1','README.md','VALIDATION.md')
foreach ($name in $names) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $stage 'installer') }
Copy-Item -LiteralPath (Join-Path $project 'outputs\HelmetVoice-TFAR-R11\@TFAR_DTR\LICENSE.md') -Destination (Join-Path $stage 'LICENSE.md')
$readme = @'
# Task force Vanguard TFAR addon installer source

The installer/ folder contains C# 5 source, safety tests and build/release instructions.

Place a copy of Task-force-Vanguard-TFAR-addon-R11.zip at outputs/HelmetVoice-TFAR-R11.zip and extract it into outputs/HelmetVoice-TFAR-R11/ beside it, then run installer/Build-Installer.ps1 from this directory using PowerShell. The legacy input names preserve the approved R11 build contract. The mod ZIP is deliberately not duplicated in this small source archive. Its own Source/ directory contains the TFAR modification source, upstream patch and rebuild instructions.

No private signing keys or user recordings are included. See installer/VALIDATION.md for evidence and remaining acceptance gates.
'@
[IO.File]::WriteAllText((Join-Path $stage 'README.md'), $readme, (New-Object Text.UTF8Encoding($false)))
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$archive = Join-Path $OutputDirectory 'Task-force-Vanguard-TFAR-addon-Installer-Source-R11.zip'
if (Test-Path -LiteralPath $archive) { throw 'Source archive already exists; use a new output name instead of replacing a published artifact.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $archive, [IO.Compression.CompressionLevel]::Optimal, $false)
$hashLine = '{0}  {1}' -f (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant(), [IO.Path]::GetFileName($archive)
Add-Content -LiteralPath (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Value $hashLine -Encoding utf8
Get-Item -LiteralPath $archive | Select-Object Name,Length
Write-Output $hashLine
