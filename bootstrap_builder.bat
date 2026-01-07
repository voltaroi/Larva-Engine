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
    for /d %%D in (clang+llvm-*) do (
      xcopy /E /I /Y "%%D\*" "%DEPS%\Compiler\clang\" >nul || goto ERROR
      rmdir /S /Q "%%D"
    )
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
REM === OpenAL ===
REM =====================================================
if not exist "%DEPS%\openal\lib\openal.lib" (
  call :InstallOpenAL
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


REM === Copie des headers dans le bon dossier (GL/) ===
if not exist "%DEPS%\freeglut\include\GL" mkdir "%DEPS%\freeglut\include\GL"
xcopy /E /I /Y /S "freeglut-temp\freeglut-3.4.0\include\GL\*.h*" "%DEPS%\freeglut\include\GL\"



REM === Compilation manuelle de FreeGLUT en statique avec clang++ ===
set CLANG_BIN="%DEPS%\Compiler\clang\bin"
set FREEGLUT_SRC=freeglut-temp\freeglut-3.4.0\src
set OBJ_DIR=freeglut-temp\freeglut-3.4.0\obj
if exist %OBJ_DIR% rmdir /S /Q %OBJ_DIR%
mkdir %OBJ_DIR%
echo [DEBUG] INCLUDES: -I"%DEPS%\freeglut\include"
for %%f in (%FREEGLUT_SRC%\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\freeglut\include" -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\freeglut\lib\freeglut_static.lib" %OBJ_DIR%\*.obj || goto ERROR

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

REM === Copie des headers dans le bon dossier (GL/) ===
if not exist "%DEPS%\glew\include\GL" mkdir "%DEPS%\glew\include\GL"
xcopy /E /I /Y /S "glew-temp\glew-2.3.0\include\GL\*.h*" "%DEPS%\glew\include\GL\"

REM === Compilation manuelle de GLEW en statique avec clang++ ===
set CLANG_BIN="%DEPS%\Compiler\clang\bin"
set GLEW_SRC=glew-temp\glew-2.3.0\src
set OBJ_DIR=glew-temp\glew-2.3.0\obj
if exist %OBJ_DIR% rmdir /S /Q %OBJ_DIR%
mkdir %OBJ_DIR%
for %%f in (%GLEW_SRC%\glew.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\glew\include" -DGLEW_STATIC -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\glew\lib\glew32.lib" %OBJ_DIR%\*.obj || goto ERROR

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
xcopy /E /I /Y /S "freetype-temp\freetype-windows-binaries-2.14.1\include" "%DEPS%\freetype\include" >nul


REM === Pas de compilation Freetype : pas de sources dans l'archive binaire ===
echo [INFO] Archive Freetype binaire, pas de compilation.

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
xcopy /E /I /Y /S "zlib-temp\zlib-1.2.13\*.h*" "%DEPS%\zlib\include"
if exist "zlib-temp\zlib-1.2.13\zconf.h.in" copy /Y "zlib-temp\zlib-1.2.13\zconf.h.in" "%DEPS%\zlib\include\zconf.h"

REM === Compilation manuelle de Zlib en statique avec clang++ ===
set CLANG_BIN="%DEPS%\Compiler\clang\bin"
set ZLIB_SRC=zlib-temp\zlib-1.2.13
set OBJ_DIR=zlib-temp\zlib-1.2.13\obj
if exist %OBJ_DIR% rmdir /S /Q %OBJ_DIR%
mkdir %OBJ_DIR%
for %%f in (%ZLIB_SRC%\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\zlib\include" -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\zlib\lib\zlibstatic.lib" %OBJ_DIR%\*.obj || goto ERROR

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
xcopy /E /I /Y /S "libsndfile-temp\libsndfile-1.2.2\*.h*" "%DEPS%\libsndfile\include"


REM === Génération d'un config.h minimal pour libsndfile ===
if not exist "libsndfile-temp\libsndfile-1.2.2\src\config.h" (
  echo /* config.h minimal pour build statique */ > "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define HAVE_STDINT_H 1 >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define HAVE_SYS_TYPES_H 1 >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define PACKAGE_NAME "libsndfile" >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define PACKAGE_VERSION "1.2.2" >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define OS_IS_WIN32 1 >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #include ^<io.h^> >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #ifndef access >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define access _access >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #endif >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #ifndef R_OK >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define R_OK 4 >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #endif >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #ifndef W_OK >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define W_OK 2 >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #endif >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #ifndef X_OK >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define X_OK 1 >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #endif >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #ifndef F_OK >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #define F_OK 0 >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
  echo #endif >> "libsndfile-temp\libsndfile-1.2.2\src\config.h"
)

REM === Génération d'un sfconfig.h minimal pour Windows (little-endian) ===
echo /* sfconfig.h minimal pour Windows little-endian - auto-generated */ > "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo #ifndef SFCONFIG_USER >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo #define SFCONFIG_USER >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo #define CPU_IS_LITTLE_ENDIAN 1 >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo #define CPU_IS_BIG_ENDIAN 0 >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo /* Assume x86/x86_64 host on Windows */ >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo #define CPU_IS_X86 1 >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo #define CPU_IS_X86_64 1 >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
echo #endif /* SFCONFIG_USER */ >> "libsndfile-temp\libsndfile-1.2.2\src\sfconfig.h"
set CLANG_BIN="%DEPS%\Compiler\clang\bin"
set SNDFILE_SRC=libsndfile-temp\libsndfile-1.2.2\src
set OBJ_DIR=libsndfile-temp\libsndfile-1.2.2\obj
if exist %OBJ_DIR% rmdir /S /Q %OBJ_DIR%
mkdir %OBJ_DIR%
for %%f in (%SNDFILE_SRC%\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\libsndfile\include" -I"libsndfile-temp\libsndfile-1.2.2\src" -I"libsndfile-temp\libsndfile-1.2.2\include" -DPACKAGE_NAME=\"libsndfile\" -DPACKAGE_VERSION=\"1.2.2\" -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\libsndfile\lib\sndfile.lib" %OBJ_DIR%\*.obj || goto ERROR

del libsndfile-1.2.2.zip
rmdir /S /Q libsndfile-temp
exit /b 0

:InstallAssimp

echo [INFO] Installation d'Assimp...
rmdir /S /Q assimp-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o assimp-5.3.0.zip ^
  https://github.com/assimp/assimp/archive/refs/tags/v5.3.0.zip || goto ERROR

powershell -Command "Expand-Archive assimp-5.3.0.zip assimp-temp" || goto ERROR
mkdir "%DEPS%\assimp\lib" 2>nul
mkdir "%DEPS%\assimp\include" 2>nul

xcopy /E /I /Y /S "assimp-temp\assimp-5.3.0\include" "%DEPS%\assimp\include" >nul
xcopy /E /I /Y /S "assimp-temp\assimp-5.3.0\code" "%DEPS%\assimp\include\code" >nul


REM === Génération de config.h à partir de config.h.in (suppression des #cmakedefine) ===
if exist "%DEPS%\assimp\include\assimp\config.h" del "%DEPS%\assimp\include\assimp\config.h"
findstr /V "#cmakedefine" "assimp-temp\assimp-5.3.0\include\assimp\config.h.in" > "%DEPS%\assimp\include\assimp\config.h"

REM === Génération automatique de revision.h minimal si absent ===
if not exist assimp-temp\assimp-5.3.0\include\assimp\revision.h (
  echo // Auto-generated revision.h > assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define VER_MAJOR 5 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define VER_MINOR 3 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define VER_PATCH 0 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define GitVersion 0 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define GitBranch 0 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define ASSIMP_REVISION "v5.3.0" >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
)

REM === Copier revision.h dans include/assimp ET code/ ===
if exist assimp-temp\assimp-5.3.0\include\assimp\revision.h copy /Y assimp-temp\assimp-5.3.0\include\assimp\revision.h assimp-temp\assimp-5.3.0\code\revision.h >nul
if exist assimp-temp\assimp-5.3.0\include\assimp\revision.h copy /Y assimp-temp\assimp-5.3.0\include\assimp\revision.h assimp-temp\assimp-5.3.0\code\Common\revision.h >nul

REM === Copie du dossier contrib pour les dépendances internes (utf8cpp, etc.) ===
xcopy /E /I /Y /S "assimp-temp\assimp-5.3.0\contrib" "%DEPS%\assimp\include\contrib" >nul

REM === Compilation manuelle d'Assimp en statique avec clang++ ===

set CLANG_BIN="%DEPS%\Compiler\clang\bin"
set ASSIMP_SRC=assimp-temp\assimp-5.3.0\code
set OBJ_DIR=assimp-temp\assimp-5.3.0\obj
if exist %OBJ_DIR% rmdir /S /Q %OBJ_DIR%
mkdir %OBJ_DIR%
echo [INFO] Compilation récursive des sources Assimp...
for /R %ASSIMP_SRC% %%f in (*.cpp) do (
  echo "%%f" | findstr /I /C:"AssetLib\\C4D\\C4DImporter.cpp" >nul
  if errorlevel 1 findstr /C:"#include \"AssimpPCH.h\"" "%%f" >nul
  if errorlevel 1 %CLANG_BIN%\clang++.exe -c "%%f" -I"%DEPS%\assimp\include" -I"assimp-temp\assimp-5.3.0\include\assimp" -I"assimp-temp\assimp-5.3.0\code" -I"assimp-temp\assimp-5.3.0\code\Common" -I"assimp-temp\assimp-5.3.0\contrib\pugixml\src" -I"%DEPS%\zlib\include" -I"assimp-temp\assimp-5.3.0\contrib\rapidjson\include" -I"assimp-temp\assimp-5.3.0\contrib\unzip" -I"assimp-temp\assimp-5.3.0\contrib" -I"assimp-temp\assimp-5.3.0\contrib\openddlparser\include" -DASSIMP_BUILD_NO_EXPORT -DASSIMP_BUILD_NO_OWN_ZLIB -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
)

%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\assimp\lib\assimp.lib" %OBJ_DIR%\*.obj || goto ERROR

del assimp-5.3.0.zip
rmdir /S /Q assimp-temp
echo [INFO] Compilation Assimp terminée et librairie statique générée.
exit /b 0

:InstallOpenAL
echo [INFO] Installation OpenAL...
rmdir /S /Q OpenAL-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o openal-soft-1.25.0-bin.zip ^
 https://openal-soft.org/openal-binaries/openal-soft-1.25.0-bin.zip || goto ERROR

powershell -Command "Expand-Archive openal-soft-1.25.0-bin.zip OpenAL-temp" || goto ERROR
mkdir "%DEPS%\OpenAL\lib" 2>nul
mkdir "%DEPS%\OpenAL\include" 2>nul

xcopy /E /I /Y /S "OpenAL-temp\openal-soft-1.25.0-bin\include\*" "%DEPS%\OpenAL\include"

del openal-soft-1.25.0-bin.zip
rmdir /S /Q OpenAL-temp
exit /b 0