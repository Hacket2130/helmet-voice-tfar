[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
$sourceRoot=Split-Path $PSScriptRoot -Parent
if(Test-Path -LiteralPath $Destination){throw 'Use a fresh source export directory.'}
$extensions=@('.md','.ps1','.cpp','.hpp','.h','.cs','.vcxproj','.manifest','.json')
foreach($file in Get-ChildItem -LiteralPath $sourceRoot -File -Recurse){
    if($file.FullName -match '\\work\\'){continue}
    if($file.Extension -notin $extensions -and $file.Name -ne '$PBOPREFIX$'){continue}
    $relative=$file.FullName.Substring($sourceRoot.Length+1)
    $target=Join-Path $Destination ('standalone\'+$relative)
    $null=New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent)
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
Write-Output "Exported text source to $Destination"
