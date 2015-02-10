@ECHO OFF
setlocal ENABLEEXTENSIONS
if "%APP_PATH%" == "" (set APP_PATH=%~dp0)
pushd %APP_PATH%
:start
if "%APP_OPTIONS%" == "" (set APP_OPTIONS= )
if "%APP_ARCH%" == "" (
    set APP_ARCH=x86
    if /i "%PROCESSOR_ARCHITECTURE%" == "amd64" (set APP_ARCH=amd64) else if /i "%PROCESSOR_ARCHITEW6432%" == "amd64" (set APP_ARCH=amd64)
)
if "%APP_SOURCE%" == "" (set APP_SOURCE=http://redeclipse.net/files)
if "%APP_BRANCH%" == "" (
    set APP_BRANCH=stable
    if EXIST .git set APP_BRANCH=devel
)
if NOT "%APP_BRANCH%" == "stable" (if "%APP_HOME%" == "" set APP_HOME=home)
if "%APP_BRANCH%" == "source" goto after
echo.
:update
if NOT EXIST bin\tools\wget.exe (
    echo Unable to find wget.exe, are you sure it is in bin/tools? Trying to run anyway...
    echo.
    goto after
)
if NOT EXIST bin\tools\unzip.exe (
    echo Unable to find unzip.exe, are you sure it is in bin/tools? Trying to run anyway...
    echo.
    goto after
)
if NOT EXIST cache mkdir cache
if EXIST cache\version.txt del /f /q cache\version.txt
if EXIST version.txt set /p APP_VERSION=<version.txt
if "%APP_VERSION%" == "" set APP_VERSION=0
set APP_VERSION=%APP_VERSION:~0,12%
echo Fetching version update information..
echo.
bin\tools\wget.exe --tries=3 --output-document=cache/version.txt "%APP_SOURCE%/%APP_BRANCH%/version.txt"
if NOT EXIST cache\version.txt (
    echo Failed to retrieve version update information. Trying to run anyway...
    echo.
    goto after
)
set /p APP_RETURN=<cache\version.txt
set APP_RETURN=%APP_RETURN:~0,12%
if "%APP_RETURN%" == "" (
    echo Failed to retrieve version update information. Trying to run anyway..
    echo.
    goto after
)
if "%APP_RETURN%" LEQ "%APP_VERSION%" goto good
echo New "%APP_BRANCH%" version detected [%APP_VERSION% to %APP_RETURN%], downloading...
echo.
bin\tools\wget.exe --tries=3 --output-document=cache/windows.zip "%APP_SOURCE%/%APP_BRANCH%/windows.zip"
if NOT EXIST cache\windows.zip (
    echo Failed to retrieve version update package. Trying to run anyway...
    echo.
    goto after
)
bin\tools\unzip.exe -o cache/windows.zip
echo.
echo %APP_RETURN% > version.txt
:good
echo Everything up to date!
:after
if NOT "%APP_HOME%" == "" (set APP_OPTIONS=-h%APP_HOME% %APP_OPTIONS%)
:retry
if EXIST bin\%APP_ARCH%\redeclipse.exe (
    start bin\%APP_ARCH%\redeclipse.exe %APP_OPTIONS% %*
    goto end
) else (
    if NOT "%APP_ARCH%" == "x86" (
        set APP_ARCH=x86
        goto retry
    )
    echo Unable to find the Red Eclipse client.
)
:error
echo There was an error running Red Eclipse.
pause
:end
popd
endlocal
