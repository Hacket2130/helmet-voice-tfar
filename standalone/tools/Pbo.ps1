# Minimal lossless PBO reader/writer. Existing entry bytes (including compressed
# payloads) are preserved. New/changed entries are stored uncompressed.
Set-StrictMode -Version Latest
function Read-Pbo {
    param([string]$Path)
    $reader = [IO.BinaryReader]::new([IO.File]::OpenRead($Path))
    function Read-Z {
        $bytes = [Collections.Generic.List[byte]]::new()
        while (($b = $reader.ReadByte()) -ne 0) { $bytes.Add($b) }
        [Text.Encoding]::UTF8.GetString($bytes.ToArray())
    }
    try {
        $props = [ordered]@{}
        $entries = [Collections.Generic.List[object]]::new()
        while ($true) {
            $name = Read-Z
            $method = $reader.ReadUInt32(); $original = $reader.ReadUInt32()
            $reserved = $reader.ReadUInt32(); $stamp = $reader.ReadUInt32(); $size = $reader.ReadUInt32()
            if ($name -eq '') {
                if ($method -eq 0) { break }
                if ($method -ne 0x56657273) { throw 'Unknown PBO header' }
                while (($key = Read-Z) -ne '') { $props[$key] = Read-Z }
            } else {
                $entries.Add([pscustomobject]@{Name=$name; Method=$method; Original=$original; Reserved=$reserved; Stamp=$stamp; Size=$size; Data=$null})
            }
        }
        foreach ($entry in $entries) {
            $entry.Data = $reader.ReadBytes($entry.Size)
            if ($entry.Data.Length -ne $entry.Size) { throw "Truncated PBO entry: $($entry.Name)" }
        }
        [pscustomobject]@{Properties=$props; Entries=$entries}
    } finally { $reader.Dispose() }
}

function Write-Pbo {
    param($Archive, [string]$Path)
    $memory = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($memory)
    function Write-Z([string]$Value) {
        $writer.Write([Text.Encoding]::UTF8.GetBytes($Value)); $writer.Write([byte]0)
    }
    try {
        Write-Z ''
        $writer.Write([uint32]0x56657273)
        1..4 | ForEach-Object { $writer.Write([uint32]0) }
        foreach ($key in $Archive.Properties.Keys) { Write-Z $key; Write-Z $Archive.Properties[$key] }
        Write-Z ''
        foreach ($entry in $Archive.Entries) {
            Write-Z $entry.Name
            $writer.Write([uint32]$entry.Method); $writer.Write([uint32]$entry.Original)
            $writer.Write([uint32]$entry.Reserved); $writer.Write([uint32]$entry.Stamp)
            $writer.Write([uint32]$entry.Data.Length)
        }
        Write-Z ''
        1..5 | ForEach-Object { $writer.Write([uint32]0) }
        foreach ($entry in $Archive.Entries) { $writer.Write([byte[]]$entry.Data) }
        $writer.Flush()
        $digest = [Security.Cryptography.SHA1]::HashData($memory.ToArray())
        $writer.Write([byte]0); $writer.Write($digest); $writer.Flush()
        [IO.File]::WriteAllBytes($Path, $memory.ToArray())
    } finally { $writer.Dispose(); $memory.Dispose() }
}

function New-PboFromFolder {
    param([string]$Folder, [string]$Prefix, [string]$Destination)
    $folderPath = (Resolve-Path -LiteralPath $Folder).Path.TrimEnd('\')
    $entries = @(Get-ChildItem -LiteralPath $folderPath -File -Recurse | Sort-Object FullName | Where-Object Name -ne '$PBOPREFIX$' | ForEach-Object {
        $data = [IO.File]::ReadAllBytes($_.FullName)
        [pscustomobject]@{Name=$_.FullName.Substring($folderPath.Length+1); Method=0; Original=$data.Length; Reserved=0; Stamp=0; Data=$data}
    })
    Write-Pbo ([pscustomobject]@{Properties=[ordered]@{prefix=$Prefix}; Entries=$entries}) $Destination
}
