@echo off

REM Pas de setlocal ici pour garder les variables dans le parent

REM Place ROOT sur la racine du projet (un niveau au-dessus de bootstrap)
pushd ..
set ROOT=%cd%
popd
set DEPS=%ROOT%\Dependencies
set BUILD=%ROOT%\bootstrap\build-temp

set CLANG=%DEPS%\Compiler\clang\bin\clang++.exe
set CLANG_C=%DEPS%\Compiler\clang\bin\clang.exe
set AR=%DEPS%\Compiler\clang\bin\llvm-ar.exe

echo [INFO] Chemin Clang : %CLANG%
echo [INFO] Chemin llvm-ar : %AR%

if not exist "%CLANG%" (
    echo [ERREUR] clang++ introuvable
    exit /b 1
)

if not exist "%BUILD%" mkdir "%BUILD%"
echo [INFO] Dossier build-temp déjà existant.
echo [INFO] Variables d'environnement configurées avec succès.
REM (Do not exit here, allow parent script to continue)
