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
    if NOT DEFINED REDECLIPSE_START set REDECLIPSE_START=start
    set REDECLIPSE_SUFFIX=.exe
    set REDECLIPSE_MAKE=mingw32-make
:redeclipse_setup
    if DEFINED REDECLIPSE_ARCH goto redeclipse_home
    set REDECLIPSE_ARCH=x86
    if DEFINED PROCESSOR_ARCHITEW6432 (
        set REDECLIPSE_MACHINE=%PROCESSOR_ARCHITEW6432%
    ) else (
        set REDECLIPSE_MACHINE=%PROCESSOR_ARCHITECTURE%
    )
    if /i "%REDECLIPSE_MACHINE%" == "amd64" set REDECLIPSE_ARCH=amd64
:redeclipse_home
    if DEFINED REDECLIPSE_HOME (
        set REDECLIPSE_OPTIONS="-h%REDECLIPSE_HOME%" %REDECLIPSE_OPTIONS%
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
