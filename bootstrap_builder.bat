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
set ZIG_DIR=%DEPS%\Compiler\zig
set ZIG_EXE=%ZIG_DIR%\zig.exe
set ZIG_CXX=%ZIG_DIR%\zig-cxx.bat
set OPENAL_DIR=%DEPS%\OpenAL
set GLM_DIR=%DEPS%\glm
mkdir "%DEPS%" 2>nul
mkdir "%DEPS%\Compiler\clang" 2>nul
mkdir "%DEPS%\Compiler\zig" 2>nul
mkdir Release\Builder 2>nul

REM =====================================================
REM === CLANG / LLVM (OFFICIAL LLVM MSVC BUILD)
REM =====================================================
if not exist "%DEPS%\Compiler\clang\bin\clang++.exe" (
  echo [INFO] Installing Clang/LLVM...
    "C:\Windows\System32\curl.exe" -L --fail -o clang.tar.xz ^
        https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/clang+llvm-21.1.8-x86_64-pc-windows-msvc.tar.xz || goto ERROR

    tar -xf clang.tar.xz || goto ERROR
    for /d %%D in (clang+llvm-*) do (
      xcopy /E /I /Y "%%D\*" "%DEPS%\Compiler\clang\" >nul || goto ERROR
      rmdir /S /Q "%%D"
    )
    del clang.tar.xz
) else (
  echo [INFO] Clang already present, using existing install.
)

REM =====================================================
REM === ZIG (LINUX CROSS-COMPILER) ===
REM =====================================================
if not exist "%ZIG_EXE%" (
  call :InstallZig
) else (
  echo [INFO] Zig already present, using existing install.
)
call :EnsureZigCxxWrapper

REM =====================================================
REM === CMAKE ===
REM =====================================================
if not exist "%CMAKE_BIN%" (
  call :InstallCMake
) else (
  echo [INFO] CMake already present, using existing install.
)

REM =====================================================
REM === NINJA (GENERATEUR CMAKE) ===
REM =====================================================
if not exist "%NINJA_EXE%" (
  call :InstallNinja
) else (
  echo [INFO] Ninja already present, using existing install.
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
set "FREETYPE_NEEDS_INSTALL=0"
if exist "%DEPS%\freetype\lib\freetype.lib" (
  "%DEPS%\Compiler\clang\bin\llvm-readobj.exe" --coff-directives "%DEPS%\freetype\lib\freetype.lib" | findstr /C:"msvcrtd.lib" >nul
  if not errorlevel 1 set "FREETYPE_NEEDS_INSTALL=1"
) else (
  if exist "%DEPS%\freetype\lib\freetyped.lib" (
    "%DEPS%\Compiler\clang\bin\llvm-readobj.exe" --coff-directives "%DEPS%\freetype\lib\freetyped.lib" | findstr /C:"msvcrtd.lib" >nul
    if not errorlevel 1 set "FREETYPE_NEEDS_INSTALL=1"
  ) else (
    set "FREETYPE_NEEDS_INSTALL=1"
  )
)
if "%FREETYPE_NEEDS_INSTALL%"=="1" (
  call :InstallFreetype
)

REM =====================================================
REM === LIBSNDFILE ===
REM =====================================================
set "LIBSNDFILE_NEEDS_INSTALL=0"
if exist "%DEPS%\libsndfile\lib\sndfile.lib" (
  "%DEPS%\Compiler\clang\bin\llvm-readobj.exe" --coff-directives "%DEPS%\libsndfile\lib\sndfile.lib" | findstr /C:"msvcrtd.lib" >nul
  if not errorlevel 1 set "LIBSNDFILE_NEEDS_INSTALL=1"
) else (
  set "LIBSNDFILE_NEEDS_INSTALL=1"
)
if "%LIBSNDFILE_NEEDS_INSTALL%"=="1" (
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
echo [INFO] Building the builder...

"%DEPS%\Compiler\clang\bin\clang++.exe" ^
    -std=c++17 ^
    Builder/main.cpp ^
    Builder/Builder.cpp ^
    Builder/ProjectConfig.cpp ^
    -o Release\Builder\larva-builder.exe || goto ERROR

echo.
echo ========================================
echo  Builder built successfully
echo ========================================
pause
exit /b 0

:ERROR
color 0C
echo.
echo [ERROR] Bootstrap failed.
echo Check your connection or an antivirus blocking curl.
echo.
pause
exit /b 1

REM =====================================================
REM === FONCTIONS ===
REM =====================================================

:InstallCMake
echo [INFO] Installing CMake...
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
echo [INFO] Installing Ninja...
rmdir /S /Q ninja-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o ninja.zip ^
  https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip || goto ERROR

powershell -Command "Expand-Archive ninja.zip ninja-temp" || goto ERROR
mkdir "%NINJA_DIR%" 2>nul
copy /Y "ninja-temp\ninja.exe" "%NINJA_EXE%" >nul || goto ERROR

del ninja.zip
rmdir /S /Q ninja-temp
exit /b 0

:InstallZig
echo [INFO] Installing Zig...
rmdir /S /Q zig-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o zig.zip ^
  https://ziglang.org/download/0.15.2/zig-x86_64-windows-0.15.2.zip || goto ERROR

powershell -Command "Expand-Archive zig.zip zig-temp" || goto ERROR
mkdir "%ZIG_DIR%" 2>nul
for /d %%D in (zig-temp\zig-*-windows-*) do xcopy /E /I /Y "%%D\*" "%ZIG_DIR%\" >nul || goto ERROR

del zig.zip
rmdir /S /Q zig-temp
exit /b 0

:EnsureZigCxxWrapper
if exist "%ZIG_CXX%" (
  exit /b 0
)
echo [INFO] Creating Zig Linux wrapper...
(
  echo @echo off
  echo setlocal
  echo "%%~dp0zig.exe" c++ -target x86_64-linux-musl %%*
  echo exit /b %%errorlevel%%
) > "%ZIG_CXX%" || goto ERROR
exit /b 0

:InstallFreeGLUT
echo [INFO] Installing FreeGLUT...
rmdir /S /Q freeglut-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o freeglut.zip ^
  https://github.com/freeglut/freeglut/archive/refs/tags/v3.4.0.zip || goto ERROR

powershell -Command "Expand-Archive freeglut.zip freeglut-temp" || goto ERROR
mkdir "%DEPS%\freeglut\lib" 2>nul
mkdir "%DEPS%\freeglut\include" 2>nul


REM === Copy headers into GL/ ===
if not exist "%DEPS%\freeglut\include\GL" mkdir "%DEPS%\freeglut\include\GL"
xcopy /E /I /Y /S "freeglut-temp\freeglut-3.4.0\include\GL\*.h*" "%DEPS%\freeglut\include\GL\"



REM === Manual static FreeGLUT build with clang++ ===
set CLANG_BIN="%DEPS%\Compiler\clang\bin"
set FREEGLUT_SRC=freeglut-temp\freeglut-3.4.0\src
set OBJ_DIR=freeglut-temp\freeglut-3.4.0\obj
if exist %OBJ_DIR% rmdir /S /Q %OBJ_DIR%
mkdir %OBJ_DIR%
echo [DEBUG] INCLUDES: -I"%DEPS%\freeglut\include"
for %%f in (%FREEGLUT_SRC%\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\freeglut\include" -DFREEGLUT_STATIC -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
for %%f in (%FREEGLUT_SRC%\mswin\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\freeglut\include" -DFREEGLUT_STATIC -o "%OBJ_DIR%\mswin_%%~nf.obj" || goto ERROR
%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\freeglut\lib\freeglut_static.lib" %OBJ_DIR%\*.obj || goto ERROR

REM === Debug/release aliases expected by linker ===
if not exist "%DEPS%\freeglut\lib\freeglut_staticd.lib" copy /Y "%DEPS%\freeglut\lib\freeglut_static.lib" "%DEPS%\freeglut\lib\freeglut_staticd.lib" >nul
if not exist "%DEPS%\freeglut\lib\freeglutd.lib" copy /Y "%DEPS%\freeglut\lib\freeglut_static.lib" "%DEPS%\freeglut\lib\freeglutd.lib" >nul

del freeglut.zip
rmdir /S /Q freeglut-temp
exit /b 0

:InstallGLEW
echo [INFO] Installing GLEW...
rmdir /S /Q glew-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o glew-2.3.0.zip ^
  https://github.com/nigels-com/glew/releases/download/glew-2.3.0/glew-2.3.0.zip || goto ERROR

powershell -Command "Expand-Archive glew-2.3.0.zip glew-temp" || goto ERROR
mkdir "%DEPS%\glew\lib" 2>nul
mkdir "%DEPS%\glew\include" 2>nul

REM === Copy headers into GL/ ===
if not exist "%DEPS%\glew\include\GL" mkdir "%DEPS%\glew\include\GL"
xcopy /E /I /Y /S "glew-temp\glew-2.3.0\include\GL\*.h*" "%DEPS%\glew\include\GL\"

REM === Manual static GLEW build with clang++ ===
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
echo [INFO] Installing Freetype (static build)...
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
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DFT_DISABLE_BZIP2=ON ^
  -DFT_DISABLE_PNG=ON ^
  -DFT_DISABLE_HARFBUZZ=ON ^
  -DZLIB_LIBRARY="%DEPS%/zlib/lib/zlibstatic.lib" ^
  -DZLIB_INCLUDE_DIR="%DEPS%/zlib/include" ^
  -DCMAKE_INSTALL_PREFIX="%DEPS%/freetype" || goto ERROR

"%CMAKE_BIN%" --build "%FT_BUILD%" --config Release || goto ERROR
"%CMAKE_BIN%" --install "%FT_BUILD%" --config Release || goto ERROR

REM === Normalize freetype lib names ===
if exist "%DEPS%\freetype\lib\freetype.lib" copy /Y "%DEPS%\freetype\lib\freetype.lib" "%DEPS%\freetype\lib\freetyped.lib" >nul
if not exist "%DEPS%\freetype\lib\freetype.lib" goto ERROR

del freetype-2.13.3.zip
rmdir /S /Q freetype-temp
exit /b 0

:InstallZlib
echo [INFO] Installing zlib...
rmdir /S /Q zlib-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o zlib.zip ^
  https://github.com/ShiftMediaProject/zlib/archive/refs/tags/v1.2.13.zip || goto ERROR

powershell -Command "Expand-Archive zlib.zip zlib-temp" || goto ERROR
mkdir "%DEPS%\zlib\lib" 2>nul
mkdir "%DEPS%\zlib\include" 2>nul
copy "zlib-temp\zlib-1.2.13\lib\zlibstatic.lib" "%DEPS%\zlib\lib\"
xcopy /E /I /Y /S "zlib-temp\zlib-1.2.13\*.h*" "%DEPS%\zlib\include"
if exist "zlib-temp\zlib-1.2.13\zconf.h.in" copy /Y "zlib-temp\zlib-1.2.13\zconf.h.in" "%DEPS%\zlib\include\zconf.h"

REM === Manual static zlib build with clang++ ===
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
echo [INFO] Installing libsndfile...
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
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
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

echo [INFO] Installing Assimp...
rmdir /S /Q assimp-temp 2>nul
"C:\Windows\System32\curl.exe" -L --fail -o assimp-5.3.0.zip ^
  https://github.com/assimp/assimp/archive/refs/tags/v5.3.0.zip || goto ERROR

powershell -Command "Expand-Archive assimp-5.3.0.zip assimp-temp" || goto ERROR
mkdir "%DEPS%\assimp\lib" 2>nul
mkdir "%DEPS%\assimp\include" 2>nul

xcopy /E /I /Y /S "assimp-temp\assimp-5.3.0\include" "%DEPS%\assimp\include" >nul
xcopy /E /I /Y /S "assimp-temp\assimp-5.3.0\code" "%DEPS%\assimp\include\code" >nul


REM === Generate config.h from config.h.in (remove #cmakedefine) ===
if exist "%DEPS%\assimp\include\assimp\config.h" del "%DEPS%\assimp\include\assimp\config.h"
findstr /V "#cmakedefine" "assimp-temp\assimp-5.3.0\include\assimp\config.h.in" > "%DEPS%\assimp\include\assimp\config.h"
echo #define ASSIMP_BUILD_NO_C4D_IMPORTER 1 >> "%DEPS%\assimp\include\assimp\config.h"
echo #define ASSIMP_BUILD_NO_IFC_IMPORTER 1 >> "%DEPS%\assimp\include\assimp\config.h"
echo #define ASSIMP_BUILD_NO_OPENGEX_IMPORTER 1 >> "%DEPS%\assimp\include\assimp\config.h"

REM === Auto-generate minimal revision.h if missing ===
if not exist assimp-temp\assimp-5.3.0\include\assimp\revision.h (
  echo // Auto-generated revision.h > assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define VER_MAJOR 5 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define VER_MINOR 3 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define VER_PATCH 0 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define GitVersion 0 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define GitBranch 0 >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
  echo #define ASSIMP_REVISION "v5.3.0" >> assimp-temp\assimp-5.3.0\include\assimp\revision.h
)

REM === Copy revision.h into include/assimp and code/ ===
if exist assimp-temp\assimp-5.3.0\include\assimp\revision.h copy /Y assimp-temp\assimp-5.3.0\include\assimp\revision.h assimp-temp\assimp-5.3.0\code\revision.h >nul
if exist assimp-temp\assimp-5.3.0\include\assimp\revision.h copy /Y assimp-temp\assimp-5.3.0\include\assimp\revision.h assimp-temp\assimp-5.3.0\code\Common\revision.h >nul

REM === Copy contrib folder for internal deps (utf8cpp, etc.) ===
xcopy /E /I /Y /S "assimp-temp\assimp-5.3.0\contrib" "%DEPS%\assimp\include\contrib" >nul

REM === Manual static Assimp build with clang++ ===

set CLANG_BIN="%DEPS%\Compiler\clang\bin"
set ASSIMP_SRC=assimp-temp\assimp-5.3.0\code
set OBJ_DIR=assimp-temp\assimp-5.3.0\obj
if exist %OBJ_DIR% rmdir /S /Q %OBJ_DIR%
mkdir %OBJ_DIR%
echo [INFO] Recursively compiling Assimp sources...
for /R %ASSIMP_SRC% %%f in (*.cpp) do (
  echo "%%f" | findstr /I /C:"AssetLib\\C4D" >nul
  if errorlevel 1 echo "%%f" | findstr /I /C:"AssetLib\\OpenGEX" >nul
  if errorlevel 1 echo "%%f" | findstr /I /C:"AssetLib\\IFC" >nul
  if errorlevel 1 findstr /C:"#include \"AssimpPCH.h\"" "%%f" >nul
  if errorlevel 1 %CLANG_BIN%\clang++.exe -c "%%f" -I"%DEPS%\assimp\include" -I"assimp-temp\assimp-5.3.0\include\assimp" -I"assimp-temp\assimp-5.3.0\code" -I"assimp-temp\assimp-5.3.0\code\Common" -I"assimp-temp\assimp-5.3.0\contrib\pugixml\src" -I"%DEPS%\zlib\include" -I"assimp-temp\assimp-5.3.0\contrib\rapidjson\include" -I"assimp-temp\assimp-5.3.0\contrib\unzip" -I"assimp-temp\assimp-5.3.0\contrib" -I"assimp-temp\assimp-5.3.0\contrib\openddlparser\include" -DASSIMP_BUILD_NO_EXPORT -DASSIMP_BUILD_NO_OWN_ZLIB -DASSIMP_BUILD_NO_C4D_IMPORTER=1 -DASSIMP_BUILD_NO_IFC_IMPORTER=1 -DASSIMP_BUILD_NO_OPENGEX_IMPORTER=1 -o "%OBJ_DIR%\%%~nf.obj" || goto ERROR
)

REM === Compile required unzip sources ===
set UNZIP_SRC=assimp-temp\assimp-5.3.0\contrib\unzip
for %%f in (%UNZIP_SRC%\*.c) do %CLANG_BIN%\clang.exe -c "%%f" -I"%DEPS%\zlib\include" -I"%UNZIP_SRC%" -o "%OBJ_DIR%\unzip_%%~nf.obj" || goto ERROR

%CLANG_BIN%\llvm-lib.exe /OUT:"%DEPS%\assimp\lib\assimp.lib" %OBJ_DIR%\*.obj || goto ERROR

del assimp-5.3.0.zip
rmdir /S /Q assimp-temp
echo [INFO] Assimp build done; static library generated.
exit /b 0

:InstallOpenAL
echo [INFO] Installing OpenAL...
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