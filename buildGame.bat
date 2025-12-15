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
 -I%DEP_PATH%\glm-master -I%DEP_PATH%\freetype\include ^
 -DFREEGLUT_STATIC -DGLEW_STATIC

set FILES=

for %%F in (Game\*.cpp Engine\Graphics\*.cpp Engine\Core\*.cpp Engine\Network\Client\*.cpp) do (
    set SRC=%%F
    rem
    call set "OBJNAME=%%F"
    call set "OBJNAME=%%OBJNAME:\=_%%"
    call set "OBJNAME=%%OBJNAME:.cpp=%%"
    call set "OBJ=%OBJ_DIR%\%%OBJNAME%%.o"

    if not exist "!OBJ!" (
        echo [NEW ] Compiling !SRC!
        g++ -c "!SRC!" -o "!OBJ!" %COMPILE_FLAGS%
    ) else (
        for %%S in ("!SRC!") do for %%O in ("!OBJ!") do (
            if %%~tS GTR %%~tO (
                echo [UPDATE] Compiling !SRC!
                g++ -c "!SRC!" -o "!OBJ!" %COMPILE_FLAGS%
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
 -L%DEP_PATH%\zlib\lib ^
 -L%DEP_PATH%\libsndfile\lib ^
 -L%DEP_PATH%\openal-soft-1.24.2-bin\libs ^
 -L%DEP_PATH%\freeglut\lib ^
 -L%DEP_PATH%\freetype\lib ^
 -static -static-libgcc -static-libstdc++ ^
 -lfreeglut_static -lglew32 -lassimp -lzlibstatic -lsndfile -lOpenAL32 -lfreetype -latomic ^
 -lopengl32 -lglu32 -lws2_32 -lwinmm -lgdi32 -lole32 -luuid -loleaut32 -limm32 -lversion ^
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
  
  :: Compresser les assets en un fichier PAK
  echo.
  echo Gestion des assets...
  
  :: Fonction pour récréer le PAK
  set ASSETS_PAK=%OUT_DIR%\game.pak
  set NEED_REBUILD=1
  
  :: Vérifier si le PAK existe et si les assets sont plus récents
  if exist "!ASSETS_PAK!" (
    set NEED_REBUILD=0
    for /R "%CD%\Assets" %%F in (*.*) do (
      for %%Z in ("!ASSETS_PAK!") do (
        if %%~tF GTR %%~tZ (
          echo [UPDATE] Assets plus recents que le PAK existant
          set NEED_REBUILD=1
        )
      )
    )
  )
  
  :: Recréer le PAK si nécessaire
  if !NEED_REBUILD! equ 1 (
    if exist "!ASSETS_PAK!" (
      echo Suppression du PAK existant...
      del "!ASSETS_PAK!"
    )
    echo Compression des assets dans game.pak...
    PowerShell -NoProfile -ExecutionPolicy Bypass -File "%CD%\createPak.ps1" -assetsDir "%CD%\Assets" -outputPak "!ASSETS_PAK!"
    
    if exist "!ASSETS_PAK!" (
      echo Assets comprimes avec succes dans game.pak
    ) else (
      echo Attention: impossible de creer le fichier game.pak
    )
  ) else (
    echo PAK a jour, pas de modification necessaire
  )
)
pause
