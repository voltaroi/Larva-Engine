@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion
title Larva-Engine - Bootstrap Builder

echo ========================================
echo  Bootstrap Builder - Larva Engine
echo ========================================
echo.

REM === ROOT ===
set ROOT=%~dp0
cd /d "%ROOT%"

REM === DOSSIERS ===
set DEPS=%ROOT%Dependencies
mkdir "%DEPS%" 2>nul
mkdir "%DEPS%\Compiler\clang" 2>nul
mkdir Release\Builder 2>nul

REM =====================================================
REM === CLANG / LLVM (OFFICIEL LLVM MSVC BUILD)
REM =====================================================
if not exist "%DEPS%\Compiler\clang\bin\clang++.exe" (
    echo [INFO] Installation de Clang/LLVM...
    "C:\Windows\System32\curl.exe" -L --fail -o clang.tar.xz ^
        https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/clang+llvm-21.1.8-x86_64-pc-windows-msvc.tar.xz || goto ERROR

    tar -xf clang.tar.xz || goto ERROR
    xcopy /E /I /Y clang+llvm-* "%DEPS%\Compiler\clang" >nul
    rmdir /S /Q clang+llvm-* 
    del clang.tar.xz
) else (
    echo [INFO] Clang déjà installé, utilisation existante.
)

REM =====================================================
REM === FREEGLUT ===
REM =====================================================
if not exist "%DEPS%\freeglut\lib\freeglut_static.lib" (
    call :InstallFreeGLUT
)

REM =====================================================
REM === GLEW ===
REM =====================================================
if not exist "%DEPS%\glew\lib\glew32.lib" (
    call :InstallGLEW
)

REM =====================================================
REM === ZLIB ===
REM =====================================================
if not exist "%DEPS%\zlib\lib\zlibstatic.lib" (
    call :InstallZlib
)

REM =====================================================
REM === FREETYPE ===
REM =====================================================
if not exist "%DEPS%\freetype\lib\freetype.lib" (
  call :InstallFreetype
)

REM =====================================================
REM === LIBSNDFILE ===
REM =====================================================
if not exist "%DEPS%\libsndfile\lib\sndfile.lib" (
  call :InstallLibsndfile
)

REM =====================================================
REM === ASSIMP ===
REM =====================================================
if not exist "%DEPS%\assimp\lib\assimp.lib" (
  call :InstallAssimp
)

REM =====================================================
REM === COMPILATION BUILDER
REM =====================================================
echo.
echo [INFO] Compilation du builder...

"%DEPS%\Compiler\clang\bin\clang++.exe" ^
    -std=c++17 ^
    Builder/main.cpp ^
    Builder/Builder.cpp ^
    Builder/ProjectConfig.cpp ^
    -o Release\Builder\larva-builder.exe || goto ERROR

echo.
echo ========================================
echo  Builder compilé avec succès ✔
echo ========================================
pause
exit /b 0

:ERROR
color 0C
echo.
echo [ERREUR] Le bootstrap a échoué.
echo Vérifie ta connexion ou un antivirus bloquant curl.
echo.
pause
exit /b 1

REM =====================================================
REM === FONCTIONS ===
REM =====================================================

:InstallFreeGLUT
echo [INFO] Installation FreeGLUT...
rmdir /S /Q freeglut-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o freeglut.zip ^
  https://github.com/freeglut/freeglut/archive/refs/tags/v3.4.0.zip || goto ERROR

powershell -Command "Expand-Archive freeglut.zip freeglut-temp" || goto ERROR
mkdir "%DEPS%\freeglut\lib" 2>nul
mkdir "%DEPS%\freeglut\include" 2>nul
xcopy /E /I /Y /S "freeglut-temp\freeglut-3.4.0\*.h*" "%DEPS%\freeglut\include\"

del freeglut.zip
rmdir /S /Q freeglut-temp
exit /b 0

:InstallGLEW
echo [INFO] Installation GLEW...
rmdir /S /Q glew-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o glew-2.3.0.zip ^
  https://github.com/nigels-com/glew/releases/download/glew-2.3.0/glew-2.3.0.zip || goto ERROR

powershell -Command "Expand-Archive glew-2.3.0.zip glew-temp" || goto ERROR
mkdir "%DEPS%\glew\lib" 2>nul
mkdir "%DEPS%\glew\include" 2>nul
xcopy /E /I /Y /S "glew-temp\glew-2.3.0\*.h*" "%DEPS%\glew\include\"

del glew-2.3.0.zip
rmdir /S /Q glew-temp
exit /b 0

:InstallFreetype
echo [INFO] Installation Freetype...
rmdir /S /Q freetype-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o freetype-windows-binaries-2.14.1.zip ^
  https://github.com/ubawurinna/freetype-windows-binaries/archive/refs/tags/v2.14.1.zip || goto ERROR

powershell -Command "Expand-Archive freetype-windows-binaries-2.14.1.zip freetype-temp" || goto ERROR
mkdir "%DEPS%\freetype\lib" 2>nul
mkdir "%DEPS%\freetype\include" 2>nul
xcopy /E /I /Y /S "freetype-temp\freetype-windows-binaries-2.14.1\include" "%DEPS%\freetype\include\" >nul

del freetype-windows-binaries-2.14.1.zip
rmdir /S /Q freetype-temp
exit /b 0

:InstallZlib
echo [INFO] Installation zlib...
rmdir /S /Q zlib-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o zlib.zip ^
  https://github.com/ShiftMediaProject/zlib/archive/refs/tags/v1.2.13.zip || goto ERROR

powershell -Command "Expand-Archive zlib.zip zlib-temp" || goto ERROR
mkdir "%DEPS%\zlib\lib" 2>nul
mkdir "%DEPS%\zlib\include" 2>nul
copy "zlib-temp\zlib-1.2.13\lib\zlibstatic.lib" "%DEPS%\zlib\lib\"
xcopy /E /I /Y /S "zlib-temp\zlib-1.2.13\*.h*" "%DEPS%\zlib\include\"

del zlib.zip
rmdir /S /Q zlib-temp
exit /b 0

:InstallLibsndfile
echo [INFO] Installation libsndfile...
rmdir /S /Q libsndfile-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o libsndfile-1.2.2.zip ^
  https://github.com/libsndfile/libsndfile/archive/refs/tags/1.2.2.zip || goto ERROR

powershell -Command "Expand-Archive libsndfile-1.2.2.zip libsndfile-temp" || goto ERROR
mkdir "%DEPS%\libsndfile\lib" 2>nul
mkdir "%DEPS%\libsndfile\include" 2>nul
xcopy /E /I /Y /S "libsndfile-temp\libsndfile-1.2.2\*.h*" "%DEPS%\libsndfile\include\"

del libsndfile-1.2.2.zip
rmdir /S /Q libsndfile-temp
exit /b 0

:InstallAssimp
echo [INFO] Installation d'Assimp...
rmdir /S /Q assimp-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o assimp.zip ^
  https://github.com/assimp/assimp/archive/refs/tags/v5.2.5.zip || goto ERROR

powershell -Command "Expand-Archive assimp.zip assimp-temp" || goto ERROR
mkdir "%DEPS%\assimp\lib" 2>nul
mkdir "%DEPS%\assimp\include" 2>nul
xcopy /E /I /Y /S "assimp-temp\assimp-5.2.5\*.h*" "%DEPS%\assimp\include\"

del assimp.zip
rmdir /S /Q assimp-temp
exit /b 0
