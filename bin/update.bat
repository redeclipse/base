@ECHO OFF
setlocal ENABLEEXTENSIONS
if NOT DEFINED REDECLIPSE_PATH set REDECLIPSE_PATH=%~dp0\..
pushd %REDECLIPSE_PATH%
set REDECLIPSE_PATH=%CD%
:setup
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
if NOT DEFINED REDECLIPSE_GITHUB set REDECLIPSE_GITHUB=https://github.com/red-eclipse
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
echo Branch: %REDECLIPSE_BRANCH%
echo.
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
echo @ECHO OFF > "%REDECLIPSE_TMP%\install.bat"
echo setlocal ENABLEEXTENSIONS >> "%REDECLIPSE_TMP%\install.bat"
if NOT "%REDECLIPSE_BRANCH%" == "stable" goto binary
:base
echo Querying base version from: %REDECLIPSE_TMP%\base.ini
if EXIST "%REDECLIPSE_TMP%\base.ini" set /p REDECLIPSE_BASE=< "%REDECLIPSE_TMP%\base.ini"
if "%REDECLIPSE_BASE%" == "" set REDECLIPSE_BASE=0
set REDECLIPSE_BASE=%REDECLIPSE_BASE:~0,40%
echo.
echo Cached base: %REDECLIPSE_BASE%
echo Fetching base information from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/base.txt
echo.
if EXIST "%REDECLIPSE_TMP%\base.txt" del /f /q "%REDECLIPSE_TMP%\base.txt"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/base.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/base.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TMP%\base.txt" (
    echo Failed to retrieve base update information. Trying to run anyway...
    echo.
    goto data
)
set /p REDECLIPSE_RBASE=< "%REDECLIPSE_TMP%\base.txt"
if "%REDECLIPSE_RBASE%" == "" (
    echo Failed to retrieve base update information. Trying to run anyway..
    echo.
    goto data
)
set REDECLIPSE_RBASE=%REDECLIPSE_RBASE:~0,40%
echo Remote base: %REDECLIPSE_BRANCH% %REDECLIPSE_RBASE%
if "%REDECLIPSE_RBASE%" == "%REDECLIPSE_BASE%" goto data
echo Downloading base update from: %REDECLIPSE_GITHUB%/base/archive/%REDECLIPSE_RBASE%.zip
echo.
if EXIST "%REDECLIPSE_TMP%\base.zip" del /f /q "%REDECLIPSE_TMP%\base.zip"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/base.zip" "%REDECLIPSE_GITHUB%/base/archive/%REDECLIPSE_RBASE%.zip"
echo.
if NOT EXIST "%REDECLIPSE_TMP%\base.zip" (
    echo Failed to retrieve base update package. Trying to run anyway...
    echo.
    goto data
)
echo "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TMP%\base.zip" -d "%REDECLIPSE_PATH%" ^|^| exit /b 1 >> "%REDECLIPSE_TMP%\install.bat"
set REDECLIPSE_TRYUPDATE=1
:data
echo Querying data version from: %REDECLIPSE_TMP%\data.ini
if EXIST "%REDECLIPSE_TMP%\data.ini" set /p REDECLIPSE_DATA=< "%REDECLIPSE_TMP%\data.ini"
if "%REDECLIPSE_DATA%" == "" set REDECLIPSE_DATA=0
set REDECLIPSE_DATA=%REDECLIPSE_DATA:~0,40%
echo.
echo Cached data: %REDECLIPSE_DATA%
echo Fetching data information from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/data.txt
echo.
if EXIST "%REDECLIPSE_TMP%\data.txt" del /f /q "%REDECLIPSE_TMP%\data.txt"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/data.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/data.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TMP%\data.txt" (
    echo Failed to retrieve data update information. Trying to run anyway...
    echo.
    goto binary
)
set /p REDECLIPSE_RDATA=< "%REDECLIPSE_TMP%\data.txt"
if "%REDECLIPSE_RDATA%" == "" (
    echo Failed to retrieve data update information. Trying to run anyway..
    echo.
    goto binary
)
set REDECLIPSE_RDATA=%REDECLIPSE_RDATA:~0,40%
echo Remote data: %REDECLIPSE_BRANCH% %REDECLIPSE_RDATA%
if "%REDECLIPSE_RDATA%" == "%REDECLIPSE_DATA%" goto binary
echo Downloading data update from: %REDECLIPSE_GITHUB%/data/archive/%REDECLIPSE_RDATA%.zip
echo.
if EXIST "%REDECLIPSE_TMP%\data.zip" del /f /q "%REDECLIPSE_TMP%\data.zip"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/data.zip" "%REDECLIPSE_GITHUB%/data/archive/%REDECLIPSE_RDATA%.zip"
echo.
if NOT EXIST "%REDECLIPSE_TMP%\data.zip" (
    echo Failed to retrieve data update package. Trying to run anyway...
    echo.
    goto binary
)
echo "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TMP%\data.zip" -d "%REDECLIPSE_PATH%\data" ^|^| exit /b 1 >> "%REDECLIPSE_TMP%\install.bat"
set REDECLIPSE_TRYUPDATE=1
:binary
echo Querying binary version from: %REDECLIPSE_TMP%\version.ini
if EXIST "%REDECLIPSE_TMP%\version.ini" set /p REDECLIPSE_VERSION=< "%REDECLIPSE_TMP%\version.ini"
if "%REDECLIPSE_VERSION%" == "" set REDECLIPSE_VERSION=0
set REDECLIPSE_VERSION=%REDECLIPSE_VERSION:~0,14%
echo.
echo Cached version: %REDECLIPSE_VERSION%
echo Fetching version information from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/version.txt
echo.
if EXIST "%REDECLIPSE_TMP%\version.txt" del /f /q "%REDECLIPSE_TMP%\version.txt"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/version.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/version.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TMP%\version.txt" (
    echo Failed to retrieve version update information. Trying to run anyway...
    echo.
    goto deploy
)
set /p REDECLIPSE_RVERSION=< "%REDECLIPSE_TMP%\version.txt"
if "%REDECLIPSE_RVERSION%" == "" (
    echo Failed to retrieve version update information. Trying to run anyway..
    echo.
    goto deploy
)
set REDECLIPSE_RVERSION=%REDECLIPSE_RVERSION:~0,14%
echo Remote version: %REDECLIPSE_BRANCH% %REDECLIPSE_RVERSION%
if NOT "%REDECLIPSE_TRYUPDATE%" == "1" if "%REDECLIPSE_RVERSION%" == "%REDECLIPSE_VERSION%" goto good
echo Downloading binary update from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
echo.
if EXIST "%REDECLIPSE_TMP%\windows.zip" del /f /q "%REDECLIPSE_TMP%\windows.zip"
bin\tools\wget.exe --tries=3 --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TMP%/windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
echo.
if NOT EXIST "%REDECLIPSE_TMP%\windows.zip" (
    echo Failed to retrieve binary update package. Trying to run anyway...
    echo.
    goto deploy
)
echo "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TMP%\windows.zip" -d "%REDECLIPSE_PATH%" ^|^| exit /b 1 >> "%REDECLIPSE_TMP%\install.bat"
:deploy
echo endlocal >> "%REDECLIPSE_TMP%\install.bat"
echo Deploying: "%REDECLIPSE_TMP%\install.bat"
set REDECLIPSE_INSTALL=call
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
set REDECLIPSE_INSTALL=bin\tools\elevate.exe -wait
:unpack
%REDECLIPSE_INSTALL% "%REDECLIPSE_TMP%\install.bat" || (
    echo There was an error deploying the files. Trying to run anyway...
    echo.
    goto bail
)
echo.
echo %REDECLIPSE_RBASE% > "%REDECLIPSE_TMP%\base.ini"
echo %REDECLIPSE_RDATA% > "%REDECLIPSE_TMP%\data.ini"
echo %REDECLIPSE_RVERSION% > "%REDECLIPSE_TMP%\version.ini"
:good
if "%REDECLIPSE_BRANCH%" == "stable" (
    set /p REDECLIPSE_NEWSTABLE=< bin\version.txt
    if NOT "%REDECLIPSE_STABLE%" == "%REDECLIPSE_NEWSTABLE%" (
        call bin\update.bat
        goto bail
    )
)
echo Everything is up to date!
:bail
popd
endlocal
