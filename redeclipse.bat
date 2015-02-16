@ECHO OFF
setlocal enableextensions enabledelayedexpansion
if NOT DEFINED REDECLIPSE_PATH set REDECLIPSE_PATH=%~dp0
if NOT DEFINED REDECLIPSE_BINARY set REDECLIPSE_BINARY=redeclipse
set REDECLIPSE_BATCH=%REDECLIPSE_PATH%\%0
for %%a in ("%REDECLIPSE_BATCH%") do set REDECLIPSE_FILETIME=%%~ta
:setup
    if NOT DEFINED REDECLIPSE_OPTIONS set REDECLIPSE_OPTIONS=
    if NOT DEFINED REDECLIPSE_ARCH (
        set REDECLIPSE_ARCH=x86
        if /i "%PROCESSOR_ARCHITECTURE%" == "amd64" (set REDECLIPSE_ARCH=amd64) else if /i "%PROCESSOR_ARCHITEW6432%" == "amd64" (set REDECLIPSE_ARCH=amd64)
    )
    if NOT DEFINED REDECLIPSE_BRANCH (
        set REDECLIPSE_BRANCH=stable
        if EXIST .git set REDECLIPSE_BRANCH=devel
        if EXIST "%REDECLIPSE_PATH%\bin\branch.txt" set /p REDECLIPSE_BRANCH=< "%REDECLIPSE_PATH%\bin\branch.txt"
    )
    if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "devel" if NOT "%REDECLIPSE_BRANCH%" == "source" if NOT "%REDECLIPSE_BRANCH%" == "inplace" (
        set REDECLIPSE_BRANCH=inplace
    )
    if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT DEFINED REDECLIPSE_HOME set REDECLIPSE_HOME=home
    if DEFINED REDECLIPSE_HOME set REDECLIPSE_OPTIONS=-h"%REDECLIPSE_HOME%" %REDECLIPSE_OPTIONS%
:update
    if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "devel" goto runit
    echo.
    echo Checking for updates. To disable: set REDECLIPSE_BRANCH=inplace
    echo.
:begin
    set REDECLIPSE_RETRY=false
    goto update
:retry
    if "%REDECLIPSE_RETRY%" == "true" goto runit
    set REDECLIPSE_RETRY=true
:update
    set /p REDECLIPSE_BINVER=< "%REDECLIPSE_PATH%\bin\version.txt"
    call "%REDECLIPSE_PATH%\bin\update.bat" && (
        for %%a in ("%REDECLIPSE_BATCH%") do set REDECLIPSE_FILENOW=%%~ta
        if NOT "!REDECLIPSE_FILENOW!" == "!REDECLIPSE_FILETIME!" (
            echo The batch file has been modified, you should re-run it.
            pause
            exit /b 0
        )
    ) || (
        for %%a in ("%REDECLIPSE_BATCH%") do set REDECLIPSE_FILENOW=%%~ta
        if NOT "!REDECLIPSE_FILENOW!" == "!REDECLIPSE_FILETIME!" (
            echo The batch file has been modified, you should re-run it.
            pause
            exit /b 0
        )
        goto retry
    )
    if NOT "%REDECLIPSE_BRANCH%" == "stable" goto runit
    set /p REDECLIPSE_BINNEW=< "%REDECLIPSE_PATH%\bin\version.txt"
    if NOT "%REDECLIPSE_BINVER%" == "%REDECLIPSE_BINNEW%" goto update
:runit
    if EXIST "%REDECLIPSE_PATH%\bin\%REDECLIPSE_ARCH%\%REDECLIPSE_BINARY%.exe" (
        pushd "%REDECLIPSE_PATH%" || goto error
        start bin\%REDECLIPSE_ARCH%\%REDECLIPSE_BINARY%.exe %REDECLIPSE_OPTIONS% %* || goto error
        popd
        exit /b 1
    ) else (
        if "%REDECLIPSE_BRANCH%" == "source" (
            mingw32-make -C src all install && goto runit
            set REDECLIPSE_BRANCH=devel
        )
        if NOT "%REDECLIPSE_BRANCH%" == "inplace" if NOT "%REDECLIPSE_TRYUPDATE%" == "1" (
            set REDECLIPSE_TRYUPDATE=1
            goto begin
        )
        if NOT "%REDECLIPSE_ARCH%" == "x86" (
            set REDECLIPSE_ARCH=x86
            goto runit
        )
        echo Unable to find a working binary.
    )
:error
    echo There was an error running Red Eclipse.
    pause
