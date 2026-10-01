@echo off
setlocal
if not defined ECLIPSE_RECOIL_HOME set "ECLIPSE_RECOIL_HOME=%APPDATA%\Eclipse Recoil"
if not exist "%ECLIPSE_RECOIL_HOME%" mkdir "%ECLIPSE_RECOIL_HOME%"
if not exist "%ECLIPSE_RECOIL_HOME%" exit /b 1
> "%ECLIPSE_RECOIL_HOME%\localinit.cfg" (
    echo exec "config/csgopen/tdm.cfg"
    echo sv_defaultmap "maps/echo"
    echo servermaster ""
    echo serverlanport 0
    echo httpserver 0
)
> "%ECLIPSE_RECOIL_HOME%\autoexec.cfg" echo exec "config/csgopen/client.cfg"
set "REDECLIPSE_HOME=%ECLIPSE_RECOIL_HOME%"
set "REDECLIPSE_DATADIR="
set "REDECLIPSE_EXTRADIRS="
set "REDECLIPSE_PATH="
cd /d "%~dp0" || exit /b 1
"bin\eclipse-recoil.exe" "-h%ECLIPSE_RECOIL_HOME%" "-g%ECLIPSE_RECOIL_HOME%\game.log" -bconfig/csgopen/branding.cfg -sm -ss0 "-xtdm echo" %*
exit /b %ERRORLEVEL%
