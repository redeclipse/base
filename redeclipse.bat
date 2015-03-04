@ECHO OFF
setlocal enableextensions enabledelayedexpansion
:path
    if DEFINED REDECLIPSE_PATH goto init
    pushd "%~dp0"
    set REDECLIPSE_PATH=%CD%
    popd
:init
    set REDECLIPSE_SCRIPT=%~dp0\%0
    for %%a in ("%REDECLIPSE_SCRIPT%") do set REDECLIPSE_SCRIPT_TIME=%%~ta
    if NOT DEFINED REDECLIPSE_BINARY set REDECLIPSE_BINARY=redeclipse
    set REDECLIPSE_SUFFIX=.exe
    set REDECLIPSE_MAKE=mingw32-make
:setup
    if DEFINED REDECLIPSE_ARCH goto branch
    set REDECLIPSE_ARCH=x86
    if DEFINED PROCESSOR_ARCHITEW6432 (
        set REDECLIPSE_MACHINE=%PROCESSOR_ARCHITEW6432%
    ) else (
        set REDECLIPSE_MACHINE=%PROCESSOR_ARCHITECTURE%
    )
    if /i "%REDECLIPSE_MACHINE%" == "amd64" set REDECLIPSE_ARCH=amd64
:branch
    if NOT DEFINED REDECLIPSE_BRANCH (
        set REDECLIPSE_BRANCH=stable
        if EXIST .git set REDECLIPSE_BRANCH=master
        if EXIST "%REDECLIPSE_PATH%\branch.txt" set /p REDECLIPSE_BRANCH=< "%REDECLIPSE_PATH%\branch.txt"
    )
    if "%REDECLIPSE_BRANCH%" == "devel" set REDECLIPSE_BRANCH=master
    if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "master" if NOT "%REDECLIPSE_BRANCH%" == "source" if NOT "%REDECLIPSE_BRANCH%" == "inplace" (
        set REDECLIPSE_BRANCH=inplace
    )
    if NOT DEFINED REDECLIPSE_HOME if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "inplace" set REDECLIPSE_HOME=home
    if DEFINED REDECLIPSE_HOME set REDECLIPSE_OPTIONS="-h%REDECLIPSE_HOME%" %REDECLIPSE_OPTIONS%
:check
    if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "master" goto runit
    echo.
    echo Checking for updates to "%REDECLIPSE_BRANCH%". To disable: set REDECLIPSE_BRANCH=inplace
    echo.
:begin
    set REDECLIPSE_RETRY=false
    goto update
:retry
    if "%REDECLIPSE_RETRY%" == "true" goto runit
    set REDECLIPSE_RETRY=true
    echo Retrying...
:update
    call "%REDECLIPSE_PATH%\bin\update.bat" && (
        for %%a in ("%REDECLIPSE_SCRIPT%") do set REDECLIPSE_SCRIPT_NOW=%%~ta
        if NOT "!REDECLIPSE_SCRIPT_NOW!" == "!REDECLIPSE_SCRIPT_TIME!" (
            call :runit "%REDECLIPSE_SCRIPT%"
            exit /b 0
        )
        goto runit
    ) || (
        for %%a in ("%REDECLIPSE_SCRIPT%") do set REDECLIPSE_SCRIPT_NOW=%%~ta
        if NOT "!REDECLIPSE_SCRIPT_NOW!" == "!REDECLIPSE_SCRIPT_TIME!" (
            call :retry "%REDECLIPSE_SCRIPT%"
            exit /b 0
        )
        goto retry
    )
:runit
    if EXIST "%REDECLIPSE_PATH%\bin\%REDECLIPSE_ARCH%\%REDECLIPSE_BINARY%%REDECLIPSE_SUFFIX%" (
        pushd "%REDECLIPSE_PATH%" || goto error
        start bin\%REDECLIPSE_ARCH%\%REDECLIPSE_BINARY%%REDECLIPSE_SUFFIX% %REDECLIPSE_OPTIONS% %* || (
            popd
            goto error
        )
        popd
        exit /b 0
    ) else (
        if "%REDECLIPSE_BRANCH%" == "source" (
            %REDECLIPSE_MAKE% -C src all install && goto runit
            set REDECLIPSE_BRANCH=master
        )
        if NOT "%REDECLIPSE_BRANCH%" == "inplace" if NOT "%REDECLIPSE_TRYUPDATE%" == "true" (
            set REDECLIPSE_TRYUPDATE=true
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
    exit /b 1
