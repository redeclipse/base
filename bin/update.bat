@ECHO OFF
setlocal enableextensions enabledelayedexpansion
:redeclipse_update_path
    if DEFINED REDECLIPSE_PATH goto redeclipse_update_init
    pushd "%~dp0\.."
    set REDECLIPSE_PATH=%CD%
    popd
:redeclipse_update_init
    if NOT "%REDECLIPSE_DEPLOY%" == "true" set REDECLIPSE_DEPLOY=false
    if NOT DEFINED REDECLIPSE_UPDATER set REDECLIPSE_UPDATER=%~dp0\%~0
    if NOT DEFINED REDECLIPSE_SOURCE set REDECLIPSE_SOURCE=http://redeclipse.net/files
    if NOT DEFINED REDECLIPSE_GITHUB set REDECLIPSE_GITHUB=https://github.com/red-eclipse
    if DEFINED REDECLIPSE_CACHE goto redeclipse_update_setup
    for /f "tokens=3* delims= " %%a in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" /v "Personal"') do set REDECLIPSE_WINDOCS=%%a
    if EXIST "%REDECLIPSE_WINDOCS%" (
        set REDECLIPSE_CACHE=%REDECLIPSE_WINDOCS%\My Games\Red Eclipse\cache
    ) else if EXIST "%REDECLIPSE_HOME%" (
        set REDECLIPSE_CACHE=%REDECLIPSE_HOME%\cache
    ) else (
        set REDECLIPSE_CACHE=cache
    )
:redeclipse_update_setup
    if EXIST "%REDECLIPSE_PATH%\branch.txt" set /p REDECLIPSE_BRANCH_CURRENT=< "%REDECLIPSE_PATH%\branch.txt"
    if NOT DEFINED REDECLIPSE_BRANCH (
        if DEFINED REDECLIPSE_BRANCH_CURRENT (
            set REDECLIPSE_BRANCH=%REDECLIPSE_BRANCH_CURRENT%
        ) else if EXIST .git (
            set REDECLIPSE_BRANCH=devel
        ) else (
            set REDECLIPSE_BRANCH=stable
        )
    )
    set REDECLIPSE_UPDATE=%REDECLIPSE_BRANCH%
    set REDECLIPSE_TEMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%
:redeclipse_update_branch
    echo branch: %REDECLIPSE_UPDATE%
    echo folder: %REDECLIPSE_PATH%
    echo cached: %REDECLIPSE_TEMP%
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
    if NOT EXIST "%REDECLIPSE_TEMP%" mkdir "%REDECLIPSE_TEMP%"
    echo @ECHO OFF> "%REDECLIPSE_TEMP%\install.bat"
    echo setlocal enableextensions>> "%REDECLIPSE_TEMP%\install.bat"
    if "%REDECLIPSE_BRANCH%" == "devel" goto redeclipse_update_bins_run
:redeclipse_update_module
    %REDECLIPSE_CURL% --silent --output "%REDECLIPSE_TEMP%\mods.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/mods.txt"
    if NOT EXIST "%REDECLIPSE_TEMP%\mods.txt" (
        echo Failed to retrieve modules update information.
        goto redeclipse_update_bins_run
    )
    set /p REDECLIPSE_MODULE_LIST=< "%REDECLIPSE_TEMP%\mods.txt"
    if "%REDECLIPSE_MODULE_LIST%" == "" (
        echo Failed to get module list, continuing..
        goto redeclipse_update_bins_run
    )
    if EXIST "%REDECLIPSE_TEMP%\data.txt" del /f /q "%REDECLIPSE_TEMP%\data.txt"
    if EXIST "%REDECLIPSE_TEMP%\data.zip" del /f /q "%REDECLIPSE_TEMP%\data.zip"
    for %%a in (%REDECLIPSE_MODULE_LIST%) do (
        set REDECLIPSE_MODULE_RUN=%%a
        if NOT "!REDECLIPSE_MODULE_RUN!" == "" (
            call :redeclipse_update_module_run "%REDECLIPSE_UPDATER%" || (echo !REDECLIPSE_MODULE_RUN!: There was an error updating the module, continuing..)
        )
    )
    goto redeclipse_update_bins_run
:redeclipse_update_module_run
    echo.
    if "%REDECLIPSE_MODULE_RUN%" == "base" (set REDECLIPSE_MODULE_DIR=) else (set REDECLIPSE_MODULE_DIR=\data\%REDECLIPSE_MODULE_RUN%)
    if EXIST "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%\version.txt" goto redeclipse_update_module_ver
    echo %REDECLIPSE_MODULE_RUN%: Unable to find version.txt. Will start from scratch.
    set REDECLIPSE_MODULE_INSTALLED=none
    echo mkdir "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%">> "%REDECLIPSE_TEMP%\install.bat"
    goto redeclipse_update_module_get
:redeclipse_update_module_ver
    if EXIST "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%\version.txt" set /p REDECLIPSE_MODULE_INSTALLED=< "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%\version.txt"
    if "%REDECLIPSE_MODULE_INSTALLED%" == "" set REDECLIPSE_MODULE_INSTALLED=none
    echo %REDECLIPSE_MODULE_RUN%: %REDECLIPSE_MODULE_INSTALLED% is installed.
    set REDECLIPSE_MODULE_CACHED=none
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.txt" goto redeclipse_update_module_get
    set /p REDECLIPSE_MODULE_CACHED=< "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.txt"
    if "%REDECLIPSE_MODULE_CACHED%" == "" set REDECLIPSE_MODULE_CACHED=none
    echo %REDECLIPSE_MODULE_RUN%: %REDECLIPSE_MODULE_CACHED% is in the cache.
    del /f /q "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.txt"
:redeclipse_update_module_get
    %REDECLIPSE_CURL% --silent --output "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/%REDECLIPSE_MODULE_RUN%.txt"
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.txt" (
        echo %REDECLIPSE_MODULE_RUN%: Failed to retrieve update information.
        exit /b 1
    )
    set /p REDECLIPSE_MODULE_REMOTE=< "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.txt"
    if "%REDECLIPSE_MODULE_REMOTE%" == "" (
        echo %REDECLIPSE_MODULE_RUN%: Failed to read update information.
        exit /b 1
    )
    echo %REDECLIPSE_MODULE_RUN%: %REDECLIPSE_MODULE_REMOTE% is the current version.
    if "%REDECLIPSE_MODULE_REMOTE%" == "%REDECLIPSE_MODULE_INSTALLED%" (
        echo echo %REDECLIPSE_MODULE_RUN%: already up to date.>> "%REDECLIPSE_TEMP%\install.bat"
        exit /b 0
    )
:redeclipse_update_module_blob
    if EXIST "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" (
        if "%REDECLIPSE_MODULE_CACHED%" == "%REDECLIPSE_MODULE_REMOTE%" (
            echo %REDECLIPSE_MODULE_RUN%: Using cached file "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip"
            goto redeclipse_update_module_blob_deploy
        ) else del /f /q "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip"
    )
    echo %REDECLIPSE_MODULE_RUN%: %REDECLIPSE_GITHUB%/%REDECLIPSE_MODULE_RUN%/zipball/%REDECLIPSE_MODULE_REMOTE%
    echo.
    %REDECLIPSE_CURL% --output "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" "%REDECLIPSE_GITHUB%/%REDECLIPSE_MODULE_RUN%/zipball/%REDECLIPSE_MODULE_REMOTE%"
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" (
        echo %REDECLIPSE_MODULE_RUN%: Failed to retrieve update package.
        exit /b 1
    )
:redeclipse_update_module_blob_deploy
    echo echo %REDECLIPSE_MODULE_RUN%: deploying blob.>> "%REDECLIPSE_TEMP%\install.bat"
    echo %REDECLIPSE_UNZIP% "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" -d "%REDECLIPSE_TEMP%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    if "%REDECLIPSE_MODULE_RUN%" == "base" goto redeclipse_update_module_blob_deploy_ext
    echo    rmdir /s /q "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    mkdir "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%">> "%REDECLIPSE_TEMP%\install.bat"
:redeclipse_update_module_blob_deploy_ext
    echo    xcopy /e /c /i /f /h /y "%REDECLIPSE_TEMP%\red-eclipse-%REDECLIPSE_MODULE_RUN%-%REDECLIPSE_MODULE_REMOTE:~0,7%\*" "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    rmdir /s /q "%REDECLIPSE_TEMP%\red-eclipse-%REDECLIPSE_MODULE_RUN%-%REDECLIPSE_MODULE_REMOTE:~0,7%">> "%REDECLIPSE_TEMP%\install.bat"
    echo    ^(echo %REDECLIPSE_MODULE_REMOTE%^)^> "%REDECLIPSE_PATH%%REDECLIPSE_MODULE_DIR%\version.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     del /f /q "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo     exit 1>> "%REDECLIPSE_TEMP%\install.bat"
    echo ^)>> "%REDECLIPSE_TEMP%\install.bat"
    set REDECLIPSE_DEPLOY=true
    exit /b 0
:redeclipse_update_bins_run
    echo.
    if EXIST "%REDECLIPSE_PATH%\bin\version.txt" set /p REDECLIPSE_BINS=< "%REDECLIPSE_PATH%\bin\version.txt"
    if "%REDECLIPSE_BINS%" == "" set REDECLIPSE_BINS=none
    echo bins: %REDECLIPSE_BINS% is installed.
    set REDECLIPSE_BINS_CACHED=none
    if NOT EXIST "%REDECLIPSE_TEMP%\bins.txt" goto redeclipse_update_bins_get
    set /p REDECLIPSE_BINS_CACHED=< "%REDECLIPSE_TEMP%\bins.txt"
    if "%REDECLIPSE_BINS_CACHED%" == "" set REDECLIPSE_BINS_CACHED=none
    echo bins: %REDECLIPSE_BINS_CACHED% is in the cache.
    del /f /q "%REDECLIPSE_TEMP%\bins.txt"
:redeclipse_update_bins_get
    %REDECLIPSE_CURL% --silent --output "%REDECLIPSE_TEMP%\bins.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/bins.txt"
    if NOT EXIST "%REDECLIPSE_TEMP%\bins.txt" (
        echo bins: Failed to retrieve update information.
        goto redeclipse_update_deploy
    )
    set /p REDECLIPSE_BINS_REMOTE=< "%REDECLIPSE_TEMP%\bins.txt"
    if "%REDECLIPSE_BINS_REMOTE%" == "" (
        echo bins: Failed to read update information.
        goto redeclipse_update_deploy
    )
    echo bins: %REDECLIPSE_BINS_REMOTE% is the current version.
    if NOT "%REDECLIPSE_TRYUPDATE%" == "true" if "%REDECLIPSE_BINS_REMOTE%" == "%REDECLIPSE_BINS%" (
        echo echo bins: already up to date.>> "%REDECLIPSE_TEMP%\install.bat"
        goto redeclipse_update_deploy
    )
:redeclipse_update_bins_blob
    if EXIST "%REDECLIPSE_TEMP%\windows.zip" (
        if "%REDECLIPSE_BINS_CACHED%" == "%REDECLIPSE_BINS_REMOTE%" (
            echo bins: Using cached file "%REDECLIPSE_TEMP%\windows.zip"
            goto redeclipse_update_bins_deploy
        ) else del /f /q "%REDECLIPSE_TEMP%\windows.zip"
    )
    echo bins: %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
    echo.
    %REDECLIPSE_CURL% --output "%REDECLIPSE_TEMP%\windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip"
    if NOT EXIST "%REDECLIPSE_TEMP%\windows.zip" (
        echo bins: Failed to retrieve update package.
        goto redeclipse_update_deploy
    )
:redeclipse_update_bins_deploy
    echo echo bins: deploying blob.>> "%REDECLIPSE_TEMP%\install.bat"
    echo %REDECLIPSE_UNZIP% "%REDECLIPSE_TEMP%\windows.zip" -d "%REDECLIPSE_PATH%" ^&^& ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     ^(echo %REDECLIPSE_BINS_REMOTE%^)^> "%REDECLIPSE_PATH%\bin\version.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo ^) ^|^| ^(>> "%REDECLIPSE_TEMP%\install.bat"
    echo     del /f /q "%REDECLIPSE_TEMP%\bins.txt">> "%REDECLIPSE_TEMP%\install.bat"
    echo     exit 1>> "%REDECLIPSE_TEMP%\install.bat"
    echo ^)>> "%REDECLIPSE_TEMP%\install.bat"
    set REDECLIPSE_DEPLOY=true
:redeclipse_update_deploy
    if NOT "%REDECLIPSE_DEPLOY%" == "true" exit /b 0
    echo.
    echo deploy: %REDECLIPSE_TEMP%\install.bat
    echo.
    set REDECLIPSE_INSTALL=call
    copy /y nul test.tmp> nul 2>&1 && (
        del /f /q test.tmp
        goto redeclipse_update_unpack
    )
    echo Administrator permissions are required to deploy the files.
    if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\elevate.exe" (
        echo Unable to find elevate.exe, are you sure it is in tools?
        goto redeclipse_update_unpack
    )
    set REDECLIPSE_INSTALL="%REDECLIPSE_PATH%\bin\tools\elevate.exe" -wait
:redeclipse_update_unpack
%REDECLIPSE_INSTALL% "%REDECLIPSE_TEMP%\install.bat" && (
    echo.
    (echo %REDECLIPSE_BRANCH%)> "%REDECLIPSE_PATH%\branch.txt"
    exit /b 0
) || (
    echo.
    echo There was an error deploying the files.
    echo.
    exit /b 1
)
