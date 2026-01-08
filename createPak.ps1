param(
    [string]$assetsDir = "Assets",
    [string]$outputPak = "game.pak"
)

if (-not (Test-Path $assetsDir)) {
    Write-Error "Assets directory not found: $assetsDir"
    exit 1
}

Write-Host "[CreatePak] Creating PAK file: $outputPak from $assetsDir"

$files = Get-ChildItem -Path $assetsDir -Recurse -File

if ($files.Count -eq 0) {
    Write-Error "No files found in $assetsDir"
    exit 1
}

Write-Host "[CreatePak] Found $($files.Count) files to pack"

$stream = [System.IO.File]::Open($outputPak, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
$writer = New-Object System.IO.BinaryWriter($stream)

$writer.Write([uint32]0x4B414C50)

$writer.Write([uint32]$files.Count)

$dataOffset = 8
foreach ($file in $files) {
    $relativePath = $file.FullName -replace [regex]::Escape($assetsDir), "" -replace "\\", "/" -replace "^/", ""
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($relativePath)
    $dataOffset += 2 + $pathBytes.Length + 8
}

$currentOffset = $dataOffset
foreach ($file in $files) {
    $relativePath = $file.FullName -replace [regex]::Escape($assetsDir), "" -replace "\\", "/" -replace "^/", ""
    $fileSize = $file.Length
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($relativePath)
    
    $writer.Write([uint16]$pathBytes.Length)
    
    $writer.Write($pathBytes)
    
    $writer.Write([uint32]$currentOffset)
    $writer.Write([uint32]$fileSize)
    
    Write-Host "[CreatePak] Added: $relativePath (offset: $currentOffset, size: $fileSize)"
    
    $currentOffset += $fileSize
}

foreach ($file in $files) {
    $fileData = [System.IO.File]::ReadAllBytes($file.FullName)
    $writer.Write($fileData)
}

$writer.Close()
$stream.Close()

Write-Host "[CreatePak] PAK file created successfully: $outputPak"
Write-Host "[CreatePak] Total size: $((Get-Item $outputPak).Length) bytes"
