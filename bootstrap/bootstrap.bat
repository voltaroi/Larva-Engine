@echo off
setlocal EnableDelayedExpansion

echo ========================================
echo  Bootstrap Builder - Larva Engine (CLANG)
echo ========================================

call env.bat || goto ERROR


call build_freeglut.bat || goto ERROR
call build_glew.bat || goto ERROR
call build_zlib.bat || goto ERROR
call build_assimp.bat || goto ERROR
call build_freetype.bat || goto ERROR
call build_libsndfile.bat || goto ERROR
call build_openal.bat || goto ERROR

echo.
echo [OK] Bootstrap terminé avec succès ✔
pause
exit /b 0

:ERROR
echo.
echo [ERREUR] Bootstrap échoué
echo Vérifie ta connexion, les permissions et que clang est installé.
pause
exit /b 1
