@ECHO OFF
setlocal ENABLEEXTENSIONS
if "%REDECLIPSE_PATH%" == "" set REDECLIPSE_PATH=%~dp0\..
pushd %REDECLIPSE_PATH%

if "%REDECLIPSE_SOURCE%" == "" set REDECLIPSE_SOURCE=http://redeclipse.net/files
if "%REDECLIPSE_BRANCH%" == "" (
    set REDECLIPSE_BRANCH=stable
    if EXIST .git set REDECLIPSE_BRANCH=devel
)
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
if EXIST cache\current.txt set /p REDECLIPSE_VERSION=< cache\current.txt
if "%REDECLIPSE_VERSION%" == "" set REDECLIPSE_VERSION=0
set REDECLIPSE_VERSION=%REDECLIPSE_VERSION:~0,12%
echo.
echo Current version: %REDECLIPSE_BRANCH% %REDECLIPSE_VERSION%
echo Fetching version information from: %REDECLIPSE_SOURCE%/%REDECLIPSE_BRANCH%/version.txt
echo.
bin\tools\wget.exe --tries=3 --output-document=cache/version.txt "%REDECLIPSE_SOURCE%/%REDECLIPSE_BRANCH%/version.txt" > nul 2>&1
if NOT EXIST cache\version.txt (
    echo Failed to retrieve version update information. Trying to run anyway...
    echo.
    goto after
)
set /p REDECLIPSE_RETURN=< cache\version.txt
set REDECLIPSE_RETURN=%REDECLIPSE_RETURN:~0,12%
if "%REDECLIPSE_RETURN%" == "" (
    echo Failed to retrieve version update information. Trying to run anyway..
    echo.
    goto after
)
if "%REDECLIPSE_RETURN%" LEQ "%REDECLIPSE_VERSION%" goto good
echo Updated version: %REDECLIPSE_BRANCH% %REDECLIPSE_RETURN%
echo Downloading update from: %REDECLIPSE_SOURCE%/%REDECLIPSE_BRANCH%/windows.zip
echo.
bin\tools\wget.exe --tries=3 --output-document=cache/windows.zip "%REDECLIPSE_SOURCE%/%REDECLIPSE_BRANCH%/windows.zip"
if NOT EXIST cache\windows.zip (
    echo Failed to retrieve version update package. Trying to run anyway...
    echo.
    goto after
)
bin\tools\unzip.exe -o cache/windows.zip
echo.
echo %REDECLIPSE_RETURN% > cache\current.txt
:good
echo Everything is up to date!
:after
popd
endlocal
