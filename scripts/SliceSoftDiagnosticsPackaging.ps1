Set-StrictMode -Version Latest

function Read-SliceSoftBinaryRange
{
    param([System.IO.BinaryReader]$Reader, [long]$Offset, [int]$Count)
    if ($Offset -lt 0 -or $Count -lt 0 -or $Offset -gt $Reader.BaseStream.Length - $Count)
    {
        throw "Binary structure extends beyond the file."
    }
    [void]$Reader.BaseStream.Seek($Offset, [System.IO.SeekOrigin]::Begin)
    $bytes = $Reader.ReadBytes($Count)
    if ($bytes.Length -ne $Count) { throw "Binary structure is truncated." }
    return ,$bytes
}

function Read-SliceSoftPeCodeView
{
    param([string]$Path)
    $reader = [System.IO.BinaryReader]::new([System.IO.File]::OpenRead($Path))
    try
    {
        $dos = Read-SliceSoftBinaryRange $reader 0 64
        if ([BitConverter]::ToUInt16($dos, 0) -ne 0x5A4D) { throw "Not a PE image: $Path" }
        $peOffset = [BitConverter]::ToUInt32($dos, 60)
        $header = Read-SliceSoftBinaryRange $reader $peOffset 24
        if ([BitConverter]::ToUInt32($header, 0) -ne 0x4550) { throw "Invalid PE signature: $Path" }
        $sectionCount = [BitConverter]::ToUInt16($header, 6)
        $optionalSize = [BitConverter]::ToUInt16($header, 20)
        $optional = Read-SliceSoftBinaryRange $reader ($peOffset + 24) $optionalSize
        $magic = [BitConverter]::ToUInt16($optional, 0)
        $directoryOffset = if ($magic -eq 0x20B) { 112 } elseif ($magic -eq 0x10B) { 96 } else { throw "Unknown PE optional header." }
        if ($optionalSize -lt $directoryOffset + 56 -or
            [BitConverter]::ToUInt32($optional, ($directoryOffset - 4)) -lt 7)
        { throw "PE debug directory is missing: $Path" }
        $debugRva = [BitConverter]::ToUInt32($optional, ($directoryOffset + 48))
        $debugSize = [BitConverter]::ToUInt32($optional, ($directoryOffset + 52))
        if ($debugSize -eq 0 -or $debugSize -gt 1048576) { throw "PE CodeView directory is missing or invalid: $Path" }
        $debugOffset = $null
        for ($index = 0; $index -lt $sectionCount; ++$index)
        {
            $section = Read-SliceSoftBinaryRange $reader ($peOffset + 24 + $optionalSize + 40 * $index) 40
            $rva = [BitConverter]::ToUInt32($section, 12)
            $rawSize = [BitConverter]::ToUInt32($section, 16)
            if ($debugRva -ge $rva -and [long]$debugRva - $rva + $debugSize -le $rawSize)
            {
                $debugOffset = [long][BitConverter]::ToUInt32($section, 20) + $debugRva - $rva
                break
            }
        }
        if ($null -eq $debugOffset) { throw "PE debug RVA is not backed by a section: $Path" }
        for ($index = 0; $index + 28 -le $debugSize; $index += 28)
        {
            $entry = Read-SliceSoftBinaryRange $reader ($debugOffset + $index) 28
            if ([BitConverter]::ToUInt32($entry, 12) -ne 2) { continue }
            $recordSize = [BitConverter]::ToUInt32($entry, 16)
            if ($recordSize -lt 25 -or $recordSize -gt 1048576) { continue }
            $record = Read-SliceSoftBinaryRange $reader ([BitConverter]::ToUInt32($entry, 24)) $recordSize
            if ([BitConverter]::ToUInt32($record, 0) -ne 0x53445352) { continue }
            $guid = [Guid]::new([byte[]]$record[4..19])
            $nameEnd = [Array]::IndexOf($record, [byte]0, 24)
            if ($nameEnd -lt 0) { throw "CodeView PDB name is not terminated: $Path" }
            $pdbName = [System.Text.Encoding]::UTF8.GetString($record, 24, $nameEnd - 24)
            return [pscustomobject]@{
                guid = $guid.ToString("D"); age = [BitConverter]::ToUInt32($record, 20)
                pdbFile = [System.IO.Path]::GetFileName($pdbName)
            }
        }
        throw "An RSDS CodeView record was not found: $Path"
    }
    finally { $reader.Dispose() }
}

function Read-SliceSoftPdbIdentity
{
    param([string]$Path)
    $reader = [System.IO.BinaryReader]::new([System.IO.File]::OpenRead($Path))
    try
    {
        $header = Read-SliceSoftBinaryRange $reader 0 56
        $magic = [System.Text.Encoding]::ASCII.GetString($header, 0, 32)
        if ($magic -ne "Microsoft C/C++ MSF 7.00`r`n$([char]0x1A)DS$([char]0)$([char]0)$([char]0)")
        { throw "Only MSF 7 PDB files are supported: $Path" }
        $pageSize = [BitConverter]::ToUInt32($header, 32)
        $directorySize = [BitConverter]::ToUInt32($header, 44)
        $blockMap = [BitConverter]::ToUInt32($header, 52)
        if ($pageSize -lt 512 -or $pageSize -gt 65536 -or ($pageSize -band ($pageSize - 1)) -ne 0 -or
            $directorySize -lt 12 -or $directorySize -gt 16777216)
        { throw "PDB directory size is invalid: $Path" }
        $pageCount = [int][Math]::Ceiling($directorySize / [double]$pageSize)
        if ($pageCount * 4 -gt $pageSize) { throw "Multi-page PDB directory block maps are unsupported: $Path" }
        $blocks = Read-SliceSoftBinaryRange $reader ([long]$blockMap * $pageSize) ($pageCount * 4)
        $directory = [byte[]]::new($directorySize)
        for ($index = 0; $index -lt $pageCount; ++$index)
        {
            $page = [BitConverter]::ToUInt32($blocks, $index * 4)
            $count = [int][Math]::Min($pageSize, $directorySize - $index * $pageSize)
            $data = Read-SliceSoftBinaryRange $reader ([long]$page * $pageSize) $count
            [Array]::Copy($data, 0, $directory, $index * $pageSize, $count)
        }
        $streams = [BitConverter]::ToUInt32($directory, 0)
        if ($streams -lt 2 -or [long]$streams * 4 + 4 -gt $directorySize) { throw "PDB stream table is invalid: $Path" }
        $streamZeroSize = [BitConverter]::ToUInt32($directory, 4)
        $infoSize = [BitConverter]::ToUInt32($directory, 8)
        if ($infoSize -lt 28 -or $infoSize -eq [uint32]::MaxValue) { throw "PDB info stream is missing: $Path" }
        $streamZeroPages = if ($streamZeroSize -eq [uint32]::MaxValue) { 0 } else { [Math]::Ceiling($streamZeroSize / [double]$pageSize) }
        $infoBlockOffset = [long]4 + $streams * 4 + $streamZeroPages * 4
        if ($infoBlockOffset + 4 -gt $directorySize) { throw "PDB info stream block is missing: $Path" }
        $infoPage = [BitConverter]::ToUInt32($directory, [int]$infoBlockOffset)
        $info = Read-SliceSoftBinaryRange $reader ([long]$infoPage * $pageSize) 28
        return [pscustomobject]@{
            guid = ([Guid]::new([byte[]]$info[12..27])).ToString("D")
            age = [BitConverter]::ToUInt32($info, 8)
        }
    }
    finally { $reader.Dispose() }
}

function Copy-SliceSoftDiagnosticsDependencies
{
    param([string]$MetadataPath, [string]$StagingDir)
    if (-not (Test-Path -LiteralPath $MetadataPath -PathType Leaf))
    { throw "Diagnostics runtime metadata was not generated: $MetadataPath" }
    $metadata = @{}
    foreach ($line in Get-Content -LiteralPath $MetadataPath)
    {
        if ($line -match '^([^=]+)=(.*)$') { $metadata[$Matches[1]] = $Matches[2] }
    }
    $inventory = @()
    foreach ($component in @("spdlog", "fmt"))
    {
        if (-not $metadata.ContainsKey($component) -or -not $metadata.ContainsKey("${component}License"))
        { throw "Diagnostics metadata is missing '$component'." }
        $library = [string]$metadata[$component]
        $license = [string]$metadata["${component}License"]
        if (-not (Test-Path -LiteralPath $library -PathType Leaf) -or -not (Test-Path -LiteralPath $license -PathType Leaf))
        { throw "Diagnostics library or license is missing: $component" }
        $extension = [System.IO.Path]::GetExtension($library).ToLowerInvariant()
        if ($extension -notin @(".lib", ".dll")) { throw "Unsupported diagnostic library type: $library" }
        $relative = $null
        if ($extension -eq ".dll")
        {
            $relative = [System.IO.Path]::GetFileName($library)
            $destination = Join-Path $StagingDir $relative
            if ((Test-Path -LiteralPath $destination) -and
                (Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath $library).Hash)
            { throw "Diagnostics dependency name collides with an existing payload: $relative" }
            Copy-Item -LiteralPath $library -Destination $destination -Force
        }
        $licenseRelative = "licenses/$component.txt"
        New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir "licenses") | Out-Null
        Copy-Item -LiteralPath $license -Destination (Join-Path $StagingDir $licenseRelative) -Force
        $inventory += [ordered]@{
            component = $component
            linkage = $(if ($extension -eq ".dll") { "shared" } else { "static" })
            file = $relative
            librarySha256 = (Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash.ToLowerInvariant()
            license = $licenseRelative
            licenseSha256 = (Get-FileHash -LiteralPath $license -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    return ,$inventory
}

function Export-SliceSoftSymbolArchive
{
    param([string[]]$BinaryPaths, [string]$ArchiveRoot, [string]$BuildManifest, [string]$Config,
        [string]$PackagedBinaryDirectory = "")
    $pairs = @()
    $names = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($binary in $BinaryPaths)
    {
        $pe = Read-SliceSoftPeCodeView -Path $binary
        if (-not $names.Add([System.IO.Path]::GetFileName($binary)) -or -not $names.Add($pe.pdbFile))
        { throw "Symbol archive contains duplicate filenames: $binary" }
        $pdb = Join-Path (Split-Path -Parent $binary) $pe.pdbFile
        if (-not (Test-Path -LiteralPath $pdb -PathType Leaf)) { throw "Matching PDB is missing: $pdb" }
        $identity = Read-SliceSoftPdbIdentity -Path $pdb
        if ($identity.guid -ne $pe.guid -or $identity.age -ne $pe.age)
        { throw "PDB GUID/age does not match PE CodeView: $binary" }
        $pairs += [pscustomobject]@{ binary = $binary; pdb = $pdb; identity = $identity }
    }
    $archiveId = "{0}_{1}_{2}" -f $Config, [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssfff"), ([Guid]::NewGuid().ToString("N").Substring(0, 8))
    $directory = Join-Path $ArchiveRoot $archiveId
    New-Item -ItemType Directory -Path $directory -ErrorAction Stop | Out-Null
    $entries = @()
    foreach ($pair in $pairs)
    {
        $binaryName = [System.IO.Path]::GetFileName($pair.binary)
        $pdbName = [System.IO.Path]::GetFileName($pair.pdb)
        $archivedBinary = Join-Path $directory $binaryName
        $archivedPdb = Join-Path $directory $pdbName
        Copy-Item -LiteralPath $pair.binary -Destination $archivedBinary
        Copy-Item -LiteralPath $pair.pdb -Destination $archivedPdb
        $archivedPe = Read-SliceSoftPeCodeView -Path $archivedBinary
        $archivedIdentity = Read-SliceSoftPdbIdentity -Path $archivedPdb
        if ($archivedPe.guid -ne $archivedIdentity.guid -or $archivedPe.age -ne $archivedIdentity.age)
        { throw "Build artifacts changed while archiving: $binaryName" }
        $binaryHash = (Get-FileHash -LiteralPath $archivedBinary -Algorithm SHA256).Hash.ToLowerInvariant()
        if (-not [string]::IsNullOrWhiteSpace($PackagedBinaryDirectory))
        {
            $packagedBinary = Join-Path $PackagedBinaryDirectory $binaryName
            if (-not (Test-Path -LiteralPath $packagedBinary -PathType Leaf) -or
                (Get-FileHash -LiteralPath $packagedBinary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $binaryHash)
            { throw "Symbol archive does not match packaged binary: $binaryName" }
        }
        $entries += [ordered]@{
            binary = $binaryName; pdb = $pdbName
            binarySha256 = $binaryHash
            pdbSha256 = (Get-FileHash -LiteralPath $archivedPdb -Algorithm SHA256).Hash.ToLowerInvariant()
            codeViewGuid = $archivedIdentity.guid; codeViewAge = $archivedIdentity.age
        }
    }
    Copy-Item -LiteralPath $BuildManifest -Destination (Join-Path $directory "slicesoft_build_manifest.json")
    $manifest = [ordered]@{
        schema = "slicesoft.symbol_archive.1"; config = $Config; archiveId = $archiveId
        generatedAtUtc = [DateTime]::UtcNow.ToString("o"); symbols = $entries
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $directory "symbols_manifest.json") -Encoding UTF8
    return [ordered]@{ archiveId = $archiveId; includedInRuntime = $false; binaryCount = $entries.Count }
}
