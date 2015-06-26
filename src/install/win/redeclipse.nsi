;--------------------------------
; Name and file
  Name "Red Eclipse"
  VIProductVersion "1.5.2.0"
  OutFile "redeclipse_1.5.2_win.exe"
  VIAddVersionKey "OriginalFilename" $OutFile
;--------------------------------
; Installer version information
  VIAddVersionKey "ProductName" "Red Eclipse Installer"
  VIAddVersionKey "FileDescription" "Red Eclipse Installer"
  VIAddVersionKey "CompanyName" "Red Eclipse Team"
  VIAddVersionKey "LegalCopyright" "2015 Red Eclipse Team"
  VIAddVersionKey "FileVersion" "1.0.0.0"
;--------------------------------
; General
  ;Include Modern UI
  !include "MUI2.nsh"

  SetCompressor lzma
  SetCompressorDictSize 96
  SetDatablockOptimize on

  SetDateSave off ; Installed files will show the date they were installed instead of when they were created in git

  ;Default installation folder
  InstallDir "$PROGRAMFILES\Red Eclipse"

  ;Get installation folder from registry if available
  InstallDirRegKey HKLM "Software\Red Eclipse" "Install_Dir"

  ;Request the most privileges user can get
  RequestExecutionLevel highest

  ;Change the branding from NSIS version to Red Eclipse Team
  BrandingText "Red Eclipse Team"

  !define MUI_ICON "..\..\redeclipse.ico" ; Use RE icon for installer.
  !define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico" ; Don't use RE icon so uninstaller isn't mistaken for game.
  Icon "..\..\redeclipse.ico"
  UninstallIcon "..\..\redeclipse.ico"

  AutoCloseWindow false ; Do not close the Finish page automatically

  !define MUI_FINISHPAGE_NOREBOOTSUPPORT ; Do not need to reboot to install Red Eclipse

  ; Warn if aborting installer
  !define MUI_ABORTWARNING

  ; All pages
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP "header.bmp"

;--------------------------------
;Pages

  ; Finish Page
   !define MUI_WELCOMEFINISHPAGE_BITMAP "finish.bmp"
	; Auth application link
;	!define MUI_FINISHPAGE_LINK "Click here to apply for an optional player account."
;     !define MUI_FINISHPAGE_LINK_LOCATION "http://redeclipse.net/apply"
   ; Run game after install checkbox
  !define MUI_FINISHPAGE_RUN "$INSTDIR\redeclipse.bat"
  !define MUI_FINISHPAGE_RUN_TEXT "Run Red Eclipse"

  !define MUI_COMPONENTSPAGE_SMALLDESC
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
    !define MUI_FINISHPAGE_NOAUTOCLOSE ; Allow user to review install log before continuing to Finish page.

  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
    !define MUI_UNFINISHPAGE_NOAUTOCLOSE ; Allow user to review UNinstall log before continuing to Finish page.
  !insertmacro MUI_UNPAGE_FINISH
  
;--------------------------------
;Compute Estimated Size for Add/Remove Programs
  !define ARP "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse"
  !include "FileFunc.nsh"

;--------------------------------
;Installer Sections

Section "Red Eclipse (required)" GameFiles

  SectionIn RO
  
  SetOutPath $INSTDIR
  
  File /r /x "redeclipse.app" /x "readme.md" /x ".git" /x ".gitattributes" /x ".gitignore" /x ".gitmodules" /x "redeclipse*win*.exe" "..\..\..\*.*"
  
  WriteRegStr HKLM "SOFTWARE\Red Eclipse" "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "DisplayName" "Red Eclipse"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "DisplayVersion" "1.5.x"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "DisplayIcon" "$INSTDIR\src\redeclipse.ico"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "Publisher" "Red Eclipse Team"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "InstallSource" "http://redeclipse.net/download"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "InstallLocation" "$INSTDIR"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "HelpLink" "http://redeclipse.net/wiki"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "URLInfoAbout" "http://redeclipse.net/"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

  ; Estimated Size for Add/Remove Programs
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${ARP}" "EstimatedSize" "$0"

SectionEnd

Section "Start Menu Shortcuts" StartMenu

  CreateDirectory "$SMPROGRAMS\Red Eclipse"
  
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$INSTDIR\Red Eclipse.lnk"                "$INSTDIR\redeclipse.bat" "" "$INSTDIR\src\redeclipse.ico" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Red Eclipse\Red Eclipse.lnk" "$INSTDIR\redeclipse.bat" "" "$INSTDIR\src\redeclipse.ico" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Red Eclipse\Uninstall Red Eclipse.lnk"   "$INSTDIR\uninstall.exe"   "" "$INSTDIR\uninstall.exe" 0
  
SectionEnd
 
;--------------------------------
;Uninstaller Section
Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse"
  DeleteRegKey HKLM "Software\Red Eclipse"

  RMDir /r "$SMPROGRAMS\Red Eclipse"
  RMDir /r "$INSTDIR"

SectionEnd

;--------------------------------
; Languages
 
  !insertmacro MUI_LANGUAGE "English"
  LangString DESC_GameFiles ${LANG_ENGLISH} "The Red Eclipse game files. Required to play the game."
  LangString DESC_StartMenu ${LANG_ENGLISH} "Add shortcuts to your Start Menu"

  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${GameFiles} $(DESC_GameFiles)
    !insertmacro MUI_DESCRIPTION_TEXT ${StartMenu} $(DESC_StartMenu)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
