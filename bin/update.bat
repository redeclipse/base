@ECHO OFF
setlocal enableextensions enabledelayedexpansion
if DEFINED REDECLIPSE_PATH goto setup
pushd %~dp0\..
set REDECLIPSE_PATH=%CD%
popd
:setup
    if DEFINED REDECLIPSE_CACHE goto start
    for /f "tokens=3* delims= " %%a in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" /v "Personal"') do set USERMYDOCS=%%a
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
        echo Cannot determine current stable bins version.
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
    if NOT "%REDECLIPSE_BRANCH%" == "stable" goto bins
:base
    echo.
    if EXIST "%REDECLIPSE_PATH%\bin\base.txt" set /p REDECLIPSE_BASE=< "%REDECLIPSE_PATH%\bin\base.txt"
    if "%REDECLIPSE_BASE%" == "" set REDECLIPSE_BASE=none
    echo [I] base: %REDECLIPSE_BASE%
    set REDECLIPSE_BASE_CACHED=none
    if NOT EXIST "%REDECLIPSE_TEMP%\base.txt" goto baseget
    set /p REDECLIPSE_BASE_CACHED=< "%REDECLIPSE_TEMP%\base.txt"
    if "%REDECLIPSE_BASE_CACHED%" == "" set REDECLIPSE_BASE_CACHED=none
    echo [C] base: %REDECLIPSE_BASE_CACHED%
    del /f /q "%REDECLIPSE_TEMP%\base.txt"
:baseget
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\base.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/base.txt"> nul 2>&1
    if NOT EXIST "%REDECLIPSE_TEMP%\base.txt" (
        echo Failed to retrieve base update information.
        goto data
    )
    set /p REDECLIPSE_BASE_REMOTE=< "%REDECLIPSE_TEMP%\base.txt"
    if "%REDECLIPSE_BASE_REMOTE%" == "" (
        echo Failed to retrieve base update information.
        goto data
    )
    echo [R] base: %REDECLIPSE_BASE_REMOTE%
    if "%REDECLIPSE_BASE_REMOTE%" == "%REDECLIPSE_BASE%" goto data
    if "%REDECLIPSE_BASE%" == "none" goto baseblob
:basepatch
    if EXIST "%REDECLIPSE_TEMP%\base.patch" del /f /q "%REDECLIPSE_TEMP%\base.patch"
    echo [D] base: %REDECLIPSE_GITHUB%/base/compare/%REDECLIPSE_BASE%...%REDECLIPSE_BASE_REMOTE%.patch
    echo.
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\base.patch" "%REDECLIPSE_GITHUB%/base/compare/%REDECLIPSE_BASE%...%REDECLIPSE_BASE_REMOTE%.patch"
    if NOT EXIST "%REDECLIPSE_TEMP%\base.patch" (
        echo Failed to retrieve base update package. Downloading full zip instead.
        goto baseblob
    )
:basepatchdeploy
    echo %REDECLIPSE_GITAPPLY% --directory="%REDECLIPSE_PATH%" "%REDECLIPSE_TEMP%\base.patch" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     (echo %REDECLIPSE_BASE_REMOTE%)^> "%REDECLIPSE_PATH%\bin\base.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
    set REDECLIPSE_DEPLOY=true
    goto data
:baseblob
    if EXIST "%REDECLIPSE_TEMP%\base.zip" (
        if "%REDECLIPSE_BASE_CACHED%" == "%REDECLIPSE_BASE_REMOTE%" (
            echo [F] base: Using cached file "%REDECLIPSE_TEMP%\base.zip"
            goto baseblobdeploy
        ) else del /f /q "%REDECLIPSE_TEMP%\base.zip"
    )
    echo [D] base: %REDECLIPSE_GITHUB%/base/zipball/%REDECLIPSE_BASE_REMOTE%
    echo.
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\base.zip" "%REDECLIPSE_GITHUB%/base/zipball/%REDECLIPSE_BASE_REMOTE%"
    if NOT EXIST "%REDECLIPSE_TEMP%\base.zip" (
        echo Failed to retrieve base update package.
        goto data
    )
:baseblobdeploy
    echo %REDECLIPSE_UNZIP% -o "%REDECLIPSE_TEMP%\base.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\red-eclipse-base-%REDECLIPSE_BASE_REMOTE:~0,7%\*" "%REDECLIPSE_PATH%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    rmdir /s /q "%REDECLIPSE_TEMP%\red-eclipse-base-%REDECLIPSE_BASE_REMOTE:~0,7%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    (echo %REDECLIPSE_BASE_REMOTE%)^> "%REDECLIPSE_PATH%\bin\base.txt">> "%REDECLIPSE_TEMP%\install.bat"
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
    set REDECLIPSE_DATA_CACHED=none
    if NOT EXIST "%REDECLIPSE_TEMP%\data.txt" goto dataget
    set /p REDECLIPSE_DATA_CACHED=< "%REDECLIPSE_TEMP%\data.txt"
    if "%REDECLIPSE_DATA_CACHED%" == "" set REDECLIPSE_DATA_CACHED=none
    echo [C] data: %REDECLIPSE_DATA_CACHED%
    del /f /q "%REDECLIPSE_TEMP%\data.txt"
:dataget
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\data.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/data.txt"> nul 2>&1
    if NOT EXIST "%REDECLIPSE_TEMP%\data.txt" (
        echo Failed to retrieve data update information.
        goto bins
    )
    set /p REDECLIPSE_DATA_REMOTE=< "%REDECLIPSE_TEMP%\data.txt"
    if "%REDECLIPSE_DATA_REMOTE%" == "" (
        echo Failed to retrieve data update information.
        goto bins
    )
    echo [R] data: %REDECLIPSE_DATA_REMOTE%
    if "%REDECLIPSE_DATA_REMOTE%" == "%REDECLIPSE_DATA%" goto bins
    if "%REDECLIPSE_DATA%" == "none" goto datablob
:datapatch
    if EXIST "%REDECLIPSE_TEMP%\data.patch" del /f /q "%REDECLIPSE_TEMP%\data.patch"
    echo [D] data: %REDECLIPSE_GITHUB%/data/compare/%REDECLIPSE_DATA%...%REDECLIPSE_DATA_REMOTE%.patch
    echo.
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\data.patch" "%REDECLIPSE_GITHUB%/data/compare/%REDECLIPSE_DATA%...%REDECLIPSE_DATA_REMOTE%.patch"
    if NOT EXIST "%REDECLIPSE_TEMP%\data.patch" (
        echo Failed to retrieve data update package. Downloading full zip instead.
        goto datablob
    )
:datapatchdeploy
    echo %REDECLIPSE_GITAPPLY% --directory="%REDECLIPSE_PATH%\data" "%REDECLIPSE_TEMP%\data.patch" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     (echo %REDECLIPSE_DATA_REMOTE%)^> "%REDECLIPSE_PATH%\bin\data.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
    set REDECLIPSE_DEPLOY=true
    goto bins
:datablob
    if EXIST "%REDECLIPSE_TEMP%\data.zip" (
        if "%REDECLIPSE_DATA_CACHED%" == "%REDECLIPSE_DATA_REMOTE%" (
            echo [F] data: Using cached file "%REDECLIPSE_TEMP%\data.zip"
            goto datablobdeploy
        ) else del /f /q "%REDECLIPSE_TEMP%\data.zip"
    )
    echo [D] data: %REDECLIPSE_GITHUB%/data/zipball/%REDECLIPSE_DATA_REMOTE%
    echo.
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\data.zip" "%REDECLIPSE_GITHUB%/data/zipball/%REDECLIPSE_DATA_REMOTE%"
    if NOT EXIST "%REDECLIPSE_TEMP%\data.zip" (
        echo Failed to retrieve data update package.
        goto bins
    )
:datablobdeploy
    echo %REDECLIPSE_UNZIP% -o "%REDECLIPSE_TEMP%\data.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\red-eclipse-data-%REDECLIPSE_DATA_REMOTE:~0,7%\*" "%REDECLIPSE_PATH%\data">> "%REDECLIPSE_TEMP%\install.bat"
    echo    rmdir /s /q "%REDECLIPSE_TEMP%\red-eclipse-data-%REDECLIPSE_DATA_REMOTE:~0,7%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    (echo %REDECLIPSE_DATA_REMOTE%)^> "%REDECLIPSE_PATH%\bin\data.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
    set REDECLIPSE_DEPLOY=true
:bins
    echo.
    if EXIST "%REDECLIPSE_PATH%\bin\bins.txt" set /p REDECLIPSE_BINS=< "%REDECLIPSE_PATH%\bin\bins.txt"
    if "%REDECLIPSE_BINS%" == "" set REDECLIPSE_BINS=none
    echo [I] bins: %REDECLIPSE_BINS%
    set REDECLIPSE_BINS_CACHED=none
    if NOT EXIST "%REDECLIPSE_TEMP%\bins.txt" goto binsget
    set /p REDECLIPSE_BINS_CACHED=< "%REDECLIPSE_TEMP%\bins.txt"
    if "%REDECLIPSE_BINS_CACHED%" == "" set REDECLIPSE_BINS_CACHED=none
    echo [C] bins: %REDECLIPSE_BINS_CACHED%
:binsget
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\bins.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/bins.txt"> nul 2>&1
    if NOT EXIST "%REDECLIPSE_TEMP%\bins.txt" (
        echo Failed to retrieve bins update information.
        goto deploy
    )
    set /p REDECLIPSE_BINS_REMOTE=< "%REDECLIPSE_TEMP%\bins.txt"
    if "%REDECLIPSE_BINS_REMOTE%" == "" (
        echo Failed to retrieve bins update information.
        goto deploy
    )
    echo [R] bins: %REDECLIPSE_BINS_REMOTE%
    if NOT "%REDECLIPSE_TRYUPDATE%" == "true" if "%REDECLIPSE_BINS_REMOTE%" == "%REDECLIPSE_BINS%" goto deploy
:binsblob
    if EXIST "%REDECLIPSE_TEMP%\windows.zip" (
        if "%REDECLIPSE_BINS_CACHED%" == "%REDECLIPSE_BINS_REMOTE%" (
            echo [F] bins: Using cached file "%REDECLIPSE_TEMP%\windows.zip"
            goto binsdeploy
        ) else del /f /q "%REDECLIPSE_TEMP%\windows.zip"
    )
    echo [D] bins: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
    echo.
    %REDECLIPSE_WGET% --output-document="%REDECLIPSE_TEMP%\windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
    if NOT EXIST "%REDECLIPSE_TEMP%\windows.zip" (
        echo Failed to retrieve bins update package.
        goto deploy
    )
:binsdeploy
    echo %REDECLIPSE_UNZIP% -o "%REDECLIPSE_TEMP%\windows.zip" -d "%REDECLIPSE_PATH%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     (echo %REDECLIPSE_BINS_REMOTE%)^> "%REDECLIPSE_PATH%\bin\bins.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
    set REDECLIPSE_DEPLOY=true
:deploy
    echo.
    if NOT "%REDECLIPSE_DEPLOY%" == "true" (
        echo Everything is already up to date.
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
        echo Updated successfully.
        (echo %REDECLIPSE_BRANCH%)> "%REDECLIPSE_PATH%\bin\branch.txt"
        exit /b 0
    ) || (
        echo.
        echo There was an error deploying the files.
        exit /b 1
    )
