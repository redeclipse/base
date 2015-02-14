@ECHO OFF
setlocal ENABLEEXTENSIONS
if NOT DEFINED REDECLIPSE_PATH set REDECLIPSE_PATH=%~dp0\..
pushd %REDECLIPSE_PATH%
set REDECLIPSE_PATH=%CD%
:setup
if DEFINED REDECLIPSE_CACHE goto start
for /f "tokens=3* delims= " %%G in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" /v "Personal"') do (set USERMYDOCS=%%G)
if EXIST "%USERMYDOCS%" (
    set REDECLIPSE_CACHE=%USERMYDOCS%\My Games\Red Eclipse\cache

) else if EXIST "%REDECLIPSE_HOME%" (
    set REDECLIPSE_CACHE=%REDECLIPSE_HOME%\cache
) else (
    set REDECLIPSE_CACHE=cache
)
:start
echo Folder: %REDECLIPSE_PATH%
echo Cached: %REDECLIPSE_CACHE%
if NOT DEFINED REDECLIPSE_SOURCE set REDECLIPSE_SOURCE=http://redeclipse.net/files
if NOT DEFINED REDECLIPSE_GITHUB set REDECLIPSE_GITHUB=https://github.com/red-eclipse
if NOT DEFINED REDECLIPSE_BRANCH (
    set REDECLIPSE_BRANCH=stable
    if EXIST .git set REDECLIPSE_BRANCH=devel
)
if NOT "%REDECLIPSE_BRANCH%" == "stable" goto notstable
if NOT EXIST "%REDECLIPSE_PATH%\bin\version.txt" (
    echo Unable to find %REDECLIPSE_PATH%\bin\version.txt
    goto bail
)
set /p REDECLIPSE_BINVER=< "%REDECLIPSE_PATH%\bin\version.txt" 
if "%REDECLIPSE_BINVER%" == "" (
    echo Cannot determine current stable binary version, aborting...
    goto bail
)
set REDECLIPSE_BINVER=%REDECLIPSE_BINVER:~0,5%
set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%/%REDECLIPSE_BINVER%
set REDECLIPSE_TEMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%\%REDECLIPSE_BINVER%
goto branch
:notstable
set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%
set REDECLIPSE_TEMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%
:branch
echo Branch: %REDECLIPSE_BRANCH%
echo.
if NOT EXIST bin\tools\wget.exe (
    echo Unable to find wget.exe, are you sure it is in bin/tools? Trying to run anyway...
    goto bail
)
if NOT EXIST bin\tools\unzip.exe (
    echo Unable to find unzip.exe, are you sure it is in bin/tools? Trying to run anyway...
    goto bail
)
if NOT EXIST "%REDECLIPSE_TEMP%" mkdir "%REDECLIPSE_TEMP%"
echo @ECHO OFF > "%REDECLIPSE_TEMP%\install.bat"
echo setlocal ENABLEEXTENSIONS >> "%REDECLIPSE_TEMP%\install.bat"
echo set REDECLIPSE_ERROR=0 >> "%REDECLIPSE_TEMP%\install.bat"
if NOT "%REDECLIPSE_BRANCH%" == "stable" goto binary
:base
if EXIST "%REDECLIPSE_TEMP%\base.ini" set /p REDECLIPSE_BASE=< "%REDECLIPSE_TEMP%\base.ini"
if "%REDECLIPSE_BASE%" == "" set REDECLIPSE_BASE=none
set REDECLIPSE_BASE=%REDECLIPSE_BASE:~0,40%
echo Current base: %REDECLIPSE_BASE%
set REDECLIPSE_OBASE=none
if NOT EXIST "%REDECLIPSE_TEMP%\base.txt" goto baseget
set /p REDECLIPSE_OBASE=< "%REDECLIPSE_TEMP%\base.txt"
if "%REDECLIPSE_OBASE%" == "" set REDECLIPSE_OBASE=none
set REDECLIPSE_OBASE=%REDECLIPSE_OBASE:~0,40%
echo Cached base: %REDECLIPSE_OBASE%
del /f /q "%REDECLIPSE_TEMP%\base.txt"
:baseget
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/base.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/base.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TEMP%\base.txt" (
    echo Failed to retrieve base update information. Trying to run anyway...
    goto data
)
set /p REDECLIPSE_RBASE=< "%REDECLIPSE_TEMP%\base.txt"
if "%REDECLIPSE_RBASE%" == "" (
    echo Failed to retrieve base update information. Trying to run anyway..
    goto data
)
set REDECLIPSE_RBASE=%REDECLIPSE_RBASE:~0,40%
echo Remote base: %REDECLIPSE_RBASE%
if "%REDECLIPSE_RBASE%" == "%REDECLIPSE_BASE%" goto data
if "%REDECLIPSE_BASE%" == "none" goto baseblob
:basepatch
if NOT EXIST bin\tools\git-apply.exe (
    echo Unable to find git-apply.exe, are you sure it is in bin/tools? Downloading full zip instead...
    goto baseblob
)
if NOT "%REDECLIPSE_OBASE%" == "%REDECLIPSE_RBASE%" if EXIST "%REDECLIPSE_TEMP%\base.patch" del /f /q "%REDECLIPSE_TEMP%\base.patch"
echo Downloading base update from: %REDECLIPSE_GITHUB%/base/compare/%REDECLIPSE_BASE%...%REDECLIPSE_RBASE%.patch
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/base.patch" "%REDECLIPSE_GITHUB%/base/compare/%REDECLIPSE_BASE%...%REDECLIPSE_RBASE%.patch"
if NOT EXIST "%REDECLIPSE_TEMP%\base.patch" (
    echo Failed to retrieve base update package. Downloading full zip instead...
    goto baseblob
)
echo "%REDECLIPSE_PATH%\bin\tools\git-apply.exe" --stat --apply --verbose "%REDECLIPSE_TEMP%\base.patch" --directory="%REDECLIPSE_PATH%" ^&^& ^(echo %REDECLIPSE_RBASE% ^> "%REDECLIPSE_TEMP%\base.ini"^) ^|^| set REDECLIPSE_ERROR=1 >> "%REDECLIPSE_TEMP%\install.bat"
goto data
:baseblob
if NOT "%REDECLIPSE_OBASE%" == "%REDECLIPSE_RBASE%" if EXIST "%REDECLIPSE_TEMP%\base.zip" del /f /q "%REDECLIPSE_TEMP%\base.zip"
echo Downloading base update from: %REDECLIPSE_GITHUB%/base/archive/%REDECLIPSE_RBASE%.zip
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/base.zip" "%REDECLIPSE_GITHUB%/base/archive/%REDECLIPSE_RBASE%.zip"
if NOT EXIST "%REDECLIPSE_TEMP%\base.zip" (
    echo Failed to retrieve base update package. Trying to run anyway...
    goto data
)
echo "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TEMP%\base.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^( >> "%REDECLIPSE_TEMP%\install.bat"
echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\base-%REDECLIPSE_RBASE%\*" "%REDECLIPSE_PATH%" >> "%REDECLIPSE_TEMP%\install.bat"
echo    rmdir /s /q "%REDECLIPSE_TEMP%\base-%REDECLIPSE_RBASE%" >> "%REDECLIPSE_TEMP%\install.bat"
echo    echo %REDECLIPSE_RBASE% ^> "%REDECLIPSE_TEMP%\base.ini" >> "%REDECLIPSE_TEMP%\install.bat"
echo ^) ^|^| set REDECLIPSE_ERROR=1 >> "%REDECLIPSE_TEMP%\install.bat"
:data
echo.
if EXIST "%REDECLIPSE_PATH%\data\readme.txt" goto dataver
echo Unable to find "data\readme.txt". Will start from scratch.
set REDECLIPSE_DATA=none
echo mkdir "%REDECLIPSE_PATH%\data" >> "%REDECLIPSE_TEMP%\install.bat"
goto dataget
:dataver
if EXIST "%REDECLIPSE_TEMP%\data.ini" set /p REDECLIPSE_DATA=< "%REDECLIPSE_TEMP%\data.ini"
if "%REDECLIPSE_DATA%" == "" set REDECLIPSE_DATA=none
set REDECLIPSE_DATA=%REDECLIPSE_DATA:~0,40%
echo Current data: %REDECLIPSE_DATA%
set REDECLIPSE_ODATA=none
if NOT EXIST "%REDECLIPSE_TEMP%\data.txt" goto dataget
set /p REDECLIPSE_ODATA=< "%REDECLIPSE_TEMP%\data.txt"
if "%REDECLIPSE_ODATA%" == "" set REDECLIPSE_ODATA=none
set REDECLIPSE_ODATA=%REDECLIPSE_ODATA:~0,40%
echo Cached data: %REDECLIPSE_ODATA%
del /f /q "%REDECLIPSE_TEMP%\data.txt"
:dataget
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/data.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/data.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TEMP%\data.txt" (
    echo Failed to retrieve data update information. Trying to run anyway...
    goto binary
)
set /p REDECLIPSE_RDATA=< "%REDECLIPSE_TEMP%\data.txt"
if "%REDECLIPSE_RDATA%" == "" (
    echo Failed to retrieve data update information. Trying to run anyway..
    goto binary
)
set REDECLIPSE_RDATA=%REDECLIPSE_RDATA:~0,40%
echo Remote data: %REDECLIPSE_RDATA%
if "%REDECLIPSE_RDATA%" == "%REDECLIPSE_DATA%" goto binary
if "%REDECLIPSE_DATA%" == "none" goto datablob
:datapatch
if NOT EXIST bin\tools\git-apply.exe (
    echo Unable to find git-apply.exe, are you sure it is in bin/tools? Downloading full zip instead...
    goto datablob
)
if NOT "%REDECLIPSE_ODATA%" == "%REDECLIPSE_RDATA%" if EXIST "%REDECLIPSE_TEMP%\data.patch" del /f /q "%REDECLIPSE_TEMP%\data.patch"
echo Downloading data update from: %REDECLIPSE_GITHUB%/data/compare/%REDECLIPSE_DATA%...%REDECLIPSE_RDATA%.patch
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/data.patch" "%REDECLIPSE_GITHUB%/data/compare/%REDECLIPSE_DATA%...%REDECLIPSE_RDATA%.patch"
if NOT EXIST "%REDECLIPSE_TEMP%\data.patch" (
    echo Failed to retrieve data update package. Downloading full zip instead...
    goto datablob
)
echo "%REDECLIPSE_PATH%\bin\tools\git-apply.exe" --stat --apply --verbose "%REDECLIPSE_TEMP%\data.patch" --directory="%REDECLIPSE_PATH%\data" ^&^& ^(echo %REDECLIPSE_RDATA% ^> "%REDECLIPSE_TEMP%\data.ini"^) ^|^| set REDECLIPSE_ERROR=1 >> "%REDECLIPSE_TEMP%\install.bat"
goto binary
:datablob
echo Downloading data update from: %REDECLIPSE_GITHUB%/data/archive/%REDECLIPSE_RDATA%.zip
if NOT "%REDECLIPSE_ODATA%" == "%REDECLIPSE_RDATA%" if EXIST "%REDECLIPSE_TEMP%\data.zip" del /f /q "%REDECLIPSE_TEMP%\data.zip"
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/data.zip" "%REDECLIPSE_GITHUB%/data/archive/%REDECLIPSE_RDATA%.zip"
if NOT EXIST "%REDECLIPSE_TEMP%\data.zip" (
    echo Failed to retrieve data update package. Trying to run anyway...
    goto binary
)
echo "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TEMP%\data.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^( >> "%REDECLIPSE_TEMP%\install.bat"
echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\data-%REDECLIPSE_RDATA%\*" "%REDECLIPSE_PATH%\data" >> "%REDECLIPSE_TEMP%\install.bat"
echo    rmdir /s /q "%REDECLIPSE_TEMP%\data-%REDECLIPSE_RDATA%" >> "%REDECLIPSE_TEMP%\install.bat"
echo    echo %REDECLIPSE_RDATA% ^> "%REDECLIPSE_TEMP%\data.ini" >> "%REDECLIPSE_TEMP%\install.bat"
echo ^) ^|^| set REDECLIPSE_ERROR=1 >> "%REDECLIPSE_TEMP%\install.bat"
:binary
echo.
if EXIST "%REDECLIPSE_TEMP%\version.ini" set /p REDECLIPSE_VERSION=< "%REDECLIPSE_TEMP%\version.ini"
if "%REDECLIPSE_VERSION%" == "" set REDECLIPSE_VERSION=none
set REDECLIPSE_VERSION=%REDECLIPSE_VERSION:~0,14%
echo Current version: %REDECLIPSE_VERSION%
:binaryget
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/version.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/version.txt" > nul 2>&1
if NOT EXIST "%REDECLIPSE_TEMP%\version.txt" (
    echo Failed to retrieve version update information. Trying to run anyway...
    goto deploy
)
set /p REDECLIPSE_RVERSION=< "%REDECLIPSE_TEMP%\version.txt"
if "%REDECLIPSE_RVERSION%" == "" (
    echo Failed to retrieve version update information. Trying to run anyway..
    goto deploy
)
set REDECLIPSE_RVERSION=%REDECLIPSE_RVERSION:~0,14%
echo Remote version: %REDECLIPSE_RVERSION%
if NOT "%REDECLIPSE_TRYUPDATE%" == "1" if "%REDECLIPSE_RVERSION%" == "%REDECLIPSE_VERSION%" goto deploy
echo Downloading binary update from: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
if EXIST "%REDECLIPSE_TEMP%\windows.zip" del /f /q "%REDECLIPSE_TEMP%\windows.zip"
bin\tools\wget.exe --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%" --output-document="%REDECLIPSE_TEMP%/windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
if NOT EXIST "%REDECLIPSE_TEMP%\windows.zip" (
    echo Failed to retrieve binary update package. Trying to run anyway...
    goto deploy
)
echo "%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o "%REDECLIPSE_TEMP%\windows.zip" -d "%REDECLIPSE_PATH%" ^&^& ^(echo %REDECLIPSE_RVERSION% ^> "%REDECLIPSE_TEMP%\version.ini"^) ^|^| set REDECLIPSE_ERROR=1 >> "%REDECLIPSE_TEMP%\install.bat"
:deploy
echo endlocal >> "%REDECLIPSE_TEMP%\install.bat"
echo if "%%REDECLIPSE_ERROR%%" == "1" exit /b 1 >> "%REDECLIPSE_TEMP%\install.bat"
echo Deploying: "%REDECLIPSE_TEMP%\install.bat"
set REDECLIPSE_INSTALL=call
copy /y nul test.tmp > nul 2>&1 && (
    del /f /q test.tmp
    goto unpack
)
echo Administrator permissions are required to deploy the files.
if NOT EXIST bin\tools\elevate.exe (
    echo Unable to find elevate.exe, are you sure it is in bin/tools? Attempting deployment anyway...
    goto unpack
)
set REDECLIPSE_INSTALL=bin\tools\elevate.exe -wait
:unpack
%REDECLIPSE_INSTALL% "%REDECLIPSE_TEMP%\install.bat" || (
    echo There was an error deploying the files. Trying to run anyway...
    goto bail
)
:good
if NOT "%REDECLIPSE_BRANCH%" == "stable" goto yay
set /p REDECLIPSE_BINNEW=< bin\version.txt
if "%REDECLIPSE_BINVER%" == "%REDECLIPSE_BINNEW%" goto bail
call bin\update.bat
goto bail
:yay
echo.
echo Everything is up to date!
:bail
echo.
popd
endlocal
