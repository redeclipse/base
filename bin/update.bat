@ECHO OFF
setlocal enableextensions enabledelayedexpansion
if NOT DEFINED REDECLIPSE_PATH set REDECLIPSE_PATH=%~dp0\..
pushd %REDECLIPSE_PATH%
set REDECLIPSE_PATH=%CD%
popd
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
if NOT DEFINED REDECLIPSE_SOURCE set REDECLIPSE_SOURCE=http://redeclipse.net/files
if NOT DEFINED REDECLIPSE_GITHUB set REDECLIPSE_GITHUB=https://github.com/red-eclipse
if NOT DEFINED REDECLIPSE_BRANCH (
    set REDECLIPSE_BRANCH=stable
    if EXIST .git set REDECLIPSE_BRANCH=devel
    if EXIST "%REDECLIPSE_PATH%\bin\branch.txt" set /p REDECLIPSE_BRANCH=< "%REDECLIPSE_PATH%\bin\branch.txt"
)
if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "devel" (
    echo Unsupported update branch: "%REDECLIPSE_BRANCH%"
    exit /b 0
)
if NOT "%REDECLIPSE_BRANCH%" == "stable" goto notstable
if NOT EXIST "%REDECLIPSE_PATH%\bin\version.txt" (
    echo Unable to find %REDECLIPSE_PATH%\bin\version.txt
    exit /b 0
)
set /p REDECLIPSE_BINVER=< "%REDECLIPSE_PATH%\bin\version.txt"
if "%REDECLIPSE_BINVER%" == "" (
    echo Cannot determine current stable binaries version!
    exit /b 0
)
set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%/%REDECLIPSE_BINVER%
set REDECLIPSE_TEMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%\%REDECLIPSE_BINVER%
goto branch
:notstable
set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%
set REDECLIPSE_TEMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%
:branch
echo Branch: %REDECLIPSE_BRANCH%
echo Folder: %REDECLIPSE_PATH%
echo Cached: %REDECLIPSE_CACHE%
if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\wget.exe" (
    echo Unable to find wget.exe, are you sure it is in tools?
    exit /b 0
)
set REDECLIPSE_WGET="%REDECLIPSE_PATH%\bin\tools\wget.exe" --continue --no-check-certificate --user-agent="redeclipse-%REDECLIPSE_UPDATE%"
if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\unzip.exe" (
    echo Unable to find unzip.exe, are you sure it is in tools?
    exit /b 0
)
set REDECLIPSE_UNZIP="%REDECLIPSE_PATH%\bin\tools\unzip.exe"
if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\git-apply.exe" (
    echo Unable to find git-apply.exe, are you sure it is in tools?
    exit /b 0
)
set REDECLIPSE_GITAPPLY="%REDECLIPSE_PATH%\bin\tools\git-apply.exe" --ignore-space-change --ignore-whitespace --verbose --stat --apply
if NOT EXIST "%REDECLIPSE_TEMP%" mkdir "%REDECLIPSE_TEMP%"
echo @ECHO OFF> "%REDECLIPSE_TEMP%\install.bat"
echo setlocal ENABLEEXTENSIONS>> "%REDECLIPSE_TEMP%\install.bat"
echo set REDECLIPSE_ERROR=false>> "%REDECLIPSE_TEMP%\install.bat"
if NOT "%REDECLIPSE_BRANCH%" == "stable" goto binaries
:base
echo.
if EXIST "%REDECLIPSE_PATH%\bin\base.txt" set /p REDECLIPSE_BASE=< "%REDECLIPSE_PATH%\bin\base.txt"
if "%REDECLIPSE_BASE%" == "" set REDECLIPSE_BASE=none
echo [I] base: %REDECLIPSE_BASE%
set REDECLIPSE_OBASE=none
if NOT EXIST "%REDECLIPSE_TEMP%\base.txt" goto baseget
set /p REDECLIPSE_OBASE=< "%REDECLIPSE_TEMP%\base.txt"
if "%REDECLIPSE_OBASE%" == "" set REDECLIPSE_OBASE=none
echo [C] base: %REDECLIPSE_OBASE%
del /f /q "%REDECLIPSE_TEMP%\base.txt"
:baseget
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/base.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/base.txt"> nul 2>&1
if NOT EXIST "%REDECLIPSE_TEMP%\base.txt" (
    echo Failed to retrieve base update information.
    goto data
)
set /p REDECLIPSE_RBASE=< "%REDECLIPSE_TEMP%\base.txt"
if "%REDECLIPSE_RBASE%" == "" (
    echo Failed to retrieve base update information.
    goto data
)
echo [R] base: %REDECLIPSE_RBASE%
if "%REDECLIPSE_RBASE%" == "%REDECLIPSE_BASE%" goto data
if "%REDECLIPSE_BASE%" == "none" goto baseblob
:basepatch
if NOT "%REDECLIPSE_OBASE%" == "%REDECLIPSE_RBASE%" if EXIST "%REDECLIPSE_TEMP%\base.patch" del /f /q "%REDECLIPSE_TEMP%\base.patch"
echo [D] base: %REDECLIPSE_GITHUB%/base/compare/%REDECLIPSE_BASE%...%REDECLIPSE_RBASE%.patch
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/base.patch" "%REDECLIPSE_GITHUB%/base/compare/%REDECLIPSE_BASE%...%REDECLIPSE_RBASE%.patch"
if NOT EXIST "%REDECLIPSE_TEMP%\base.patch" (
    echo Failed to retrieve base update package. Downloading full zip instead.
    goto baseblob
)
echo %REDECLIPSE_GITAPPLY% "%REDECLIPSE_TEMP%\base.patch" --directory="%REDECLIPSE_PATH%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
echo     (echo %REDECLIPSE_RBASE%)^> "%REDECLIPSE_PATH%\bin\base.txt">> "%REDECLIPSE_TEMP%\install.bat"
echo ^) ^|^| ^(>> "%REDECLIPSE_TEMP%\install.bat"
echo     (echo 0)^> "%REDECLIPSE_PATH%\bin\base.txt">> "%REDECLIPSE_TEMP%\install.bat"
echo     set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
echo ^)>> "%REDECLIPSE_TEMP%\install.bat"
set REDECLIPSE_DEPLOY=true
goto data
:baseblob
if NOT "%REDECLIPSE_OBASE%" == "%REDECLIPSE_RBASE%" if EXIST "%REDECLIPSE_TEMP%\base.zip" del /f /q "%REDECLIPSE_TEMP%\base.zip"
echo [D] base: %REDECLIPSE_GITHUB%/base/zipball/%REDECLIPSE_RBASE%
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/base.zip" "%REDECLIPSE_GITHUB%/base/zipball/%REDECLIPSE_RBASE%"
if NOT EXIST "%REDECLIPSE_TEMP%\base.zip" (
    echo Failed to retrieve base update package.
    goto data
)
echo %REDECLIPSE_UNZIP% -o "%REDECLIPSE_TEMP%\base.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\base-%REDECLIPSE_RBASE%\*" "%REDECLIPSE_PATH%">> "%REDECLIPSE_TEMP%\install.bat"
echo    rmdir /s /q "%REDECLIPSE_TEMP%\base-%REDECLIPSE_RBASE%">> "%REDECLIPSE_TEMP%\install.bat"
echo    (echo %REDECLIPSE_RBASE%)^> "%REDECLIPSE_PATH%\bin\base.txt">> "%REDECLIPSE_TEMP%\install.bat"
echo ^) ^|^| set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
set REDECLIPSE_DEPLOY=true
:data
echo.
if EXIST "%REDECLIPSE_PATH%\data\readme.txt" goto dataver
echo Unable to find "data\readme.txt". Will start from scratch.
set REDECLIPSE_DATA=none
echo mkdir "%REDECLIPSE_PATH%\data">> "%REDECLIPSE_TEMP%\install.bat"
goto dataget
:dataver
if EXIST "%REDECLIPSE_PATH%\bin\data.txt" set /p REDECLIPSE_DATA=< "%REDECLIPSE_PATH%\bin\data.txt"
if "%REDECLIPSE_DATA%" == "" set REDECLIPSE_DATA=none
echo [I] data: %REDECLIPSE_DATA%
set REDECLIPSE_ODATA=none
if NOT EXIST "%REDECLIPSE_TEMP%\data.txt" goto dataget
set /p REDECLIPSE_ODATA=< "%REDECLIPSE_TEMP%\data.txt"
if "%REDECLIPSE_ODATA%" == "" set REDECLIPSE_ODATA=none
echo [C] data: %REDECLIPSE_ODATA%
del /f /q "%REDECLIPSE_TEMP%\data.txt"
:dataget
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/data.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/data.txt"> nul 2>&1
if NOT EXIST "%REDECLIPSE_TEMP%\data.txt" (
    echo Failed to retrieve data update information.
    goto binaries
)
set /p REDECLIPSE_RDATA=< "%REDECLIPSE_TEMP%\data.txt"
if "%REDECLIPSE_RDATA%" == "" (
    echo Failed to retrieve data update information.
    goto binaries
)
echo [R] data: %REDECLIPSE_RDATA%
if "%REDECLIPSE_RDATA%" == "%REDECLIPSE_DATA%" goto binaries
if "%REDECLIPSE_DATA%" == "none" goto datablob
:datapatch
if NOT "%REDECLIPSE_ODATA%" == "%REDECLIPSE_RDATA%" if EXIST "%REDECLIPSE_TEMP%\data.patch" del /f /q "%REDECLIPSE_TEMP%\data.patch"
echo [D] data: %REDECLIPSE_GITHUB%/data/compare/%REDECLIPSE_DATA%...%REDECLIPSE_RDATA%.patch
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/data.patch" "%REDECLIPSE_GITHUB%/data/compare/%REDECLIPSE_DATA%...%REDECLIPSE_RDATA%.patch"
if NOT EXIST "%REDECLIPSE_TEMP%\data.patch" (
    echo Failed to retrieve data update package. Downloading full zip instead.
    goto datablob
)
echo %REDECLIPSE_GITAPPLY% "%REDECLIPSE_TEMP%\data.patch" --directory="%REDECLIPSE_PATH%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
echo     (echo %REDECLIPSE_RDATA%)^> "%REDECLIPSE_PATH%\bin\data.txt">> "%REDECLIPSE_TEMP%\install.bat"
echo ^) ^|^| ^(>> "%REDECLIPSE_TEMP%\install.bat"
echo     (echo 0)^> "%REDECLIPSE_PATH%\bin\data.txt">> "%REDECLIPSE_TEMP%\install.bat"
echo     set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
echo ^)>> "%REDECLIPSE_TEMP%\install.bat"
set REDECLIPSE_DEPLOY=true
goto binaries
:datablob
echo [D] data: %REDECLIPSE_GITHUB%/data/zipball/%REDECLIPSE_RDATA%
if NOT "%REDECLIPSE_ODATA%" == "%REDECLIPSE_RDATA%" if EXIST "%REDECLIPSE_TEMP%\data.zip" del /f /q "%REDECLIPSE_TEMP%\data.zip"
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/data.zip" "%REDECLIPSE_GITHUB%/data/zipball/%REDECLIPSE_RDATA%"
if NOT EXIST "%REDECLIPSE_TEMP%\data.zip" (
    echo Failed to retrieve data update package.
    goto binaries
)
echo %REDECLIPSE_UNZIP% -o "%REDECLIPSE_TEMP%\data.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\data-%REDECLIPSE_RDATA%\*" "%REDECLIPSE_PATH%\data">> "%REDECLIPSE_TEMP%\install.bat"
echo    rmdir /s /q "%REDECLIPSE_TEMP%\data-%REDECLIPSE_RDATA%">> "%REDECLIPSE_TEMP%\install.bat"
echo    (echo %REDECLIPSE_RDATA%)^> "%REDECLIPSE_PATH%\bin\data.txt">> "%REDECLIPSE_TEMP%\install.bat"
echo ^) ^|^| set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
set REDECLIPSE_DEPLOY=true
:binaries
echo.
if EXIST "%REDECLIPSE_PATH%\bin\binaries.txt" set /p REDECLIPSE_BINARIES=< "%REDECLIPSE_PATH%\bin\binaries.txt"
if "%REDECLIPSE_BINARIES%" == "" set REDECLIPSE_BINARIES=none
echo [I] binaries: %REDECLIPSE_BINARIES%
:binariesget
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/binaries.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/binaries.txt"> nul 2>&1
if NOT EXIST "%REDECLIPSE_TEMP%\binaries.txt" (
    echo Failed to retrieve binaries update information.
    goto deploy
)
set /p REDECLIPSE_RBINARIES=< "%REDECLIPSE_TEMP%\binaries.txt"
if "%REDECLIPSE_RBINARIES%" == "" (
    echo Failed to retrieve binaries update information.
    goto deploy
)
echo [R] binaries: %REDECLIPSE_RBINARIES%
if NOT "%REDECLIPSE_TRYUPDATE%" == "1" if "%REDECLIPSE_RBINARIES%" == "%REDECLIPSE_BINARIES%" goto deploy
echo [D] binaries: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
if EXIST "%REDECLIPSE_TEMP%\windows.zip" del /f /q "%REDECLIPSE_TEMP%\windows.zip"
%REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%/windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
if NOT EXIST "%REDECLIPSE_TEMP%\windows.zip" (
    echo Failed to retrieve binaries update package.
    goto deploy
)
echo %REDECLIPSE_UNZIP% -o "%REDECLIPSE_TEMP%\windows.zip" -d "%REDECLIPSE_PATH%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
echo     (echo %REDECLIPSE_RBINARIES%)^> "%REDECLIPSE_PATH%\bin\binaries.txt">> "%REDECLIPSE_TEMP%\install.bat"
echo ^) ^|^| set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
set REDECLIPSE_DEPLOY=true
:deploy
if NOT "%REDECLIPSE_DEPLOY%" == "true" (
    echo.
    echo Everything is already up to date!
    exit /b 0
)
echo if "%%REDECLIPSE_ERROR%%" == "true" (exit /b 1)>> "%REDECLIPSE_TEMP%\install.bat"
echo Deploying: "%REDECLIPSE_TEMP%\install.bat"
set REDECLIPSE_INSTALL=call
copy /y nul test.tmp> nul 2>&1 && (
    del /f /q test.tmp
    goto unpack
)
echo Administrator permissions are required to deploy the files.
if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\elevate.exe" (
    echo Unable to find elevate.exe, are you sure it is in bin/tools?
    goto unpack
)
set REDECLIPSE_INSTALL="%REDECLIPSE_PATH%\bin\tools\elevate.exe" -wait
:unpack
%REDECLIPSE_INSTALL% "%REDECLIPSE_TEMP%\install.bat" && (
    echo.
    echo Updated successfully!
    (echo %REDECLIPSE_BRANCH%)> "%REDECLIPSE_PATH%\bin\branch.txt"
    exit /b 0
) || (
    echo.
    echo There was an error deploying the files.
    exit /b 1
)
