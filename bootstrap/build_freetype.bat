@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1

set ROOT=%~dp0..\
set FT_TEMP=%ROOT%build-temp\freetype-temp
set FT_VERSION=2.13.2
set FT_URL=https://download.savannah.gnu.org/releases/freetype/freetype-%FT_VERSION%.zip
set FT_ZIP=%FT_TEMP%\freetype.zip
set FT_SRC=%FT_TEMP%\freetype-%FT_VERSION%
set FT_INCLUDE=%ROOT%Dependencies\freetype\include
set FT_LIB=%ROOT%Dependencies\freetype\lib

if not exist "%FT_TEMP%" mkdir "%FT_TEMP%"
if not exist "%FT_INCLUDE%" mkdir "%FT_INCLUDE%"
if not exist "%FT_LIB%" mkdir "%FT_LIB%"

REM Télécharger et extraire Freetype
curl -L -o "%FT_ZIP%" %FT_URL% || exit /b 1
powershell -Command "Expand-Archive -LiteralPath '%FT_ZIP%' -DestinationPath '%FT_TEMP%'"

REM Copier les headers
xcopy /E /I /Y "%FT_SRC%include" "%FT_INCLUDE%" >nul

REM Compiler la lib statique (sources C)
pushd "%FT_SRC%src"
for %%f in (*.c) do (
    echo [INFO] Compilation de %%f
    "%CLANG_C%" -I"%FT_SRC%include" -c "%%f" -O2
    if errorlevel 1 (
        echo [ERREUR] Compilation échouée pour %%f
        popd
        exit /b 1
    )
)

REM Archivage en freetype.lib
"%AR%" rcs "%FT_LIB%\freetype.lib" *.o
if errorlevel 1 (
    echo [ERREUR] Archivage échoué !
    popd
    exit /b 1
)
del /q *.o
popd

REM Nettoyage
del "%FT_ZIP%"
rmdir /S /Q "%FT_SRC%"
rmdir /S /Q "%FT_TEMP%"

echo [INFO] Freetype statique prêt.
exit /b 0
