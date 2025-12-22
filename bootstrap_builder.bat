REM Create the Dependencies/Compiler/ folder if it doesn't exist.
if not exist "Dependencies\Compiler" mkdir "Dependencies\Compiler"

REM Remove clang folder if it exists to avoid conflicts
if exist "Dependencies\Compiler\clang" rmdir /S /Q "Dependencies\Compiler\clang"

REM Create the Dependencies/Compiler/clang/ folder
mkdir "Dependencies\Compiler\clang"
@echo off
echo === Compilation du Builder (bootstrap) ===
echo.

if not exist "Release\Builder" mkdir "Release\Builder"

echo Compilation en cours...

REM Install Clang/LLVM if missing
if not exist "Dependencies\Compiler\clang\bin\clang++.exe" (
    echo Clang/LLVM non trouve, installation en cours...
    if not exist "Dependencies/Compiler/clang_tmp.tar.xz" (
        powershell -Command "Invoke-WebRequest -Uri 'https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/clang+llvm-21.1.8-x86_64-pc-windows-msvc.tar.xz' -OutFile 'Dependencies/Compiler/clang_tmp.tar.xz'"
    ) else (
        echo Archive deja presente, pas de telechargement.
    )
    tar -xf Dependencies/Compiler/clang_tmp.tar.xz -C Dependencies/Compiler/clang
    del Dependencies/Compiler/clang_tmp.tar.xz
    if exist "Dependencies\Compiler\clang\clang+llvm-21.1.8-x86_64-pc-windows-msvc" (
        xcopy /E /I /Y "Dependencies\Compiler\clang\clang+llvm-21.1.8-x86_64-pc-windows-msvc\*" "Dependencies\Compiler\clang\"
        rmdir /S /Q "Dependencies\Compiler\clang\clang+llvm-21.1.8-x86_64-pc-windows-msvc"
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
