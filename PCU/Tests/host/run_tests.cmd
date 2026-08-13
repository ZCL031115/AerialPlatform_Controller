@echo off
setlocal

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
if exist "%VS_PATH%\Common7\Tools\VsDevCmd.bat" goto tools_found
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
if exist "%VS_PATH%\Common7\Tools\VsDevCmd.bat" goto tools_found
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional"
if exist "%VS_PATH%\Common7\Tools\VsDevCmd.bat" goto tools_found
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
if exist "%VS_PATH%\Common7\Tools\VsDevCmd.bat" goto tools_found
echo Visual C++ build tools are not installed.
exit /b 1

:tools_found
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1

set "OUT_DIR=%~dp0build"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

pushd "%OUT_DIR%"
cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_pcu_estop.c" "%~dp0..\..\Core\Src\pcu_estop.c" /Fe:test_pcu_estop.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_pcu_estop.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_pcu_app.c" "%~dp0..\..\Core\Src\pcu_app.c" "%~dp0..\..\Core\Src\pcu_estop.c" /Fe:test_pcu_app.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_pcu_app.exe
set "TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_RESULT%
