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
cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_ecu_control.c" "%~dp0..\..\Core\Src\ecu_control.c" /Fe:test_ecu_control.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_ecu_control.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_ecu_app.c" "%~dp0..\..\Core\Src\ecu_app.c" "%~dp0..\..\Core\Src\ecu_control.c" /Fe:test_ecu_app.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_ecu_app.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_ecu_bridge.c" "%~dp0..\..\Core\Src\ecu_app.c" "%~dp0..\..\Core\Src\ecu_control.c" /Fe:test_ecu_bridge.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_ecu_bridge.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_rc522_soft_spi.c" "%~dp0..\..\Core\Src\rc522_soft_spi.c" /Fe:test_rc522_soft_spi.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_rc522_soft_spi.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_rc522.c" "%~dp0..\..\Core\Src\rc522.c" /Fe:test_rc522.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_rc522.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_soft_i2c.c" "%~dp0..\..\Core\Src\soft_i2c.c" /Fe:test_soft_i2c.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_soft_i2c.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_bmp280.c" "%~dp0..\..\Core\Src\bmp280.c" /Fe:test_bmp280.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_bmp280.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_height_estimator.c" "%~dp0..\..\Core\Src\height_estimator.c" /Fe:test_height_estimator.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_height_estimator.exe
if errorlevel 1 (
  popd
  exit /b 1
)

cl /nologo /std:c11 /W4 /WX /I"%~dp0..\..\Core\Inc" "%~dp0test_ecu_monitor.c" "%~dp0..\..\Core\Src\ecu_monitor.c" "%~dp0..\..\Core\Src\height_estimator.c" /Fe:test_ecu_monitor.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_ecu_monitor.exe
set "TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_RESULT%
