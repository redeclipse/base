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
    if NOT DEFINED REDECLIPSE_SOURCE set REDECLIPSE_SOURCE=https://raw.githubusercontent.com/red-eclipse/deploy/master
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
    set REDECLIPSE_UPDATE=stable
    if NOT "%REDECLIPSE_BRANCH%" == "stable" set REDECLIPSE_UPDATE=master
    set REDECLIPSE_TEMP=%REDECLIPSE_CACHE%\%REDECLIPSE_BRANCH%
:redeclipse_update_branch
    echo branch: %REDECLIPSE_UPDATE%
    echo folder: %REDECLIPSE_PATH%
    echo cached: %REDECLIPSE_TEMP%
    if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\curl.exe" (
        echo Unable to find curl.exe, are you sure it is in tools?
        exit /b 0
    )
    set REDECLIPSE_DOWNLOADER="%REDECLIPSE_PATH%\bin\tools\curl.exe" --connect-timeout 30 -L -k -f -A "redeclipse-%REDECLIPSE_UPDATE%" -o
    if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\miniunz.exe" (
        echo Unable to find miniunz.exe, are you sure it is in tools?
        exit /b 0
    )
    set REDECLIPSE_UNZIP="%REDECLIPSE_PATH%\bin\tools\miniunz.exe" -x -o
    if NOT EXIST "%REDECLIPSE_TEMP%" mkdir "%REDECLIPSE_TEMP%"
    echo @ECHO OFF> "%REDECLIPSE_TEMP%\install.bat"
    echo setlocal enableextensions>> "%REDECLIPSE_TEMP%\install.bat"
    if "%REDECLIPSE_BRANCH%" == "devel" goto redeclipse_update_bins_run
:redeclipse_update_module
    echo modules: Updating..
    %REDECLIPSE_DOWNLOADER% "%REDECLIPSE_TEMP%\mods.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/mods.txt" || del /f /q "%REDECLIPSE_TEMP%\mods.txt"
    if NOT EXIST "%REDECLIPSE_TEMP%\mods.txt" (
        echo modules: Failed to retrieve update information.
        exit /b 1
    )
    set /p REDECLIPSE_MODULE_LIST=< "%REDECLIPSE_TEMP%\mods.txt"
    if "%REDECLIPSE_MODULE_LIST%" == "" (
        echo modules: Failed to get list, aborting..
        exit /b 1
    )
    if EXIST "%REDECLIPSE_TEMP%\data.txt" del /f /q "%REDECLIPSE_TEMP%\data.txt"
    if EXIST "%REDECLIPSE_TEMP%\data.zip" del /f /q "%REDECLIPSE_TEMP%\data.zip"
    set REDECLIPSE_MODULE_PREFETCH=
    for %%a in (%REDECLIPSE_MODULE_LIST%) do (
        del /f /q "%REDECLIPSE_TEMP%\%%a.txt"
        if "%%a" == "base" (
            %REDECLIPSE_DOWNLOADER% "%REDECLIPSE_TEMP%\%%a.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/%%a.txt" || del /f /q "%REDECLIPSE_TEMP%\%%a.txt"
            set REDECLIPSE_MODULE_RUN=%%a
            if NOT "!REDECLIPSE_MODULE_RUN!" == "" (
                call :redeclipse_update_module_run "%REDECLIPSE_UPDATER%"
                if %ERRORLEVEL% NEQ 0 (
                    echo !REDECLIPSE_MODULE_RUN!: There was an error updating the module, aborting...
                    exit /b 1
                ) else if NOT "!REDECLIPSE_DEPLOY!" == "true" (
                    echo !REDECLIPSE_MODULE_RUN!: Not updated, skipping other modules...
                    goto redeclipse_update_bins_run
                )
            )
        ) else if NOT "!REDECLIPSE_MODULE_PREFETCH!" == "" (
            set REDECLIPSE_MODULE_PREFETCH=!REDECLIPSE_MODULE_PREFETCH!,%%a
        ) else (set REDECLIPSE_MODULE_PREFETCH=%%a)
    )
    if "%REDECLIPSE_MODULE_PREFETCH" == "" (
        echo modules: Failed to get version information, aborting..
        exit /b 1
    )
    echo modules: Prefetching versions..
    %REDECLIPSE_DOWNLOADER% "%REDECLIPSE_TEMP%\#1.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/{%REDECLIPSE_MODULE_PREFETCH%}.txt"
    for %%a in (%REDECLIPSE_MODULE_LIST%) do (
        if NOT "%%a" == "base" (
            set REDECLIPSE_MODULE_RUN=%%a
            if NOT "!REDECLIPSE_MODULE_RUN!" == "" (
                call :redeclipse_update_module_run "%REDECLIPSE_UPDATER%"
                if %ERRORLEVEL% NEQ 0 (
                    echo !REDECLIPSE_MODULE_RUN!: There was an error updating the module, aborting...
                    exit /b 1
                )
            )
        )
    )
    goto redeclipse_update_bins_run
:redeclipse_update_module_run
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
:redeclipse_update_module_get
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
    set REDECLIPSE_MODULE_CACHEDVER=0
    if EXIST "%REDECLIPSE_TEMP%/%REDECLIPSE_MODULE_RUN%.cacheversion.txt" (
        set /p REDECLIPSE_MODULE_CACHEDVER=< "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.cacheversion.txt"
    )
    if EXIST "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" (
        if EXIST "%REDECLIPSE_TEMP%/%REDECLIPSE_MODULE_RUN%.cacheversion.txt" (
            if "%REDECLIPSE_MODULE_CACHEDVER%" == "%REDECLIPSE_MODULE_REMOTE%" (
                echo %REDECLIPSE_MODULE_RUN%: Cached version up-to-date.
                goto redeclipse_update_module_blob_deploy
            )
        )
    )
    if EXIST "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" (
        del /f /q "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip"
    )
    echo %REDECLIPSE_MODULE_RUN%: Downloading %REDECLIPSE_GITHUB%/%REDECLIPSE_MODULE_RUN%/zipball/%REDECLIPSE_MODULE_REMOTE%
    %REDECLIPSE_DOWNLOADER% "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" "%REDECLIPSE_GITHUB%/%REDECLIPSE_MODULE_RUN%/zipball/%REDECLIPSE_MODULE_REMOTE%" || del /f /q "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip"
    if NOT EXIST "%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.zip" (
        echo %REDECLIPSE_MODULE_RUN%: Failed to retrieve update package.
        exit /b 1
    )
    echo %REDECLIPSE_MODULE_REMOTE%>"%REDECLIPSE_TEMP%\%REDECLIPSE_MODULE_RUN%.cacheversion.txt"
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
    echo bins: Updating..
    del /f /q "%REDECLIPSE_TEMP%\bins.txt"
    %REDECLIPSE_DOWNLOADER% "%REDECLIPSE_TEMP%\bins.txt" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/bins.txt" || del /f /q "%REDECLIPSE_TEMP%\bins.txt"
    if EXIST "%REDECLIPSE_PATH%\bin\version.txt" set /p REDECLIPSE_BINS=< "%REDECLIPSE_PATH%\bin\version.txt"
    if "%REDECLIPSE_BINS%" == "" set REDECLIPSE_BINS=none
    echo bins: %REDECLIPSE_BINS% is installed.
:redeclipse_update_bins_get
    if NOT EXIST "%REDECLIPSE_TEMP%\bins.txt" (
        echo bins: Failed to retrieve update information.
        exit /b 1
    )
    set /p REDECLIPSE_BINS_REMOTE=< "%REDECLIPSE_TEMP%\bins.txt"
    if "%REDECLIPSE_BINS_REMOTE%" == "" (
        echo bins: Failed to read update information.
        exit /b 1
    )
    echo bins: %REDECLIPSE_BINS_REMOTE% is the current version.
    if NOT "%REDECLIPSE_TRYUPDATE%" == "true" if "%REDECLIPSE_BINS_REMOTE%" == "%REDECLIPSE_BINS%" (
        echo echo bins: already up to date.>> "%REDECLIPSE_TEMP%\install.bat"
        goto redeclipse_update_deploy
    )
:redeclipse_update_bins_blob
    if EXIST "%REDECLIPSE_TEMP%\windows.zip" (
        del /f /q "%REDECLIPSE_TEMP%\windows.zip"
    )
    echo bins: Downloading %REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip
    %REDECLIPSE_DOWNLOADER% "%REDECLIPSE_TEMP%\windows.zip" "%REDECLIPSE_SOURCE%/%REDECLIPSE_UPDATE%/windows.zip" || del /f /q "%REDECLIPSE_TEMP%\windows.zip"
    if NOT EXIST "%REDECLIPSE_TEMP%\windows.zip" (
        echo bins: Failed to retrieve update package.
        exit /b 1
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
    echo deploy: %REDECLIPSE_TEMP%\install.bat
    set REDECLIPSE_INSTALL=call
    copy /y nul test.tmp> nul 2>&1 && (
        del /f /q test.tmp
        goto redeclipse_update_unpack
    )
    echo Administrator permissions are required to deploy the files.
    if NOT EXIST "%REDECLIPSE_PATH%\bin\tools\elevate.exe" (
        echo Unable to find elevate.exe, are you sure it is in tools?
        echo There was an error deploying the files.
        exit /b 1
    )
    set REDECLIPSE_INSTALL="%REDECLIPSE_PATH%\bin\tools\elevate.exe" -wait
:redeclipse_update_unpack
%REDECLIPSE_INSTALL% "%REDECLIPSE_TEMP%\install.bat" && (
    (echo %REDECLIPSE_BRANCH%)> "%REDECLIPSE_PATH%\branch.txt"
    exit /b 0
) || (
    echo There was an error deploying the files.
    exit /b 1
)
