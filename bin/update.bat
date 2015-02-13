@ECHO OFF
setlocal ENABLEEXTENSIONS
if NOT DEFINED REDECLIPSE_PATH set REDECLIPSE_PATH=%~dp0\..
pushd %REDECLIPSE_PATH%
set REDECLIPSE_PATH=%CD%

if NOT DEFINED REDECLIPSE_CACHE (
    if NOT "%REDECLIPSE_HOME%" == "" (
        set REDECLIPSE_CACHE=%REDECLIPSE_HOME%\cache
    ) else if EXIST "%HOMEDRIVE%%HOMEPATH%\Documents\" (
        set REDECLIPSE_CACHE=%HOMEDRIVE%%HOMEPATH%\Documents\My Games\Red Eclipse\cache

    ) else if EXIST "%HOMEDRIVE%%HOMEPATH%\My Documents\" (
        set REDECLIPSE_CACHE=%HOMEDRIVE%%HOMEPATH%\My Documents\My Games\Red Eclipse\cache
    ) else (
        set REDECLIPSE_CACHE=cache
    )
)
echo Cache: %REDECLIPSE_CACHE%
if NOT DEFINED REDECLIPSE_SOURCE set REDECLIPSE_SOURCE=http://redeclipse.net/files
if NOT DEFINED REDECLIPSE_BRANCH (
    set REDECLIPSE_BRANCH=stable
    if EXIST .git set REDECLIPSE_BRANCH=devel
)
if "%REDECLIPSE_BRANCH%" == "stable" (
    set /p REDECLIPSE_STABLE=< bin\version.txt
    if "%REDECLIPSE_STABLE%" == "" (
        echo Cannot determine current stable binary version, aborting...
        goto bail
    )
    set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%/%REDECLIPSE_STABLE%
    set REDECLIPSE_TMP=%REDECLIPSE_CACHE%\%REDECLIPSE_STABLE%\%REDECLIPSE_UPDATE%
) else (
    set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%
    set REDECLIPSE_TMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%
)
if NOT EXIST bin\tools\wget.exe (
    echo Unable to find wget.exe, are you sure it is in bin/tools? Trying to run anyway...
    echo.
    goto bail
)
if NOT EXIST bin\tools\unzip.exe (
    echo Unable to find unzip.exe, are you sure it is in bin/tools? Trying to run anyway...
    echo.
    goto bail
)
if NOT EXIST "%REDECLIPSE_TMP%" mkdir "%REDECLIPSE_TMP%"
echo Querying version from: %REDECLIPSE_TMP%\branch.txt
if EXIST "%REDECLIPSE_TMP%\branch.txt" set /p REDECLIPSE_VERSION=< "%REDECLIPSE_TMP%\branch.txt"
if "%REDECLIPSE_VERSION%" == "" set REDECLIPSE_VERSION=0
set REDECLIPSE_VERSION=%REDECLIPSE_VERSION:~0,14%
echo.
echo Cached version: %REDECLIPSE_UPDATE% %REDECLIPSE_VERSION%
echo Fetching version information from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/version.txt
echo.
if EXIST "%REDECLIPSE_TMP%\version.txt" del /f /q "%REDECLIPSE_TMP%\version.txt"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/version.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/version.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TMP%\version.txt" (
    echo Failed to retrieve version update information. Trying to run anyway...
    echo.
    goto bail
)
set /p REDECLIPSE_REMOTE=< "%REDECLIPSE_TMP%\version.txt"
if "%REDECLIPSE_REMOTE%" == "" (
    echo Failed to retrieve version update information. Trying to run anyway..
    echo.
    goto bail
)
set REDECLIPSE_REMOTE=%REDECLIPSE_REMOTE:~0,14%
echo Remote version: %REDECLIPSE_BRANCH% %REDECLIPSE_REMOTE%
if NOT "%REDECLIPSE_TRYUPDATE%" == "1" if "%REDECLIPSE_REMOTE%" == "%REDECLIPSE_VERSION%" goto good
echo Downloading update from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
echo.
if EXIST "%REDECLIPSE_TMP%\windows.zip" del /f /q "%REDECLIPSE_TMP%\windows.zip"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
echo.
if NOT EXIST "%REDECLIPSE_TMP%\windows.zip" (
    echo Failed to retrieve version update package. Trying to run anyway...
    echo.
    goto bail
)
echo Deploying to: %REDECLIPSE_PATH%
echo "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TMP%\windows.zip" -d "%REDECLIPSE_PATH%" ^|^| exit /b 1 > "%REDECLIPSE_TMP%\install.bat"
copy /y nul test.tmp > nul 2>&1 && (
    del /f /q test.tmp
    goto unpack
)
echo Administrator permissions are required to deploy the files.
if NOT EXIST bin\tools\elevate.exe (
    echo Unable to find elevate.exe, are you sure it is in bin/tools? Attempting deployment anyway...
    echo.
    goto unpack
)
bin\tools\elevate.exe -wait "%REDECLIPSE_TMP%\install.bat" || (
    echo There was an error deploying the files. Trying to run anyway...
    echo.
    goto bail
)
goto branch
:unpack
call "%REDECLIPSE_TMP%\install.bat" || (
    echo There was an error deploying the files. Trying to run anyway...
    echo.
    goto bail
)
echo.
:branch
echo %REDECLIPSE_REMOTE% > "%REDECLIPSE_TMP%\branch.txt"
:good
echo Everything is up to date!
:bail
popd
endlocal
