@echo off
rem Compila Windows-Manager y arma la carpeta lista para usar, junto con herramientas.
rem Uso: Compilar-en-Windows.bat CARPETA_QT CARPETA_MINGW CARPETA_CMAKE CARPETA_NINJA SALIDA
rem   CARPETA_QT      Qt para MinGW (la que tiene bin\windeployqt.exe)
rem   CARPETA_MINGW   MinGW (la que tiene bin\g++.exe)
rem   CARPETA_CMAKE   CMake (la que tiene bin\cmake.exe)
rem   CARPETA_NINJA   la que tiene ninja.exe
rem   SALIDA          donde se crea la carpeta Windows-Manager
setlocal
set "QT=%~1"
set "MINGW=%~2"
set "CM=%~3"
set "NINJA=%~4"
set "SALIDA=%~5\Windows-Manager"
set "FUENTES=%~dp0gui"
set "BUILD=%TEMP%\Windows-Manager-build"
set "PATH=%MINGW%\bin;%CM%\bin;%NINJA%;%QT%\bin;%PATH%"

cmake -B "%BUILD%" -S "%FUENTES%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT%" || exit /b 1
cmake --build "%BUILD%" || exit /b 1

if exist "%SALIDA%" rmdir /s /q "%SALIDA%"
mkdir "%SALIDA%\herramientas" || exit /b 1
copy /y "%BUILD%\Windows-Manager.exe" "%SALIDA%\" >nul || exit /b 1
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%SALIDA%\Windows-Manager.exe" || exit /b 1
robocopy "%~dp0herramientas" "%SALIDA%\herramientas" /E /NFL /NDL /NJH /NJS /NP >nul
copy /y "%BUILD%\askpass.exe" "%SALIDA%\herramientas\" >nul || exit /b 1
echo.
echo LISTO: %SALIDA%
