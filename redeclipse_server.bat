@ECHO OFF
setlocal enableextensions enabledelayedexpansion
if DEFINED REDECLIPSE_PATH goto init
pushd %~dp0
set REDECLIPSE_PATH=%CD%
popd
:init
set REDECLIPSE_BINARY=redeclipse_server
call "%REDECLIPSE_PATH%\redeclipse.bat"
