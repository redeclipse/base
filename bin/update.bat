@ECHO OFF
setlocal enableextensions enabledelayedexpansion
:path
    if DEFINED REDECLIPSE_PATH goto init
    pushd "%~dp0\.."
    set REDECLIPSE_PATH=%CD%
    popd
:init
    if NOT DEFINED REDECLIPSE_UPDATER set REDECLIPSE_UPDATER=%~dp0\%0
    if NOT DEFINED REDECLIPSE_SOURCE set REDECLIPSE_SOURCE=http://redeclipse.net/files
    if NOT DEFINED REDECLIPSE_GITHUB set REDECLIPSE_GITHUB=https://github.com/red-eclipse
    if DEFINED REDECLIPSE_CACHE goto setup
    for /f "tokens=3* delims= " %%a in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" /v "Personal"') do set REDECLIPSE_WINDOCS=%%a
    if EXIST "%REDECLIPSE_WINDOCS%" (
        set REDECLIPSE_CACHE=%REDECLIPSE_WINDOCS%\My Games\Red Eclipse\cache
    ) else if EXIST "%REDECLIPSE_HOME%" (
        set REDECLIPSE_CACHE=%REDECLIPSE_HOME%\cache
    ) else (
        set REDECLIPSE_CACHE=cache
    )
:setup
    if NOT DEFINED REDECLIPSE_BRANCH (
        set REDECLIPSE_BRANCH=stable
        if EXIST .git set REDECLIPSE_BRANCH=master
        if EXIST "%REDECLIPSE_PATH%\branch.txt" set /p REDECLIPSE_BRANCH=< "%REDECLIPSE_PATH%\branch.txt"
    )
    if "%REDECLIPSE_BRANCH%" == "devel" set REDECLIPSE_BRANCH=master
    if NOT "%REDECLIPSE_BRANCH%" == "stable" if NOT "%REDECLIPSE_BRANCH%" == "master" (
        echo Unsupported update branch: "%REDECLIPSE_BRANCH%"
        exit /b 0
    )
    set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%
    set REDECLIPSE_TEMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%
:branch
    echo Branch: %REDECLIPSE_UPDATE%
    echo Folder: %REDECLIPSE_PATH%
    echo Cached: %REDECLIPSE_TEMP%
    if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\curl.exe" (
        echo Unable to find curl.exe, are you sure it is in tools?
        exit /b 0
    )
    set REDECLIPSE_CURL="%REDECLIPSE_PATH%\bin\tools\curl.exe" --location --insecure --fail --user-agent "redeclipse-%REDECLIPSE_UPDATE%"
    if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\unzip.exe" (
        echo Unable to find unzip.exe, are you sure it is in tools?
        exit /b 0
    )
    set REDECLIPSE_UNZIP="%REDECLIPSE_PATH%\bin\tools\unzip.exe" -o
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
:modules
    %REDECLIPSE_CURL% --silent --output "%REDECLIPSE_TEMP%\modules.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/modules.txt"
    if NOT EXIST "%REDECLIPSE_TEMP%\modules.txt" (
        echo Failed to retrieve modules update information.
        goto bins
    )
    set /p REDECLIPSE_MODULE_LIST=< "%REDECLIPSE_TEMP%\modules.txt"
    if "%REDECLIPSE_MODULE_LIST%" == "" (
        echo Failed to get module list, continuing..
        goto bins
    )
    for /f %%a in %REDECLIPSE_MODULE_LIST% do (
        set REDEECLIPSE_MODULE_RUN=%%a
        if NOT "!REDEECLIPSE_MODULE_RUN!" == "" (
            call :module "%REDECLIPSE_UPDATER%" && (set REDECLIPSE_DEPLOY=true) || (echo There was an error updating module !REDEECLIPSE_MODULE_RUN!, continuing..)
        )
    )
:module
    echo.
    if "%REDEECLIPSE_MODULE_RUN%" == "" exit /b 1
    if "%REDEECLIPSE_MODULE_RUN%" == "base" (set REDEECLIPSE_MODULE_DIR=) else (set REDEECLIPSE_MODULE_DIR=\%REDEECLIPSE_MODULE_RUN%)
    if EXIST "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%\readme.txt" goto modulever
    echo Unable to find ".%REDEECLIPSE_MODULE_DIR%\readme.txt". Will start from scratch.
    set REDECLIPSE_MODULE_INSTALLED=none
    echo mkdir "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%">> "%REDECLIPSE_TEMP%\install.bat"
    goto moduleget
:modulever
    if EXIST "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%\version.txt" set /p REDECLIPSE_MODULE_INSTALLED=< "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%\version.txt"
    if "%REDECLIPSE_MODULE_INSTALLED%" == "" set REDECLIPSE_MODULE_INSTALLED=none
    echo [I] %REDEECLIPSE_MODULE_RUN%: %REDECLIPSE_MODULE_INSTALLED%
    set REDECLIPSE_MODULE_CACHED=none
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt" goto moduleget
    set /p REDECLIPSE_MODULE_CACHED=< "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt"
    if "%REDECLIPSE_MODULE_CACHED%" == "" set REDECLIPSE_MODULE_CACHED=none
    echo [C] %REDEECLIPSE_MODULE_RUN%: %REDECLIPSE_MODULE_CACHED%
    del /f /q "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt"
:moduleget
    %REDECLIPSE_CURL% --silent --output "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/%REDEECLIPSE_MODULE_RUN%.txt"
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt" (
        echo Failed to retrieve %REDEECLIPSE_MODULE_RUN% update information.
        exit /b 1
    )
    set /p REDECLIPSE_MODULE_REMOTE=< "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt"
    if "%REDECLIPSE_MODULE_REMOTE%" == "" (
        echo Failed to read %REDEECLIPSE_MODULE_RUN% update information.
        exit /b 1
    )
    echo [R] %REDEECLIPSE_MODULE_RUN%: %REDECLIPSE_MODULE_REMOTE%
    if "%REDECLIPSE_MODULE_REMOTE%" == "%REDECLIPSE_MODULE_INSTALLED%" exit /b 0
    if "%REDECLIPSE_MODULE_INSTALLED%" == "none" goto moduleblob
:modulepatch
    if EXIST "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.patch" del /f /q "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.patch"
    if EXIST "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip" del /f /q "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip"
    echo [D] %REDEECLIPSE_MODULE_RUN%: %REDECLIPSE_GITHUB%/%REDEECLIPSE_MODULE_RUN%/compare/%REDECLIPSE_MODULE_INSTALLED%...%REDECLIPSE_MODULE_REMOTE%.patch
    echo.
    %REDECLIPSE_CURL% --output "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.patch" "%REDECLIPSE_GITHUB%/%REDEECLIPSE_MODULE_RUN%/compare/%REDECLIPSE_MODULE_INSTALLED%...%REDECLIPSE_MODULE_REMOTE%.patch"
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.patch" (
        echo Failed to retrieve %REDEECLIPSE_MODULE_RUN% update package. Downloading full zip instead.
        goto moduleblob
    )
:modulepatchdeploy
    echo %REDECLIPSE_GITAPPLY% --directory="%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%" "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.patch" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     ^(echo %REDECLIPSE_MODULE_REMOTE%^)^> "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%\version.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     ^(echo none^)^> "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%\version.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo     del /f /q "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo     set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
    echo ^)>> "%REDECLIPSE_TEMP%\install.bat"
    exit /b 0
:moduleblob
    if EXIST "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip" (
        if "%REDECLIPSE_MODULE_CACHED%" == "%REDECLIPSE_MODULE_REMOTE%" (
            echo [F] %REDEECLIPSE_MODULE_RUN%: Using cached file "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip"
            goto moduleblobdeploy
        ) else del /f /q "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip"
    )
    echo [D] %REDEECLIPSE_MODULE_RUN%: %REDECLIPSE_GITHUB%/%REDEECLIPSE_MODULE_RUN%/zipball/%REDECLIPSE_MODULE_REMOTE%
    echo.
    %REDECLIPSE_CURL% --output "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip" "%REDECLIPSE_GITHUB%/%REDEECLIPSE_MODULE_RUN%/zipball/%REDECLIPSE_MODULE_REMOTE%"
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip" (
        echo Failed to retrieve %REDEECLIPSE_MODULE_RUN% update package.
        exit /b 1
    )
:moduleblobdeploy
    echo %REDECLIPSE_UNZIP% "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\red-eclipse-%REDEECLIPSE_MODULE_RUN%-%REDECLIPSE_MODULE_REMOTE:~0,7%\*" "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    rmdir /s /q "%REDECLIPSE_TEMP%\red-eclipse-%REDEECLIPSE_MODULE_RUN%-%REDECLIPSE_MODULE_REMOTE:~0,7%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    ^(echo %REDECLIPSE_MODULE_REMOTE%^)^> "%REDECLIPSE_PATH%%REDEECLIPSE_MODULE_DIR%\version.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     del /f /q "%REDECLIPSE_TEMP%\%REDEECLIPSE_MODULE_RUN%.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo     set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
    echo ^)>> "%REDECLIPSE_TEMP%\install.bat"
    exit /b 0
:bins
    echo.
    if EXIST "%REDECLIPSE_PATH%\bin\version.txt" set /p REDECLIPSE_BINS=< "%REDECLIPSE_PATH%\bin\version.txt"
    if "%REDECLIPSE_BINS%" == "" set REDECLIPSE_BINS=none
    echo [I] bins: %REDECLIPSE_BINS%
    set REDECLIPSE_BINS_CACHED=none
    if NOT EXIST "%REDECLIPSE_TEMP%\bins.txt" goto binsget
    set /p REDECLIPSE_BINS_CACHED=< "%REDECLIPSE_TEMP%\bins.txt"
    if "%REDECLIPSE_BINS_CACHED%" == "" set REDECLIPSE_BINS_CACHED=none
    echo [C] bins: %REDECLIPSE_BINS_CACHED%
    del /f /q "%REDECLIPSE_TEMP%\bins.txt"
:binsget
    %REDECLIPSE_CURL% --silent --output "%REDECLIPSE_TEMP%\bins.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/bins.txt"
    if NOT EXIST "%REDECLIPSE_TEMP%\bins.txt" (
        echo Failed to retrieve bins update information.
        goto deploy
    )
    set /p REDECLIPSE_BINS_REMOTE=< "%REDECLIPSE_TEMP%\bins.txt"
    if "%REDECLIPSE_BINS_REMOTE%" == "" (
        echo Failed to read bins update information.
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
    %REDECLIPSE_CURL% --output "%REDECLIPSE_TEMP%\windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
    if NOT EXIST "%REDECLIPSE_TEMP%\windows.zip" (
        echo Failed to retrieve bins update package.
        goto deploy
    )
:binsdeploy
    echo %REDECLIPSE_UNZIP% "%REDECLIPSE_TEMP%\windows.zip" -d "%REDECLIPSE_PATH%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     ^(echo %REDECLIPSE_BINS_REMOTE%^)^> "%REDECLIPSE_PATH%\bin\version.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     del /f /q "%REDECLIPSE_TEMP%\bins.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo     set REDECLIPSE_ERROR=true>> "%REDECLIPSE_TEMP%\install.bat"
    echo ^)>> "%REDECLIPSE_TEMP%\install.bat"
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
        echo Unable to find elevate.exe, are you sure it is in tools?
        goto unpack
    )
    set REDECLIPSE_INSTALL="%REDECLIPSE_PATH%\bin\tools\elevate.exe" -wait
:unpack
%REDECLIPSE_INSTALL% "%REDECLIPSE_TEMP%\install.bat" && (
    echo.
    echo Updated successfully.
    (echo %REDECLIPSE_BRANCH%)> "%REDECLIPSE_PATH%\branch.txt"
    exit /b 0
) || (
    echo.
    echo There was an error deploying the files.
    exit /b 1
)
