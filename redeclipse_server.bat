@ECHO OFF

set APP_DIR=.
set APP_OPTIONS=
set APP_ARCH=x86

IF /I "%PROCESSOR_ARCHITECTURE%" == "amd64" (
    set APP_ARCH=amd64
)
IF /I "%PROCESSOR_ARCHITEW6432%" == "amd64" (
    set APP_ARCH=amd64
)

:RETRY
IF EXIST bin\%APP_ARCH%\redeclipse_server.exe (
    start bin\%APP_ARCH%\redeclipse_server.exe %APP_OPTIONS% %* 
) ELSE (
    IF EXIST %APP_DIR%\bin\%APP_ARCH%\redeclipse_server.exe (
        pushd %APP_DIR%
        start bin\%APP_ARCH%\redeclipse_server.exe %APP_OPTIONS% %*
        popd
    ) ELSE (
        IF %APP_ARCH% == amd64 (
            set APP_ARCH=x86
            goto RETRY
        )
        echo Unable to find the Red Eclipse server binary
        pause
    )
)
