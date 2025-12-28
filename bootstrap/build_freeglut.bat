
@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1

REM Vérifier que %BUILD% est bien défini
if "%BUILD%"=="" (
    echo [ERREUR] La variable d'environnement BUILD n'est pas définie !
    exit /b 1
)

REM Créer le dossier build-temp si besoin
if not exist "%BUILD%" mkdir "%BUILD%"
if not exist "%BUILD%" (
    echo [ERREUR] Impossible de créer le dossier build-temp : %BUILD%
    exit /b 1
)

REM Définir les chemins
set FREEGLUT_ZIP=%BUILD%\freeglut.zip
set FREEGLUT_TEMP=%BUILD%\freeglut-temp
set FREEGLUT_SRC=%FREEGLUT_TEMP%\freeglut-3.4.0

echo [INFO] Build FreeGLUT...

REM Téléchargement si le zip n'existe pas
if not exist "%FREEGLUT_ZIP%" (
    echo [INFO] Téléchargement FreeGLUT...
    curl -L -o "%FREEGLUT_ZIP%" "https://github.com/freeglut/freeglut/archive/refs/tags/v3.4.0.zip"
    if errorlevel 1 (
        echo [ERREUR] Téléchargement échoué
        exit /b 1
    )
) else (
    echo [INFO] freeglut.zip déjà présent
)

REM Décompression si le dossier source n'existe pas
if not exist "%FREEGLUT_SRC%" (
    echo [INFO] Décompression FreeGLUT...
    powershell -Command "Expand-Archive -LiteralPath '%FREEGLUT_ZIP%' -DestinationPath '%FREEGLUT_TEMP%'"
    if errorlevel 1 (
        echo [ERREUR] Décompression échouée
        exit /b 1
    )
) else (
    echo [INFO] FreeGLUT déjà décompressé
)

REM Compilation avec Clang
if not exist "%DEPS%\freeglut\lib\freeglut_static.lib" (
    echo [INFO] Compilation FreeGLUT...
    mkdir "%DEPS%\freeglut\lib" 2>nul
    mkdir "%DEPS%\freeglut\include" 2>nul


    pushd "%FREEGLUT_SRC%\src"
    set CLANG_C=%DEPS%\Compiler\clang\bin\clang.exe
    if not exist "!CLANG_C!" (
        echo [ERREUR] clang.exe introuvable : !CLANG_C!
        popd
        exit /b 1
    )
    "%DEPS%\Compiler\clang\bin\clang.exe" -I"%FREEGLUT_SRC%/include" -c *.c -DFREEGLUT_STATIC -O2
    REM Vérifier qu'il y a bien des fichiers .o
    dir /b *.o >nul 2>&1
    if errorlevel 1 (
        echo [ERREUR] Aucun fichier objet .o généré, compilation FreeGLUT échouée !
        popd
        exit /b 1
    )
    "%AR%" rcs "%DEPS%\freeglut\lib\freeglut_static.lib" *.o
    popd

    REM Copier les headers
    xcopy /E /I /Y "%FREEGLUT_SRC%\include\*.h*" "%DEPS%\freeglut\include\" >nul
) else (
    echo [INFO] FreeGLUT déjà compilé
)

echo [INFO] FreeGLUT prêt
exit /b 0
