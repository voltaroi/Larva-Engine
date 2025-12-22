@echo off
echo === Compilation du Builder (bootstrap) ===
echo.

if not exist "Release\Builder" mkdir "Release\Builder"

echo Compilation en cours...



REM Installer Clang/LLVM si absent
if not exist "Dependencies\Compiler\clang\bin\clang++.exe" (
    echo Clang/LLVM non trouve, installation en cours...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/clang+llvm-21.1.8-x86_64-pc-windows-msvc.tar.xz' -OutFile 'Dependencies/Compiler/clang_tmp.tar.xz'"
    tar -xf Dependencies/Compiler/clang_tmp.tar.xz -C Dependencies/Compiler/clang
    del Dependencies/Compiler/clang_tmp.tar.xz
    REM Déplacer le contenu à la racine si besoin
    if exist "Dependencies\Compiler\clang\clang+llvm-21.1.8-x86_64-pc-windows-msvc" (
        move Dependencies\Compiler\clang\clang+llvm-21.1.8-x86_64-pc-windows-msvc\* Dependencies\Compiler\clang\
        rmdir /S /Q Dependencies\Compiler\clang\clang+llvm-21.1.8-x86_64-pc-windows-msvc
    )
)

Dependencies\Compiler\clang\bin\clang++.exe -std=c++17 ^
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
pause
