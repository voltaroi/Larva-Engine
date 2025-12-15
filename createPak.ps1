# Script to create a custom PAK file from Assets directory
# Usage: .\createPak.ps1 <assets_directory> <output_pak_file>

param(
    [string]$assetsDir = "Assets",
    [string]$outputPak = "game.pak"
)

if (-not (Test-Path $assetsDir)) {
    Write-Error "Assets directory not found: $assetsDir"
    exit 1
}

Write-Host "[CreatePak] Creating PAK file: $outputPak from $assetsDir"

# Get all files recursively
$files = Get-ChildItem -Path $assetsDir -Recurse -File

if ($files.Count -eq 0) {
    Write-Error "No files found in $assetsDir"
    exit 1
}

Write-Host "[CreatePak] Found $($files.Count) files to pack"

# Open output file for binary writing
$stream = [System.IO.File]::Open($outputPak, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
$writer = New-Object System.IO.BinaryWriter($stream)

# Write PAK signature (PLAK = 0x4B414C50 in little-endian)
$writer.Write([uint32]0x4B414C50)

# Write number of files
$writer.Write([uint32]$files.Count)

# Calculate data offset (signature + count + all headers)
$dataOffset = 8  # signature (4) + file count (4)
foreach ($file in $files) {
    $relativePath = $file.FullName -replace [regex]::Escape($assetsDir), "" -replace "\\", "/" -replace "^/", ""
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($relativePath)
    $dataOffset += 2 + $pathBytes.Length + 8  # pathLength (2) + path + offset (4) + size (4)
}

# Write file headers
$currentOffset = $dataOffset
foreach ($file in $files) {
    $relativePath = $file.FullName -replace [regex]::Escape($assetsDir), "" -replace "\\", "/" -replace "^/", ""
    $fileSize = $file.Length
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($relativePath)
    
    # Write path length
    $writer.Write([uint16]$pathBytes.Length)
    
    # Write path
    $writer.Write($pathBytes)
    
    # Write offset and size
    $writer.Write([uint32]$currentOffset)
    $writer.Write([uint32]$fileSize)
    
    Write-Host "[CreatePak] Added: $relativePath (offset: $currentOffset, size: $fileSize)"
    
    $currentOffset += $fileSize
}

# Write file data
foreach ($file in $files) {
    $fileData = [System.IO.File]::ReadAllBytes($file.FullName)
    $writer.Write($fileData)
}

# Close file
$writer.Close()
$stream.Close()

Write-Host "[CreatePak] PAK file created successfully: $outputPak"
Write-Host "[CreatePak] Total size: $((Get-Item $outputPak).Length) bytes"
