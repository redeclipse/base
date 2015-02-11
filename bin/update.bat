@ECHO OFF
setlocal ENABLEEXTENSIONS
if "%REDECLIPSE_PATH%" == "" set REDECLIPSE_PATH=%~dp0\..
pushd %REDECLIPSE_PATH%
set REDECLIPSE_PATH=%CD%

if "%REDECLIPSE_CACHE%" == "" set REDECLIPSE_CACHE=%LOCALAPPDATA%\Red Eclipse
if "%REDECLIPSE_SOURCE%" == "" set REDECLIPSE_SOURCE=http://redeclipse.net/files
if "%REDECLIPSE_BRANCH%" == "" (
    set REDECLIPSE_BRANCH=stable
    if EXIST .git set REDECLIPSE_BRANCH=devel
)
if "%REDECLIPSE_BRANCH%" == "stable" (
    set REDECLIPSE_STABLE=< bin\version.txt
    set REDECLIPSE_STABLE=%REDECLIPSE_STABLE:~0,5%
    set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%/%REDECLIPSE_STABLE%
) else (
    set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%
)
set REDECLIPSE_TMP=%REDECLIPSE_CACHE%\%REDECLIPSE_UPDATE%
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
if NOT EXIST "%REDECLIPSE_TMP%" mkdir "%REDECLIPSE_TMP%"
echo Querying version from: %REDECLIPSE_TMP%\version.txt
if EXIST "%REDECLIPSE_TMP%\version.txt" del /f /q "%REDECLIPSE_TMP%\version.txt"
if EXIST "%REDECLIPSE_TMP%\branch.txt" set /p REDECLIPSE_VERSION=< "%REDECLIPSE_TMP%\branch.txt"
if "%REDECLIPSE_VERSION%" == "" set REDECLIPSE_VERSION=0
set REDECLIPSE_VERSION=%REDECLIPSE_VERSION:~0,12%
echo.
echo Current version: %REDECLIPSE_BRANCH% %REDECLIPSE_VERSION%
echo Fetching version information from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/version.txt
echo.
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/version.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/version.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TMP%\version.txt" (
    echo Failed to retrieve version update information. Trying to run anyway...
    echo.
    goto after
)
set /p REDECLIPSE_RETURN=< "%REDECLIPSE_TMP%\version.txt"
set REDECLIPSE_RETURN=%REDECLIPSE_RETURN:~0,12%
if "%REDECLIPSE_RETURN%" == "" (
    echo Failed to retrieve version update information. Trying to run anyway..
    echo.
    goto after
)
if "%REDECLIPSE_RETURN%" LEQ "%REDECLIPSE_VERSION%" goto good
echo Updated version: %REDECLIPSE_BRANCH% %REDECLIPSE_RETURN%
echo Downloading update from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
echo.
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
if NOT EXIST "%REDECLIPSE_TMP%\windows.zip" (
    echo Failed to retrieve version update package. Trying to run anyway...
    echo.
    goto after
)
echo Deploying to: %REDECLIPSE_PATH%
for /f "USEBACKQ tokens=2 delims=:" %%F IN (`cacls "%REDECLIPSE_PATH%" ^| find "%USERNAME%"`) do (
    if "%%F" == "W" goto unpack
    if "%%F" == "F" goto unpack
    if "%%F" == "C" goto unpack
)
echo Administrator permissions are required to deploy the files.
echo.
if NOT EXIST bin\tools\elevate.exe (
    echo Unable to find elevate.exe, are you sure it is in bin/tools? Trying to run anyway...
    echo.
    goto after
)
bin\tools\elevate.exe -wait "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o """%REDECLIPSE_TMP%\windows.zip""" -d """%REDECLIPSE_PATH%"""
goto good
:unpack
echo.
"%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TMP%\windows.zip" -d "%REDECLIPSE_PATH%"
echo %REDECLIPSE_RETURN% > "%REDECLIPSE_TMP%\branch.txt"
:good
echo Everything is up to date!
:after
popd
endlocal
