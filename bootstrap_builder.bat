@echo off
echo === Compilation du Builder (bootstrap) ===
echo.
echo Ce script compile le Builder une seule fois avec votre compilateur actuel.
echo Apres cela, le Builder pourra se compiler lui-meme !
echo.

if not exist "Release\Builder" mkdir "Release\Builder"

echo Compilation en cours...

g++ -std=c++17 ^
    Builder/main.cpp ^
    Builder/Builder.cpp ^
    Builder/ProjectConfig.cpp ^
    -o Release/Builder/larva-builder.exe

if %errorlevel% neq 0 (
    color 0C
    echo.
    echo ERREUR: Echec de la compilation du Builder
    echo.
    color 07
    pause
    exit /b 1
)

echo.
color 0A
echo ========================================
echo  Builder compile avec succes !
echo ========================================
echo.
color 07
echo Vous pouvez maintenant utiliser:
echo   Release\Builder\larva-builder.exe configs\engine_config.json
echo   Release\Builder\larva-builder.exe configs\game_config.json
echo   Release\Builder\larva-builder.exe configs\server_config.json
echo.
echo Le Builder detectera automatiquement votre compilateur (GCC, Clang, ou MSVC)
echo.
pause
