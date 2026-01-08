$sourceDir = "c:\Users\Sumicro\Desktop\Larva-Engine\Dependencies\OpenAL\bin"
$targetDir = "c:\Users\Sumicro\Desktop\Larva-Engine\projects\Healer-Adventure\Release\Client"

if (Test-Path "$sourceDir\soft_oal.dll") {
    Copy-Item -Path "$sourceDir\soft_oal.dll" -Destination "$targetDir\OpenAL32.dll" -Force
    Write-Host "Copied soft_oal.dll as OpenAL32.dll"
}

if (Test-Path "$sourceDir\soft_oal.dll") {
    Copy-Item -Path "$sourceDir\soft_oal.dll" -Destination "$targetDir\soft_oal.dll" -Force
    Write-Host "Copied soft_oal.dll"
}

Write-Host "DLL files copied successfully"
