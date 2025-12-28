
@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1


echo [DEBUG] ROOT vaut : %ROOT%
echo [DEBUG] Début création dossiers/copie assimp...
echo [INFO] Build Assimp...

REM Chemins réels des headers et lib statique
set ASSIMP_INCLUDE=%ROOT%\Dependencies\assimp\include\include\assimp
set ASSIMP_LIB=%ROOT%\Dependencies\assimp\lib\assimp.lib

REM Si la lib statique et les headers existent déjà, ne rien faire
if exist "%ASSIMP_LIB%" if exist "%ASSIMP_INCLUDE%\Importer.hpp" (
    echo [INFO] Assimp déjà prêt (lib statique et headers présents)
    exit /b 0
)

echo [INFO] Assimp statique prêt.

REM Télécharger et extraire Assimp
set ASSIMP_TEMP=%ROOT%\build-temp\assimp-temp
if not exist "%ASSIMP_TEMP%" mkdir "%ASSIMP_TEMP%"
echo [INFO] Téléchargement Assimp...
curl -L -o "%ASSIMP_TEMP%\assimp.zip" https://github.com/assimp/assimp/archive/refs/tags/v5.2.5.zip || exit /b 1
powershell -Command "Expand-Archive -LiteralPath '%ASSIMP_TEMP%\assimp.zip' -DestinationPath '%ASSIMP_TEMP%'"

REM Création explicite des dossiers assimp, assimp/lib et assimp/include
mkdir "%ROOT%\Dependencies\assimp" 2>nul
mkdir "%ROOT%\Dependencies\assimp\lib" 2>nul
mkdir "%ROOT%\Dependencies\assimp\include" 2>nul

REM Compilation des sources .cpp d'assimp en .o
set ASSIMP_SRC=%ASSIMP_TEMP%\assimp-5.2.5\code
pushd "%ASSIMP_SRC%"
for %%f in (*.cpp) do (
    echo [INFO] Compilation de %%f
    "%CLANG_C%" -I"%ASSIMP_TEMP%\assimp-5.2.5\include" -I"%ASSIMP_TEMP%\assimp-5.2.5\code" -c "%%f" -O2
    if errorlevel 1 (
        echo [ERREUR] Compilation échouée pour %%f
        popd
        exit /b 1
    )
)

REM Archivage en assimp.lib
echo [INFO] Archivage de la librairie statique assimp.lib
"%AR%" rcs "%ROOT%\Dependencies\assimp\lib\assimp.lib" *.o
if errorlevel 1 (
    echo [ERREUR] Archivage échoué !
    popd
    exit /b 1
)
del /q *.o
popd


REM Copier tous les headers nécessaires (include + code)
xcopy /E /I /Y "%ASSIMP_TEMP%\assimp-5.2.5\include" "%ROOT%\Dependencies\assimp\include\include" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie des headers assimp/include
    exit /b 1
)
xcopy /E /I /Y "%ASSIMP_TEMP%\assimp-5.2.5\code" "%ROOT%\Dependencies\assimp\include\code" >nul
REM Copier config.h si présent (parfois généré, sinon fallback sur config.h.in)
if exist "%ASSIMP_TEMP%\assimp-5.2.5\include\assimp\config.h" (
    copy /Y "%ASSIMP_TEMP%\assimp-5.2.5\include\assimp\config.h" "%ROOT%\Dependencies\assimp\include\include\assimp\config.h" >nul
) else if exist "%ASSIMP_TEMP%\assimp-5.2.5\include\assimp\config.h.in" (
    copy /Y "%ASSIMP_TEMP%\assimp-5.2.5\include\assimp\config.h.in" "%ROOT%\Dependencies\assimp\include\include\assimp\config.h" >nul
)

REM Nettoyage
del "%ASSIMP_TEMP%\assimp.zip"
rmdir /S /Q "%ASSIMP_TEMP%\assimp-5.2.5"
rmdir /S /Q "%ASSIMP_TEMP%"

echo [INFO] Assimp statique prêt.
exit /b 0
