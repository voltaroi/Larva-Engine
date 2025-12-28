@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1

REM Définir les chemins
set GLEW_ZIP=%BUILD%\glew.zip
set GLEW_TEMP=%BUILD%\glew-temp
set GLEW_SRC=%GLEW_TEMP%\glew-2.2.0

echo [INFO] Build GLEW...

REM Téléchargement si le zip n'existe pas
if not exist "%GLEW_ZIP%" (
    echo [INFO] Téléchargement GLEW...
    curl -L -o "%GLEW_ZIP%" "https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0-win32.zip"
    if errorlevel 1 (
        echo [ERREUR] Téléchargement échoué
        exit /b 1
    )
) else (
    echo [INFO] glew.zip déjà présent
)

REM Décompression si le dossier source n'existe pas
if not exist "%GLEW_SRC%" (
    echo [INFO] Décompression GLEW...
    powershell -Command "Expand-Archive -LiteralPath '%GLEW_ZIP%' -DestinationPath '%GLEW_TEMP%'"
    if errorlevel 1 (
        echo [ERREUR] Décompression échouée
        exit /b 1
    )
) else (
    echo [INFO] GLEW déjà décompressé
)

REM Copie des libs et headers
if not exist "%DEPS%\glew\lib" mkdir "%DEPS%\glew\lib"
if not exist "%DEPS%\glew\include" mkdir "%DEPS%\glew\include"
xcopy /E /I /Y "%GLEW_SRC%\include\*" "%DEPS%\glew\include\" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie des headers GLEW
    exit /b 1
)
xcopy /E /I /Y "%GLEW_SRC%\lib\*" "%DEPS%\glew\lib\" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie de la lib GLEW
    exit /b 1
)

REM Vérification
if exist "%DEPS%\glew\lib\glew32.lib" if exist "%DEPS%\glew\include\GL\glew.h" (
    echo [INFO] GLEW prêt
    exit /b 0
) else (
    echo [ERREUR] GLEW non installé correctement !
    exit /b 1
)

@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1


echo [DEBUG] ROOT vaut : %ROOT%
echo [DEBUG] Début création dossiers/copie GLEW...
echo [INFO] Build GLEW...

REM Chemins réels des headers et lib statique
set GLEW_INCLUDE=%ROOT%\Dependencies\glew\include\include\GL
set GLEW_LIB=%ROOT%\Dependencies\glew\lib\glew32.lib

REM Si la lib statique et les headers existent déjà, ne rien faire
if exist "%GLEW_LIB%" if exist "%GLEW_INCLUDE%\glew.h" (
    echo [INFO] GLEW déjà prêt (lib statique et headers présents)
    exit /b 0
)

REM Sinon, télécharger et extraire GLEW (version statique)
set GLEW_TEMP=%ROOT%\build-temp\glew-temp
if not exist "%GLEW_TEMP%" mkdir "%GLEW_TEMP%"
echo [INFO] Téléchargement GLEW...
curl -L -o "%GLEW_TEMP%\glew.zip" https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0-win32.zip || exit /b 1
powershell -Command "Expand-Archive -LiteralPath '%GLEW_TEMP%\glew.zip' -DestinationPath '%GLEW_TEMP%'"
REM Création explicite des dossiers glew, glew/lib et glew/include
mkdir "%ROOT%\Dependencies\glew" 2>nul
if not exist "%ROOT%\Dependencies\glew" (
    echo [ERREUR] Echec creation dossier Dependencies/glew
    exit /b 1
)
mkdir "%ROOT%\Dependencies\glew\lib" 2>nul
if not exist "%ROOT%\Dependencies\glew\lib" (
    echo [ERREUR] Echec creation dossier Dependencies/glew/lib
    exit /b 1
)
mkdir "%ROOT%\Dependencies\glew\include" 2>nul
if not exist "%ROOT%\Dependencies\glew\include" (
    echo [ERREUR] Echec creation dossier Dependencies/glew/include
    exit /b 1
)
xcopy /E /I /Y "%GLEW_TEMP%\glew-2.2.0\include" "%ROOT%\Dependencies\glew\include" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie des headers GLEW
    exit /b 1
)
xcopy /E /I /Y "%GLEW_TEMP%\glew-2.2.0\lib" "%ROOT%\Dependencies\glew\lib" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie de la lib GLEW
    exit /b 1
)
REM Nettoyage
del "%GLEW_TEMP%\glew.zip"
rmdir /S /Q "%GLEW_TEMP%\glew-2.2.0"
rmdir /S /Q "%GLEW_TEMP%"

echo [INFO] GLEW statique prêt.
exit /b 0
