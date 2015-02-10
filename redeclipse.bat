@ECHO OFF
setlocal ENABLEEXTENSIONS
if "%REDECLIPSE_PATH%" == "" set REDECLIPSE_PATH=%~dp0
pushd %REDECLIPSE_PATH%

if "%REDECLIPSE_OPTIONS%" == "" set REDECLIPSE_OPTIONS=
if "%REDECLIPSE_ARCH%" == "" (
    set REDECLIPSE_ARCH=x86
    if /i "%PROCESSOR_ARCHITECTURE%" == "amd64" (set REDECLIPSE_ARCH=amd64) else if /i "%PROCESSOR_ARCHITEW6432%" == "amd64" (set REDECLIPSE_ARCH=amd64)
)
if "%REDECLIPSE_BRANCH%" == "" (
    set REDECLIPSE_BRANCH=stable
    if EXIST .git set REDECLIPSE_BRANCH=devel
)
if NOT "%REDECLIPSE_BRANCH%" == "stable" if "%REDECLIPSE_HOME%" == "" set REDECLIPSE_HOME=home
if NOT "%REDECLIPSE_BRANCH%" == "source" if NOT "%REDECLIPSE_NOUPDATE%" == "1" (
    echo.
    echo Checking for updates. To disable: set REDECLIPSE_NOUPDATE=1
    call bin\update.bat
)
if NOT "%REDECLIPSE_HOME%" == "" set REDECLIPSE_OPTIONS=-h%REDECLIPSE_HOME% %REDECLIPSE_OPTIONS%
:runit
if EXIST bin\%REDECLIPSE_ARCH%\redeclipse.exe (
    start bin\%REDECLIPSE_ARCH%\redeclipse.exe %REDECLIPSE_OPTIONS% %*
    goto end
) else (
    if NOT "%REDECLIPSE_ARCH%" == "x86" (
        set REDECLIPSE_ARCH=x86
        goto runit
    )
    echo Unable to find the Red Eclipse client.
)
:error
echo There was an error running Red Eclipse.
pause
:end
popd
endlocal
