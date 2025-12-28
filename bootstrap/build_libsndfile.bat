@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1

set ROOT=%~dp0..\
set SNDFILE_TEMP=%ROOT%build-temp\libsndfile-temp
set SNDFILE_VERSION=1.2.2
set SNDFILE_URL=https://github.com/libsndfile/libsndfile/archive/refs/tags/1.2.2.zip
set SNDFILE_ZIP=%SNDFILE_TEMP%\libsndfile.zip
set SNDFILE_SRC=%SNDFILE_TEMP%\libsndfile-1.2.2\
set SNDFILE_INCLUDE=%ROOT%Dependencies\libsndfile\include
set SNDFILE_LIB=%ROOT%Dependencies\libsndfile\lib

if not exist "%SNDFILE_TEMP%" mkdir "%SNDFILE_TEMP%"
if not exist "%SNDFILE_INCLUDE%" mkdir "%SNDFILE_INCLUDE%"
if not exist "%SNDFILE_LIB%" mkdir "%SNDFILE_LIB%"

REM Télécharger et extraire libsndfile
curl -L -o "%SNDFILE_ZIP%" %SNDFILE_URL% || exit /b 1
powershell -Command "Expand-Archive -LiteralPath '%SNDFILE_ZIP%' -DestinationPath '%SNDFILE_TEMP%'"

REM Copier les headers
xcopy /E /I /Y "%SNDFILE_SRC%include" "%SNDFILE_INCLUDE%" >nul

REM Compiler la lib statique (sources C)
pushd "%SNDFILE_SRC%src"
for %%f in (*.c) do (
    echo [INFO] Compilation de %%f
    "%CLANG_C%" -I"%SNDFILE_SRC%include" -c "%%f" -O2
    if errorlevel 1 (
        echo [ERREUR] Compilation échouée pour %%f
        popd
        exit /b 1
    )
)

REM Archivage en sndfile.lib
"%AR%" rcs "%SNDFILE_LIB%\sndfile.lib" *.o
if errorlevel 1 (
    echo [ERREUR] Archivage échoué !
    popd
    exit /b 1
)
del /q *.o
popd

REM Nettoyage
del "%SNDFILE_ZIP%"
rmdir /S /Q "%SNDFILE_SRC%"
rmdir /S /Q "%SNDFILE_TEMP%"

echo [INFO] libsndfile statique prêt.
exit /b 0
