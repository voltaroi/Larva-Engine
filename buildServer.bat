@echo off
setlocal enabledelayedexpansion
color 07
echo === Compilation du projet ===

set OUT_DIR=%CD%\Release\Server
set OBJ_DIR=%CD%\objServer

:: Créer les dossiers si nécessaires
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

:: Compiler chaque .cpp en .o s'il est modifié
echo Compilation des fichiers source...

set COMPILE_FLAGS=-std=c++17 -I%CD%

set FILES=

for %%F in (Server\*.cpp Engine\Core\*.cpp Engine\Network\*.cpp Engine\Network\Server\*.cpp) do (
    set SRC=%%F
    set OBJ=%OBJ_DIR%\%%~nF.o

    if not exist !OBJ! (
        echo [NEW ] Compiling !SRC!
        g++ -c !SRC! -o !OBJ! %COMPILE_FLAGS%
    ) else (
        for %%S in (!SRC!) do for %%O in (!OBJ!) do (
            if %%~tS GTR %%~tO (
                echo [UPDATE] Compiling !SRC!
                g++ -c !SRC! -o !OBJ! %COMPILE_FLAGS%
            )
        )
    )

    set FILES=!FILES! !OBJ!
)

:: Lier les objets (sans libmingw32, pour console)
echo.
echo Linkage final...

g++ !FILES! -o "%OUT_DIR%\server.exe" -lws2_32 -mconsole

if %errorlevel% neq 0 (
    echo.
    color 0C
    echo Erreur lors de la compilation.
    color 07
    pause
    exit /b %errorlevel%
) else (
    echo.
    color 02
    echo Compilation reussie dans le dossier Release.
    color 07
)
pause
