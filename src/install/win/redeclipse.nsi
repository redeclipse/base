;--------------------------------
;General
  ;Include Modern UI
  !include "MUI2.nsh"

  ;Name and file
  Name "Red Eclipse"
  OutFile "redeclipse_1.5_win.exe"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\Red Eclipse"

  ;Get installation folder from registry if available
  InstallDirRegKey HKLM "Software\Red Eclipse" "Install_Dir"

  ;Request application privileges for Windows Vista
  RequestExecutionLevel user

  ;Change the branding from NSIS version to Red Eclipse Team
  BrandingText "Red Eclipse Team"

  !define MUI_ICON "..\..\redeclipse.ico"
  Icon "..\..\redeclipse.ico"
  UninstallIcon "..\..\redeclipse.ico"
  
  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_LICENSE "..\..\..\doc\license.txt"
  !insertmacro MUI_PAGE_INSTFILES
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections
Section "Red Eclipse (required)"

  SectionIn RO
  
  SetOutPath $INSTDIR
  
  File /r /x "readme.md" /x ".git" /x ".gitattributes" /x ".gitignore" /x ".gitmodules" "..\..\..\*.*" 
  
  WriteRegStr HKLM "SOFTWARE\Red Eclipse" "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "DisplayName" "Red Eclipse"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "DisplayVersion" "1.5"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "Publisher" "Red Eclipse Team"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "HelpLink" "http://redeclipse.net/wiki"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "URLInfoAbout" "http://redeclipse.net/"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Red Eclipse"
  
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$INSTDIR\Red Eclipse.lnk"                "$INSTDIR\redeclipse.bat" "" "$INSTDIR\src\redeclipse.ico" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Red Eclipse\Red Eclipse.lnk" "$INSTDIR\redeclipse.bat" "" "$INSTDIR\src\redeclipse.ico" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Red Eclipse\Uninstall.lnk"   "$INSTDIR\uninstall.exe"   "" "$INSTDIR\uninstall.exe" 0
  
SectionEnd
 
;--------------------------------
;Uninstaller Section
Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse"
  DeleteRegKey HKLM SOFTWARE\RedEclipse

  RMDir /r "$SMPROGRAMS\Red Eclipse"
  RMDir /r "$INSTDIR"

SectionEnd