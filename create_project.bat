@echo off
chcp 65001 >nul
setlocal

set PROJ=%1
if "%PROJ%"=="" (
    echo Usage: create_project.bat ^<project_name^>
    exit /b 1
)

set ROOT=%~dp0
set PROJ_DIR=%ROOT%projects\%PROJ%

if exist "%PROJ_DIR%" (
    echo The project "%PROJ%" already exists.
    exit /b 1
)

rem Create folder layout
mkdir "%PROJ_DIR%\client\src" >nul
mkdir "%PROJ_DIR%\server\src" >nul
mkdir "%PROJ_DIR%\configs" >nul

rem Copy Assets from Template
if exist "%ROOT%Template\Assets" (
    xcopy /E /I /Y "%ROOT%Template\Assets\" "%PROJ_DIR%\Assets\" >nul
) else (
    mkdir "%PROJ_DIR%\Assets" >nul
    echo (place project assets here) >"%PROJ_DIR%\Assets\README.txt"
)

set TEMPLATE_CLIENT=%ROOT%Template\client\src
if exist "%TEMPLATE_CLIENT%\main.cpp" (
    xcopy /E /I /Y "%TEMPLATE_CLIENT%\" "%PROJ_DIR%\client\src\" >nul
)

rem Client sources come exclusively from Template; no fallback stubs

rem Sample server main (kept minimal; extend template if needed)
set TEMPLATE_SERVER=%ROOT%Template\server\src
if exist "%TEMPLATE_SERVER%\main.cpp" (
    xcopy /E /I /Y "%TEMPLATE_SERVER%\" "%PROJ_DIR%\server\src\" >nul
) else (
    rem Sample server main (fallback only if no template)
    if not exist "%PROJ_DIR%\server\src\main.cpp" (
        >"%PROJ_DIR%\server\src\main.cpp" echo #include ^<iostream^>
        >>"%PROJ_DIR%\server\src\main.cpp" echo int main()
        >>"%PROJ_DIR%\server\src\main.cpp" echo {
        >>"%PROJ_DIR%\server\src\main.cpp" echo ^    std::cout ^<^< "Hello from %PROJ% server" ^<^< std::endl;
        >>"%PROJ_DIR%\server\src\main.cpp" echo ^    return 0;
        >>"%PROJ_DIR%\server\src\main.cpp" echo }
    )
)

rem Client config JSON
>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo {
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "name": "%PROJ% - Client",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "outputDir": "projects/%PROJ%/Release/Client",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "objectDir": "projects/%PROJ%/obj/Client",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "outputName": "client",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "cppStandard": "c++17",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "buildType": "release",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "isConsoleApp": true,
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "staticLink": true,
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "sourceFiles": [
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "projects/%PROJ%/client/src/*",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Engine/Graphics/*",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Engine/Network/Client/Client.cpp"
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     ],
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "includeDirs": [
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         ".",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/glew-2.2.0/include",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/assimp/include",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/libsndfile/include",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/openal-soft-1.24.2-bin/include",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/freeglut/include",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/glm-master",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/freetype/include"
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     ],
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "libraryDirs": [
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/glew-2.2.0/lib",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/assimp/lib",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/libsndfile/lib",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/openal-soft-1.24.2-bin/libs/Win64",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/freeglut/lib",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/freetype/lib",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "Dependencies/zlib/lib"
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     ],
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "libraries": [
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "mingw32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "freeglut_static",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "glew32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "opengl32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "glu32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "gdi32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "user32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "assimp",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "sndfile",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "OpenAL32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "freetype",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "ws2_32",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "winmm",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "zlibstatic"
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     ],
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     "defines": [
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "FREEGLUT_STATIC",
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo         "GLEW_STATIC"
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo     ]
>>"%PROJ_DIR%\configs\%PROJ%_client_config.json" echo }

rem Server config JSON
>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo {
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "name": "%PROJ% - Server",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "outputDir": "projects/%PROJ%/Release/Server",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "objectDir": "projects/%PROJ%/obj/Server",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "outputName": "server",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "cppStandard": "c++17",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "buildType": "release",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "isConsoleApp": true,
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "staticLink": false,
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "sourceFiles": [
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo         "projects/%PROJ%/server/src/*",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo         "Engine/Network/ServerChat.cpp",
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo         "Engine/Network/Server/Server.cpp"
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     ],
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "includeDirs": [
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo         "."
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     ],
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "libraryDirs": [],
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "libraries": [
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo         "ws2_32"
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     ],
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo(
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo     "defines": []
>>"%PROJ_DIR%\configs\%PROJ%_server_config.json" echo }

rem Build scripts
>"%PROJ_DIR%\build_client.bat" echo @echo off
>>"%PROJ_DIR%\build_client.bat" echo chcp 65001 ^>nul
>>"%PROJ_DIR%\build_client.bat" echo setlocal
>>"%PROJ_DIR%\build_client.bat" echo set ROOT=%%~dp0\..\..
>>"%PROJ_DIR%\build_client.bat" echo pushd "%%ROOT%%"
>>"%PROJ_DIR%\build_client.bat" echo title Build %PROJ% Client
>>"%PROJ_DIR%\build_client.bat" echo echo === Client build %PROJ% ===
>>"%PROJ_DIR%\build_client.bat" echo echo.
>>"%PROJ_DIR%\build_client.bat" echo if not exist "Release\Builder\larva-builder.exe" (
>>"%PROJ_DIR%\build_client.bat" echo ^    echo The builder is not compiling. Launching bootstrap...
>>"%PROJ_DIR%\build_client.bat" echo ^    call bootstrap_builder.bat
>>"%PROJ_DIR%\build_client.bat" echo ^    if %%errorlevel%% neq 0 (
>>"%PROJ_DIR%\build_client.bat" echo ^        popd
>>"%PROJ_DIR%\build_client.bat" echo ^        exit /b 1
>>"%PROJ_DIR%\build_client.bat" echo ^    )
>>"%PROJ_DIR%\build_client.bat" echo ^    echo.
>>"%PROJ_DIR%\build_client.bat" echo )
>>"%PROJ_DIR%\build_client.bat" echo Release\Builder\larva-builder.exe projects\%PROJ%\configs\%PROJ%_client_config.json
>>"%PROJ_DIR%\build_client.bat" echo if %%errorlevel%% neq 0 (
>>"%PROJ_DIR%\build_client.bat" echo ^    color 0C
>>"%PROJ_DIR%\build_client.bat" echo ^    echo.
>>"%PROJ_DIR%\build_client.bat" echo ^    echo Client build failed %PROJ%
>>"%PROJ_DIR%\build_client.bat" echo ^    color 07
>>"%PROJ_DIR%\build_client.bat" echo ^    pause
>>"%PROJ_DIR%\build_client.bat" echo ^    popd
>>"%PROJ_DIR%\build_client.bat" echo ^    exit /b 1
>>"%PROJ_DIR%\build_client.bat" echo )


>>"%PROJ_DIR%\build_client.bat" echo set COMPILER=Dependencies\Compiler\clang\bin\clang++.exe
>>"%PROJ_DIR%\build_client.bat" echo set COMPILER_FLAGS=-std=c++17 -O2
>>"%PROJ_DIR%\build_client.bat" echo rem Client compilation
>>"%PROJ_DIR%\build_client.bat" echo %%COMPILER%% %%COMPILER_FLAGS%% projects\%PROJ%\client\src\main.cpp -o projects\%PROJ%\Release\Client\client.exe
>>"%PROJ_DIR%\build_client.bat" echo set OUT=projects\%PROJ%\Release\Client
>>"%PROJ_DIR%\build_client.bat" echo set ASSETS_TEMP=projects\%PROJ%\Assets
>>"%PROJ_DIR%\build_client.bat" echo if not exist "%%OUT%%" mkdir "%%OUT%%"
>>"%PROJ_DIR%\build_client.bat" echo if not exist "%%ASSETS_TEMP%%\Shaders" mkdir "%%ASSETS_TEMP%%\Shaders"
>>"%PROJ_DIR%\build_client.bat" echo copy /Y "Engine\Graphics\Shaders\*" "%%ASSETS_TEMP%%\Shaders\" ^>nul
>>"%PROJ_DIR%\build_client.bat" echo powershell -ExecutionPolicy Bypass -Command "& { $assets = (Resolve-Path 'projects/%PROJ%/Assets').Path; .\createPak.ps1 $assets 'projects/%PROJ%/Release/Client/game.pak' }"
>>"%PROJ_DIR%\build_client.bat" echo copy /Y "Dependencies\openal-soft-1.24.2-bin\bin\Win64\soft_oal.dll" "%%OUT%%" ^>nul

>>"%PROJ_DIR%\build_client.bat" echo echo.
>>"%PROJ_DIR%\build_client.bat" echo color 0A
>>"%PROJ_DIR%\build_client.bat" echo echo === Client build %PROJ% complete ! ===
>>"%PROJ_DIR%\build_client.bat" echo color 07
>>"%PROJ_DIR%\build_client.bat" echo.
>>"%PROJ_DIR%\build_client.bat" echo echo Copying runtime DLLs...
>>"%PROJ_DIR%\build_client.bat" echo copy /Y "%%ROOT%%\Dependencies\OpenAL\bin\soft_oal.dll" "projects\%PROJ%\Release\Client\OpenAL32.dll" ^>nul
>>"%PROJ_DIR%\build_client.bat" echo echo DLLs copied successfully
>>"%PROJ_DIR%\build_client.bat" echo echo.
>>"%PROJ_DIR%\build_client.bat" echo pause
>>"%PROJ_DIR%\build_client.bat" echo popd

>"%PROJ_DIR%\build_server.bat" echo @echo off
>>"%PROJ_DIR%\build_server.bat" echo chcp 65001 ^>nul
>>"%PROJ_DIR%\build_server.bat" echo setlocal
>>"%PROJ_DIR%\build_server.bat" echo set ROOT=%%~dp0\..\..
>>"%PROJ_DIR%\build_server.bat" echo pushd "%%ROOT%%"
>>"%PROJ_DIR%\build_server.bat" echo title Build %PROJ% Server
>>"%PROJ_DIR%\build_server.bat" echo echo === Server build %PROJ% ===
>>"%PROJ_DIR%\build_server.bat" echo echo.
>>"%PROJ_DIR%\build_server.bat" echo if not exist "Release\Builder\larva-builder.exe" (
>>"%PROJ_DIR%\build_server.bat" echo ^    echo The builder is not compiling. Launching bootstrap...
>>"%PROJ_DIR%\build_server.bat" echo ^    call bootstrap_builder.bat
>>"%PROJ_DIR%\build_server.bat" echo ^    if %%errorlevel%% neq 0 (
>>"%PROJ_DIR%\build_server.bat" echo ^        popd
>>"%PROJ_DIR%\build_server.bat" echo ^        exit /b 1
>>"%PROJ_DIR%\build_server.bat" echo ^    )
>>"%PROJ_DIR%\build_server.bat" echo ^    echo.
>>"%PROJ_DIR%\build_server.bat" echo )
>>"%PROJ_DIR%\build_server.bat" echo Release\Builder\larva-builder.exe projects\%PROJ%\configs\%PROJ%_server_config.json
>>"%PROJ_DIR%\build_server.bat" echo if %%errorlevel%% neq 0 (
>>"%PROJ_DIR%\build_server.bat" echo ^    color 0C
>>"%PROJ_DIR%\build_server.bat" echo ^    echo.
>>"%PROJ_DIR%\build_server.bat" echo ^    echo Server build failed %PROJ%
>>"%PROJ_DIR%\build_server.bat" echo ^    color 07
>>"%PROJ_DIR%\build_server.bat" echo ^    pause
>>"%PROJ_DIR%\build_server.bat" echo ^    popd
>>"%PROJ_DIR%\build_server.bat" echo ^    exit /b 1
>>"%PROJ_DIR%\build_server.bat" echo )

>>"%PROJ_DIR%\build_server.bat" echo set COMPILER=Dependencies\Compiler\clang\bin\clang++.exe
>>"%PROJ_DIR%\build_server.bat" echo set COMPILER_FLAGS=-std=c++17 -O2
>>"%PROJ_DIR%\build_server.bat" echo rem Server compilation
>>"%PROJ_DIR%\build_server.bat" echo %%COMPILER%% %%COMPILER_FLAGS%% projects\%PROJ%\server\src\main.cpp -o projects\%PROJ%\Release\Server\server.exe
>>"%PROJ_DIR%\build_server.bat" echo echo.
>>"%PROJ_DIR%\build_server.bat" echo color 0A
>>"%PROJ_DIR%\build_server.bat" echo echo === Build server %PROJ% complete ! ===
>>"%PROJ_DIR%\build_server.bat" echo color 07
>>"%PROJ_DIR%\build_server.bat" echo echo.
>>"%PROJ_DIR%\build_server.bat" echo pause
>>"%PROJ_DIR%\build_server.bat" echo popd


rem Les scripts de build client/server utilisent le compilateur intégré Clang/LLVM
rem Chemin attendu : Dependencies\Compiler\clang\bin\clang++.exe
color 0A
echo Project "%PROJ%" created in projects\%PROJ%.
color 07
endlocal
