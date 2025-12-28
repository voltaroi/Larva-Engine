


REM Copie automatique des sources .c de zlib si absents
set ZLIB_TEMP_SRC=%ROOT%\bootstrap\build-temp\zlib-temp\zlib-1.2.13
if exist "%ZLIB_TEMP_SRC%\*.c" (
    if not exist "%DEPS%\zlib" mkdir "%DEPS%\zlib"
    for %%f in ("%ZLIB_TEMP_SRC%\*.c") do copy /Y "%%f" "%DEPS%\zlib\" >nul
)

REM Génération automatique de zconf.h si absent
    if not exist "%DEPS%\zlib\include\zconf.h" (
        REM Priorité au vrai zconf.h du dossier temporaire zlib-1.2.13
        if exist "%ZLIB_TEMP_SRC%\zconf.h" copy /Y "%ZLIB_TEMP_SRC%\zconf.h" "%DEPS%\zlib\include\zconf.h" >nul
        if not exist "%DEPS%\zlib\include\zconf.h" if exist "%DEPS%\zlib\include\zconf.h.in" (
            echo [INFO] Génération d'un zconf.h minimal depuis zconf.h.in
            powershell -Command "Get-Content '%DEPS%\zlib\include\zconf.h.in' | Where-Object {($_ -notmatch '^#cmakedefine') -and ($_ -notmatch '^#undef') -and ($_ -notmatch '^#define Z_HAVE_UNISTD_H')} | Set-Content '%DEPS%\zlib\include\zconf.h'"
        )
        if not exist "%DEPS%\zlib\include\zconf.h" if exist "%DEPS%\zlib\include\zconf.h.cmakein" (
            echo [INFO] Génération d'un zconf.h minimal depuis zconf.h.cmakein
            powershell -Command "Get-Content '%DEPS%\zlib\include\zconf.h.cmakein' | Where-Object {($_ -notmatch '^#cmakedefine') -and ($_ -notmatch '^#undef') -and ($_ -notmatch '^#define Z_HAVE_UNISTD_H')} | Set-Content '%DEPS%\zlib\include\zconf.h'"
        )
    )

REM Utilisation directe des fichiers dans Dependencies/zlib
set ZLIB_INCLUDE=%DEPS%\zlib\include
set ZLIB_LIB=%DEPS%\zlib\lib

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

echo [INFO] Vérification de la présence de zlib dans Dependencies...
if exist "%ZLIB_LIB%\zlibstatic.lib" if exist "%ZLIB_INCLUDE%\zlib.h" (
    echo [INFO] zlib déjà présent dans Dependencies.
    exit /b 0
) else (
    echo [ERREUR] zlib non trouvé dans Dependencies !
    echo [ERREUR] Vérifie que %ZLIB_LIB%\zlibstatic.lib et %ZLIB_INCLUDE%\zlib.h existent.
    exit /b 1
)

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



REM Compilation automatique de zlib en statique
set ZLIB_LIB=%DEPS%\zlib\lib
set ZLIB_SRC1=%DEPS%\zlib
set ZLIB_SRC2=%DEPS%\zlib\src
set ZLIB_SRC3=%DEPS%\zlib\source

if not exist "%ZLIB_LIB%" mkdir "%ZLIB_LIB%"

REM Chercher le dossier contenant les .c
set ZLIB_SRC=
if exist "%ZLIB_SRC1%\*.c" set ZLIB_SRC=%ZLIB_SRC1%
if exist "%ZLIB_SRC2%\*.c" set ZLIB_SRC=%ZLIB_SRC2%
if exist "%ZLIB_SRC3%\*.c" set ZLIB_SRC=%ZLIB_SRC3%

if "%ZLIB_SRC%"=="" (
    echo [ERREUR] Aucun fichier source .c de zlib trouvé dans %ZLIB_SRC1%, %ZLIB_SRC2% ou %ZLIB_SRC3%.
    echo Place les sources .c de zlib dans l'un de ces dossiers.
    exit /b 1
)

echo [INFO] Compilation de zlib à partir de %ZLIB_SRC%
pushd "%ZLIB_SRC%"
echo [DEBUG] CLANG_C = %CLANG_C%
if not exist "%CLANG_C%" (
    echo [ERREUR] clang.exe introuvable : %CLANG_C%
    popd
    exit /b 1
)
echo [DEBUG] Compilation : "%CLANG_C%" -I"%ZLIB_INCLUDE%" -c *.c -O2
"%CLANG_C%" -I"%ZLIB_INCLUDE%" -c *.c -O2
echo [DEBUG] Résultat fichiers objets :
dir /b *.o
dir /b *.o >nul 2>&1
if errorlevel 1 (
    echo [ERREUR] Aucun fichier objet .o généré, compilation zlib échouée !
    popd
    exit /b 1
)
echo [DEBUG] Archivage : "%AR%" rcs "%ZLIB_LIB%\zlibstatic.lib" *.o
"%AR%" rcs "%ZLIB_LIB%\zlibstatic.lib" *.o
if errorlevel 1 (
    echo [ERREUR] Archivage échoué !
    popd
    exit /b 1
)
del /q *.o
popd
echo [INFO] zlib compilé et archivé dans %ZLIB_LIB%\zlibstatic.lib
exit /b 0
)

REM Vérification
if exist "%DEPS%\zlib\lib\zlibstatic.lib" if exist "%DEPS%\zlib\include\zlib.h" (
    echo [INFO] zlib prêt
    exit /b 0
) else (
    echo [ERREUR] zlib non installé correctement !
    exit /b 1
)

@echo off
setlocal EnableDelayedExpansion

REM Charger l'environnement
call env.bat || exit /b 1


echo [DEBUG] ROOT vaut : %ROOT%
echo [DEBUG] Début création dossiers/copie zlib...
echo [INFO] Build zlib...

REM Chemins réels des headers et lib statique
set ZLIB_INCLUDE=%ROOT%\Dependencies\zlib\include
set ZLIB_LIB=%ROOT%\Dependencies\zlib\lib\zlibstatic.lib

REM Si la lib statique et les headers existent déjà, ne rien faire
if exist "%ZLIB_LIB%" if exist "%ZLIB_INCLUDE%\zlib.h" (
    echo [INFO] zlib déjà prêt (lib statique et headers présents)
    exit /b 0
)

REM Sinon, télécharger et extraire zlib (version statique)
set ZLIB_TEMP=%ROOT%\build-temp\zlib-temp
if not exist "%ZLIB_TEMP%" mkdir "%ZLIB_TEMP%"
echo [INFO] Téléchargement zlib...
curl -L -o "%ZLIB_TEMP%\zlib.zip" https://github.com/ShiftMediaProject/zlib/archive/refs/tags/v1.2.13.zip || exit /b 1
powershell -Command "Expand-Archive -LiteralPath '%ZLIB_TEMP%\zlib.zip' -DestinationPath '%ZLIB_TEMP%'"
REM Création explicite des dossiers zlib, zlib/lib et zlib/include
mkdir "%ROOT%\Dependencies\zlib" 2>nul
if not exist "%ROOT%\Dependencies\zlib" (
    echo [ERREUR] Echec creation dossier Dependencies/zlib
    exit /b 1
)
mkdir "%ROOT%\Dependencies\zlib\lib" 2>nul
if not exist "%ROOT%\Dependencies\zlib\lib" (
    echo [ERREUR] Echec creation dossier Dependencies/zlib/lib
    exit /b 1
)
mkdir "%ROOT%\Dependencies\zlib\include" 2>nul
if not exist "%ROOT%\Dependencies\zlib\include" (
    echo [ERREUR] Echec creation dossier Dependencies/zlib/include
    exit /b 1
)
copy "%ZLIB_TEMP%\zlib-1.2.13\lib\zlibstatic.lib" "%ROOT%\Dependencies\zlib\lib\" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie de la lib zlib
    exit /b 1
)
xcopy /E /I /Y "%ZLIB_TEMP%\zlib-1.2.13\*.h*" "%ROOT%\Dependencies\zlib\include\" >nul
if errorlevel 1 (
    echo [ERREUR] Echec copie des headers zlib
    exit /b 1
)
REM Nettoyage
del "%ZLIB_TEMP%\zlib.zip"
rmdir /S /Q "%ZLIB_TEMP%\zlib-1.2.13"
rmdir /S /Q "%ZLIB_TEMP%"

echo [INFO] zlib statique prêt.
exit /b 0
