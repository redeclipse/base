@ECHO OFF
setlocal enableextensions enabledelayedexpansion
:redeclipse_path
    if DEFINED REDECLIPSE_PATH goto redeclipse_init
    pushd "%~dp0"
    set REDECLIPSE_PATH=%CD%
    popd
:redeclipse_init
    set REDECLIPSE_SCRIPT=%~dp0\%~0
    for %%a in ("%REDECLIPSE_SCRIPT%") do set REDECLIPSE_SCRIPT_TIME=%%~ta
    if NOT DEFINED REDECLIPSE_BINARY set REDECLIPSE_BINARY=redeclipse
    if NOT DEFINED REDECLIPSE_START set REDECLIPSE_START=start /wait
    set REDECLIPSE_SUFFIX=.exe
    set REDECLIPSE_MAKE=mingw32-make
:redeclipse_setup
    if DEFINED REDECLIPSE_ARCH goto redeclipse_branch
    set REDECLIPSE_ARCH=x86
    if DEFINED PROCESSOR_ARCHITEW6432 (
        set REDECLIPSE_MACHINE=%PROCESSOR_ARCHITEW6432%
    ) else (
        set REDECLIPSE_MACHINE=%PROCESSOR_ARCHITECTURE%
    )
    if /i "%REDECLIPSE_MACHINE%" == "amd64" set REDECLIPSE_ARCH=amd64
:redeclipse_branch
    if NOT DEFINED REDECLIPSE_BRANCH (
        if EXIST "%REDECLIPSE_PATH%\branch.txt" set /p REDECLIPSE_BRANCH_CURRENT=< "%REDECLIPSE_PATH%\branch.txt"
        if DEFINED REDECLIPSE_BRANCH_CURRENT (
            set REDECLIPSE_BRANCH=%REDECLIPSE_BRANCH_CURRENT%
        ) else if EXIST .git (
            set REDECLIPSE_BRANCH=devel
        ) else (
            set REDECLIPSE_BRANCH=master
        )
    )
    if NOT DEFINED REDECLIPSE_HOME if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "steam" set REDECLIPSE_HOME=home
    if DEFINED REDECLIPSE_HOME (
        if NOT EXIST "%REDECLIPSE_HOME%" mkdir "%REDECLIPSE_HOME%"> nul 2>&1
        copy /y nul "%REDECLIPSE_HOME%\test.tmp"> nul 2>&1 && (
            del /f /q "%REDECLIPSE_HOME%\test.tmp"
            set REDECLIPSE_OPTIONS="-h%REDECLIPSE_HOME%" %REDECLIPSE_OPTIONS%
            goto redeclipse_check
        )
        echo.
        echo Can not write to the requested home directory: %REDECLIPSE_HOME%
        echo Please ensure the directory exists and is writable by the current user.
        echo.
        goto redeclipse_error
    )
:redeclipse_check
    if "%REDECLIPSE_BRANCH%" == "inplace" goto redeclipse_runit
    if "%REDECLIPSE_BRANCH%" == "steam" goto redeclipse_runit
    echo.
    echo Checking for updates to "%REDECLIPSE_BRANCH%". To disable: set REDECLIPSE_BRANCH=inplace
    echo.
:redeclipse_begin
    set REDECLIPSE_RETRY=false
    goto redeclipse_update
:redeclipse_retry
    if "%REDECLIPSE_RETRY%" == "true" goto redeclipse_runit
    set REDECLIPSE_RETRY=true
    echo Retrying...
:redeclipse_update
    call "%REDECLIPSE_PATH%\bin\update.bat" && (
        for %%a in ("%REDECLIPSE_SCRIPT%") do set REDECLIPSE_SCRIPT_NOW=%%~ta
        if NOT "!REDECLIPSE_SCRIPT_NOW!" == "!REDECLIPSE_SCRIPT_TIME!" (
            call :redeclipse_runit "%REDECLIPSE_SCRIPT%" %*
            exit /b 0
        )
        goto redeclipse_runit
    ) || (
        for %%a in ("%REDECLIPSE_SCRIPT%") do set REDECLIPSE_SCRIPT_NOW=%%~ta
        if NOT "!REDECLIPSE_SCRIPT_NOW!" == "!REDECLIPSE_SCRIPT_TIME!" (
            call :redeclipse_retry "%REDECLIPSE_SCRIPT%" %*
            exit /b 0
        )
        goto redeclipse_retry
    )
:redeclipse_runit
    if EXIST "%REDECLIPSE_PATH%\bin\%REDECLIPSE_ARCH%\%REDECLIPSE_BINARY%%REDECLIPSE_SUFFIX%" (
        pushd "%REDECLIPSE_PATH%" || goto redeclipse_error
        %REDECLIPSE_START% bin\%REDECLIPSE_ARCH%\%REDECLIPSE_BINARY%%REDECLIPSE_SUFFIX% %REDECLIPSE_OPTIONS% %* || (
            popd
            goto redeclipse_error
        )
        popd
        exit /b 0
    ) else (
        if NOT "%REDECLIPSE_BRANCH%" == "inplace" NOT "%REDECLIPSE_BRANCH%" == "steam" if NOT "%REDECLIPSE_TRYUPDATE%" == "true" (
            set REDECLIPSE_TRYUPDATE=true
            goto redeclipse_begin
        )
        if NOT "%REDECLIPSE_ARCH%" == "x86" (
            set REDECLIPSE_ARCH=x86
            goto redeclipse_runit
        )
        echo Unable to find a working binary.
    )
:redeclipse_error
    echo There was an error running Red Eclipse.
    pause
    exit /b 1
