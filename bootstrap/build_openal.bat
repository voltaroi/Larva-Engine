
@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1


echo [DEBUG] ROOT vaut : %ROOT%
echo [DEBUG] Début création dossiers/copie openal...
echo [INFO] Build OpenAL Soft...

REM Chemins réels des headers et lib statique
set OPENAL_INCLUDE=%ROOT%\Dependencies\openal\include
set OPENAL_LIB=%ROOT%\Dependencies\openal\lib\OpenAL32.lib

REM Si la lib statique et les headers existent déjà, ne rien faire
if exist "%OPENAL_LIB%" if exist "%OPENAL_INCLUDE%\AL\al.h" (
    echo [INFO] OpenAL déjà prêt (lib statique et headers présents)
    exit /b 0
)

REM Sinon, télécharger et extraire OpenAL Soft (version binaire Windows)
set OPENAL_TEMP=%ROOT%\build-temp\openal-temp
if not exist "%OPENAL_TEMP%" mkdir "%OPENAL_TEMP%"
echo [INFO] Téléchargement OpenAL Soft...
curl -L -o "%OPENAL_TEMP%\openal.zip" https://openal-soft.org/openal-binaries/openal-soft-1.23.1-bin.zip || exit /b 1
powershell -Command "Expand-Archive -LiteralPath '%OPENAL_TEMP%\openal.zip' -DestinationPath '%OPENAL_TEMP%'"
REM Création explicite des dossiers openal, openal/lib et openal/include
mkdir "%ROOT%\Dependencies\openal" 2>nul
if not exist "%ROOT%\Dependencies\openal" (
    echo [ERREUR] Echec creation dossier Dependencies/openal
    exit /b 1
)
mkdir "%ROOT%\Dependencies\openal\lib" 2>nul
if not exist "%ROOT%\Dependencies\openal\lib" (
    echo [ERREUR] Echec creation dossier Dependencies/openal/lib
    exit /b 1
)
mkdir "%ROOT%\Dependencies\openal\include" 2>nul
if not exist "%ROOT%\Dependencies\openal\include" (
    echo [ERREUR] Echec creation dossier Dependencies/openal/include
    exit /b 1
)
xcopy /E /I /Y "%OPENAL_TEMP%\openal-soft-1.23.1-bin\libs\Win64\OpenAL32.lib" "%ROOT%\Dependencies\openal\lib\" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie de la lib OpenAL
    exit /b 1
)
xcopy /E /I /Y "%OPENAL_TEMP%\openal-soft-1.23.1-bin\include" "%ROOT%\Dependencies\openal\include\" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie des headers OpenAL
    exit /b 1
)
REM Nettoyage
del "%OPENAL_TEMP%\openal.zip"
rmdir /S /Q "%OPENAL_TEMP%\openal-soft-1.23.1-bin"
rmdir /S /Q "%OPENAL_TEMP%"

echo [INFO] OpenAL statique prêt.
exit /b 0
