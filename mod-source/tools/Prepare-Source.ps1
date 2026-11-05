param([string]$Destination = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
if (!$Destination) { $Destination = Join-Path $project 'work\tfar-upstream' }
$patch = Join-Path $project 'tfar-dtr.patch'
if (!(Test-Path -LiteralPath $patch)) { throw 'Run the delivered Source/tools/Prepare-Source.ps1 alongside Source/tfar-dtr.patch' }
if (Test-Path -LiteralPath $Destination) { throw 'Destination already exists; use an empty directory' }
$env:GIT_LFS_SKIP_SMUDGE = '1'
& git clone --no-checkout https://github.com/michail-nikolaev/task-force-arma-3-radio.git $Destination
if ($LASTEXITCODE) { throw 'Clone failed' }
& git -C $Destination checkout fce977fa388bb527a989af1e162ade648c8d9d75
if ($LASTEXITCODE) { throw 'Pinned checkout failed' }
& git -C $Destination submodule update --init ts/include/glfw ts/include/glm
if ($LASTEXITCODE) { throw 'GL dependencies failed' }
& git -C $Destination -c submodule.ts/include/freetype.url=https://github.com/freetype/freetype.git submodule update --init ts/include/freetype
if ($LASTEXITCODE) { throw 'FreeType dependency failed' }
& git -C $Destination lfs pull '--include=ts/lib/x64/freetype.lib,ts/lib/x64/X3DAudio.lib'
if ($LASTEXITCODE) { throw 'LFS libraries failed' }
& git -C $Destination apply --check $patch
if ($LASTEXITCODE) { throw 'Patch does not match pinned baseline' }
& git -C $Destination apply $patch
if ($LASTEXITCODE) { throw 'Patch application failed' }
Write-Output "Prepared $Destination"
