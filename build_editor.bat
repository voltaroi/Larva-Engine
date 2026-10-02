@echo off
chcp 65001 >nul
setlocal
set ROOT=%~dp0
pushd "%ROOT%"
title Build Larva Editor
echo === Larva Editor build ===
echo.
if not exist "Release\Builder\larva-builder.exe" (
    echo The builder is not compiled. Launching bootstrap...
    call bootstrap_builder.bat
    if %errorlevel% neq 0 (
        popd
        exit /b 1
    )
    echo.
)
Release\Builder\larva-builder.exe configs\editor_config.json
if %errorlevel% neq 0 (
    color 0C
    echo.
    echo Editor build failed
    color 07
    pause
    popd
    exit /b 1
)
copy /Y "Dependencies\OpenAL\bin\soft_oal.dll" "Release\Editor\OpenAL32.dll" >nul
echo.
color 0A
echo === Larva Editor ready: Release\Editor\larva-editor.exe ===
color 07
if /I "%1"=="run" start "" "Release\Editor\larva-editor.exe"
popd
