@ECHO OFF
setlocal enableextensions enabledelayedexpansion
:path
    if DEFINED REDECLIPSE_PATH goto init
    pushd %~dp0
    set REDECLIPSE_PATH=%CD%
    popd
:init
    set REDECLIPSE_BINARY=redeclipse_server
:start
    call "%REDECLIPSE_PATH%\redeclipse.bat"
