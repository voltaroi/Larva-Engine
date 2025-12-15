@echo off
chcp 65001 >nul
setlocal
title Build All - Larva Engine
echo ========================================
echo === Build de TOUT le Larva Engine ===
echo ========================================
echo.

if not exist "Release\Builder\larva-builder.exe" (
    echo Le Builder n'est pas compile. Lancement du bootstrap...
    call bootstrap_builder.bat
    if %errorlevel% neq 0 exit /b 1
    echo.
)

echo [1/3] Build de l'Editor...
Release\Builder\larva-builder.exe configs\engine_config.json
if %errorlevel% neq 0 (
    color 0C
    echo Echec du build Editor
    color 07
    pause
    exit /b 1
)

echo.
echo [2/3] Build du Game...
Release\Builder\larva-builder.exe configs\game_config.json
if %errorlevel% neq 0 (
    color 0C
    echo Echec du build Game
    color 07
    pause
    exit /b 1
)

echo.
echo [3/3] Build du Server...
Release\Builder\larva-builder.exe configs\server_config.json
if %errorlevel% neq 0 (
    color 0C
    echo Echec du build Server
    color 07
    pause
    exit /b 1
)

echo.
color 0A
echo ========================================
echo === TOUS LES BUILDS SONT TERMINES ! ===
echo ========================================
color 07
echo.
echo - Editor : Release\Engine\editor.exe
echo - Game   : Release\Game\game.exe
echo - Server : Release\Server\server.exe
echo.
pause
