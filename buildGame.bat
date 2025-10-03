@echo off
setlocal enabledelayedexpansion
color 07
echo === Compilation du projet ===

set DEP_PATH=%CD%\Dependencies
set OUT_DIR=%CD%\Release\Game
set OBJ_DIR=%CD%\objGame

:: Créer les dossiers si nécessaires
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

:: Compiler chaque .cpp en .o s'il est modifié
echo Compilation des fichiers source...

set COMPILE_FLAGS=-std=c++17 -I%CD% ^
 -I%DEP_PATH%\glew-2.2.0\include ^
 -I%DEP_PATH%\assimp\include -I%DEP_PATH%\libsndfile\include ^
 -I%DEP_PATH%\openal-soft-1.24.2-bin\include -I%DEP_PATH%\freeglut\include ^
 -I%DEP_PATH%\glm-master -I%DEP_PATH%\freetype\include

set FILES=

for %%F in (Game\*.cpp Engine\Graphics\*.cpp Engine\Core\*.cpp Engine\Network\Client\*.cpp) do (
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

:: Lier les objets
echo.
echo Linkage final...

g++ !FILES! ^
 -L%DEP_PATH%\glew-2.2.0\lib ^
 -L%DEP_PATH%\assimp\lib ^
 -L%DEP_PATH%\libsndfile\lib ^
 -L%DEP_PATH%\openal-soft-1.24.2-bin\libs ^
 -L%DEP_PATH%\freeglut\lib ^
 -L%DEP_PATH%\freetype\lib ^
 -lglew32d -lassimp -lsndfile -lOpenAL32 -lfreeglut -lopengl32 -lglu32 -lfreetype -lws2_32^
 -o "%OUT_DIR%\game.exe"

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
