@echo off
chcp 65001 >nul
setlocal
title Build Game - Larva Engine
echo === Build du Game ===
echo.

if not exist "Release\Builder\larva-builder.exe" (
    echo Le Builder n'est pas compile. Lancement du bootstrap...
    call bootstrap_builder.bat
    if %errorlevel% neq 0 exit /b 1
    echo.
)

Release\Builder\larva-builder.exe configs\game_config.json

if %errorlevel% neq 0 (
    color 0C
    echo.
    echo Echec du build Game
    color 07
    pause
    exit /b 1
)

echo.
color 0A
echo === Build Game termine ! ===
color 07
echo.
pause
