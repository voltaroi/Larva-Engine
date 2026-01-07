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
set CMAKE_DIR=%DEPS%\Compiler\cmake
set CMAKE_BIN=%CMAKE_DIR%\bin\cmake.exe
set NINJA_DIR=%DEPS%\Compiler\ninja
set NINJA_EXE=%NINJA_DIR%\ninja.exe
set OPENAL_DIR=%DEPS%\OpenAL
set GLM_DIR=%DEPS%\glm
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
REM === CMAKE ===
REM =====================================================
if not exist "%CMAKE_BIN%" (
  call :InstallCMake
) else (
  echo [INFO] CMake déjà installé, utilisation existante.
)

REM =====================================================
REM === NINJA (GENERATEUR CMAKE) ===
REM =====================================================
if not exist "%NINJA_EXE%" (
  call :InstallNinja
) else (
  echo [INFO] Ninja déjà installé, utilisation existante.
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
REM === GLM (HEADER-ONLY) ===
REM =====================================================
if not exist "%GLM_DIR%\include\glm\glm.hpp" (
  call :InstallGLM
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
if not exist "%DEPS%\freetype\lib\freetype.lib" if not exist "%DEPS%\freetype\lib\freetyped.lib" (
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
if not exist "%OPENAL_DIR%\lib\OpenAL32.lib" (
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

:InstallCMake
echo [INFO] Installation de CMake...
rmdir /S /Q cmake-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o cmake.zip ^
  https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-windows-x86_64.zip || goto ERROR

powershell -Command "Expand-Archive cmake.zip cmake-temp" || goto ERROR
mkdir "%CMAKE_DIR%" 2>nul
for /d %%D in (cmake-temp\cmake-*-windows-x86_64) do xcopy /E /I /Y "%%D\*" "%CMAKE_DIR%\" >nul || goto ERROR

del cmake.zip
rmdir /S /Q cmake-temp
exit /b 0

:InstallNinja
echo [INFO] Installation de Ninja...
rmdir /S /Q ninja-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o ninja.zip ^
  https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip || goto ERROR

powershell -Command "Expand-Archive ninja.zip ninja-temp" || goto ERROR
mkdir "%NINJA_DIR%" 2>nul
copy /Y "ninja-temp\ninja.exe" "%NINJA_EXE%" >nul || goto ERROR

del ninja.zip
rmdir /S /Q ninja-temp
exit /b 0

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
for %%f in (%FREEGLUT_SRC%\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\freeglut\include" -DFREEGLUT_STATIC -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
for %%f in (%FREEGLUT_SRC%\mswin\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\freeglut\include" -DFREEGLUT_STATIC -o "%OBJ_DIR%\mswin_%%~nf.obj" || goto ERROR
%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\freeglut\lib\freeglut_static.lib" %OBJ_DIR%\*.obj || goto ERROR

REM === Alias debug/release names expected by linker ===
if not exist "%DEPS%\freeglut\lib\freeglut_staticd.lib" copy /Y "%DEPS%\freeglut\lib\freeglut_static.lib" "%DEPS%\freeglut\lib\freeglut_staticd.lib" >nul
if not exist "%DEPS%\freeglut\lib\freeglutd.lib" copy /Y "%DEPS%\freeglut\lib\freeglut_static.lib" "%DEPS%\freeglut\lib\freeglutd.lib" >nul

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
echo [INFO] Installation Freetype (build statique)...
rmdir /S /Q freetype-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o freetype-2.13.3.zip ^
  https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.zip || goto ERROR

powershell -Command "Expand-Archive freetype-2.13.3.zip freetype-temp" || goto ERROR
set "FT_SRC=freetype-temp\freetype-VER-2-13-3"
set "FT_BUILD=%FT_SRC%\build"
mkdir "%DEPS%\freetype\lib" 2>nul
mkdir "%DEPS%\freetype\include" 2>nul

set "DEPS_FWD=%DEPS:\=/%"
set "CLANG_CC=%DEPS_FWD%/Compiler/clang/bin/clang.exe"
set "CLANG_CXX=%DEPS_FWD%/Compiler/clang/bin/clang++.exe"
set "CLANG_RC=%DEPS_FWD%/Compiler/clang/bin/llvm-rc.exe"

"%CMAKE_BIN%" -G "Ninja" ^
  -S "%FT_SRC%" ^
  -B "%FT_BUILD%" ^
  -DCMAKE_C_COMPILER="%CLANG_CC%" ^
  -DCMAKE_CXX_COMPILER="%CLANG_CXX%" ^
  -DCMAKE_RC_COMPILER="%CLANG_RC%" ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DFT_DISABLE_BZIP2=ON ^
  -DFT_DISABLE_PNG=ON ^
  -DFT_DISABLE_HARFBUZZ=ON ^
  -DZLIB_LIBRARY="%DEPS%/zlib/lib/zlibstatic.lib" ^
  -DZLIB_INCLUDE_DIR="%DEPS%/zlib/include" ^
  -DCMAKE_INSTALL_PREFIX="%DEPS%/freetype" || goto ERROR

"%CMAKE_BIN%" --build "%FT_BUILD%" --config Release || goto ERROR
"%CMAKE_BIN%" --install "%FT_BUILD%" --config Release || goto ERROR

REM === Normaliser les noms de libs freetype ===
if exist "%DEPS%\freetype\lib\freetyped.lib" if not exist "%DEPS%\freetype\lib\freetype.lib" copy /Y "%DEPS%\freetype\lib\freetyped.lib" "%DEPS%\freetype\lib\freetype.lib" >nul
if not exist "%DEPS%\freetype\lib\freetype.lib" goto ERROR

del freetype-2.13.3.zip
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
xcopy /E /I /Y /S "libsndfile-temp\libsndfile-1.2.2\include\*.h*" "%DEPS%\libsndfile\include"

set "BUILD_DIR=libsndfile-temp\libsndfile-1.2.2\build"
set "DEPS_FWD=%DEPS:\=/%"
set "CLANG_CC=%DEPS_FWD%/Compiler/clang/bin/clang.exe"
set "CLANG_CXX=%DEPS_FWD%/Compiler/clang/bin/clang++.exe"
set "CLANG_RC=%DEPS_FWD%/Compiler/clang/bin/llvm-rc.exe"

"%CMAKE_BIN%" -G "Ninja" ^
  -S "libsndfile-temp\libsndfile-1.2.2" ^
  -B "%BUILD_DIR%" ^
  -DCMAKE_C_COMPILER="%CLANG_CC%" ^
  -DCMAKE_CXX_COMPILER="%CLANG_CXX%" ^
  -DCMAKE_RC_COMPILER="%CLANG_RC%" ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DBUILD_PROGRAMS=OFF ^
  -DBUILD_EXAMPLES=OFF ^
  -DBUILD_TESTING=OFF ^
  -DINSTALL_PKGCONFIG_MODULE=OFF ^
  -DINSTALL_CMAKE_CONFIG_MODULE=OFF ^
  -DENABLE_EXTERNAL_LIBS=OFF || goto ERROR

"%CMAKE_BIN%" --build "%BUILD_DIR%" --config Release || goto ERROR

set "FOUND_LIB="
for %%L in ("%BUILD_DIR%\src\libsndfile.lib" "%BUILD_DIR%\lib\Release\libsndfile.lib" "%BUILD_DIR%\Release\libsndfile.lib" "%BUILD_DIR%\lib\libsndfile.lib" "%BUILD_DIR%\sndfile.lib") do (
  if exist "%%~fL" set "FOUND_LIB=%%~fL"
)

if not defined FOUND_LIB goto ERROR
copy /Y "!FOUND_LIB!" "%DEPS%\libsndfile\lib\sndfile.lib" >nul || goto ERROR

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
echo #define ASSIMP_BUILD_NO_C4D_IMPORTER 1 >> "%DEPS%\assimp\include\assimp\config.h"
echo #define ASSIMP_BUILD_NO_IFC_IMPORTER 1 >> "%DEPS%\assimp\include\assimp\config.h"
echo #define ASSIMP_BUILD_NO_OPENGEX_IMPORTER 1 >> "%DEPS%\assimp\include\assimp\config.h"

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
  echo "%%f" | findstr /I /C:"AssetLib\\C4D" >nul
  if errorlevel 1 echo "%%f" | findstr /I /C:"AssetLib\\OpenGEX" >nul
  if errorlevel 1 echo "%%f" | findstr /I /C:"AssetLib\\IFC" >nul
  if errorlevel 1 findstr /C:"#include \"AssimpPCH.h\"" "%%f" >nul
  if errorlevel 1 %CLANG_BIN%\clang++.exe -c "%%f" -I"%DEPS%\assimp\include" -I"assimp-temp\assimp-5.3.0\include\assimp" -I"assimp-temp\assimp-5.3.0\code" -I"assimp-temp\assimp-5.3.0\code\Common" -I"assimp-temp\assimp-5.3.0\contrib\pugixml\src" -I"%DEPS%\zlib\include" -I"assimp-temp\assimp-5.3.0\contrib\rapidjson\include" -I"assimp-temp\assimp-5.3.0\contrib\unzip" -I"assimp-temp\assimp-5.3.0\contrib" -I"assimp-temp\assimp-5.3.0\contrib\openddlparser\include" -DASSIMP_BUILD_NO_EXPORT -DASSIMP_BUILD_NO_OWN_ZLIB -DASSIMP_BUILD_NO_C4D_IMPORTER=1 -DASSIMP_BUILD_NO_IFC_IMPORTER=1 -DASSIMP_BUILD_NO_OPENGEX_IMPORTER=1 -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
)

REM === Compilation des sources unzip nécessaires ===
set UNZIP_SRC=assimp-temp\assimp-5.3.0\contrib\unzip
for %%f in (%UNZIP_SRC%\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\zlib\include" -I"%UNZIP_SRC%" -o "%OBJ_DIR%\unzip_%%~nf.obj" || goto ERROR

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
mkdir "%OPENAL_DIR%\lib" 2>nul
mkdir "%OPENAL_DIR%\include" 2>nul
mkdir "%OPENAL_DIR%\bin" 2>nul

xcopy /E /I /Y /S "OpenAL-temp\openal-soft-1.25.0-bin\include\*" "%OPENAL_DIR%\include" >nul

set "OPENAL_LIB_SRC="
for %%L in ("OpenAL-temp\openal-soft-1.25.0-bin\libs\Win64\OpenAL32.lib" "OpenAL-temp\openal-soft-1.25.0-bin\libs\OpenAL32.lib" "OpenAL-temp\openal-soft-1.25.0-bin\lib\Win64\OpenAL32.lib" "OpenAL-temp\openal-soft-1.25.0-bin\lib\OpenAL32.lib") do (
  if exist "%%~fL" set "OPENAL_LIB_SRC=%%~fL"
)
if not defined OPENAL_LIB_SRC goto ERROR
copy /Y "%OPENAL_LIB_SRC%" "%OPENAL_DIR%\lib\OpenAL32.lib" >nul || goto ERROR

if exist "OpenAL-temp\openal-soft-1.25.0-bin\bin\Win64" xcopy /Y /I "OpenAL-temp\openal-soft-1.25.0-bin\bin\Win64\*.dll" "%OPENAL_DIR%\bin\" >nul

del openal-soft-1.25.0-bin.zip
rmdir /S /Q OpenAL-temp
exit /b 0

:InstallGLM
echo [INFO] Installation GLM...
rmdir /S /Q glm-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o glm-0.9.9.8.zip ^
  https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.zip || goto ERROR

powershell -Command "Expand-Archive glm-0.9.9.8.zip glm-temp" || goto ERROR
mkdir "%GLM_DIR%\include" 2>nul
xcopy /E /I /Y /S "glm-temp\glm-0.9.9.8\glm" "%GLM_DIR%\include\glm" >nul

del glm-0.9.9.8.zip
rmdir /S /Q glm-temp
exit /b 0