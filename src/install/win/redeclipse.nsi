Unicode true ; the authors' names are UTF-8
; --------------------------------
; Name and file, filled in from engine/version.h by dist.mk
  !define APPNAMEFULL "~REPNAME~"
  !define APPAUTHORS "~REPAUTHORS~"
  !define APPPUBLISHER "~REPPUBLISHER~"
  !define APPCOPY "~REPCOPY~"
  !define APPURL "~REPURL~"
  !define APPUNAME "~REPUNAME~" ; the join link scheme, as the game registers it
  Name "${APPNAMEFULL}"
  VIProductVersion "~REPVERSION~.0"
  OutFile "~REPOUTFILE~"
  VIAddVersionKey "OriginalFilename" $OutFile
; --------------------------------
; Installer information
  VIAddVersionKey "ProductName" "${APPNAMEFULL} Installer"
  VIAddVersionKey "FileDescription" "${APPNAMEFULL} Installer"
  VIAddVersionKey "CompanyName" "${APPNAMEFULL}"
  VIAddVersionKey "LegalCopyright" "(C) ${APPCOPY} ${APPAUTHORS}"
  VIAddVersionKey "FileVersion" "~REPVERSION~"
  BrandingText "${APPAUTHORS}" ; Change the branding from the NSIS version to the authors
; --------------------------------
; General
  ; Include Modern UI
  !include "MUI2.nsh"
  !addplugindir "."

  SetCompressor lzma
  SetCompressorDictSize 96
  SetDatablockOptimize on

  SetDateSave off ; Installed files will show the date they were installed instead of when they were created in git

  InstallDir "$PROGRAMFILES\${APPNAMEFULL}" ; Default installation folder
  InstallDirRegKey HKLM "Software\${APPNAMEFULL}" "Install_Dir" ; Get installation folder from registry if available

  ; Request the most privileges user can get
  RequestExecutionLevel admin

  !define MUI_ICON "..\..\redeclipse.ico" ; Use RE icon for installer.
  !define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico" ; Don't use RE icon so uninstaller isn't mistaken for game.
  Icon "..\..\redeclipse.ico"
  UninstallIcon "..\..\redeclipse.ico"

  AutoCloseWindow false ; Do not close the Finish page automatically
  !define MUI_FINISHPAGE_NOREBOOTSUPPORT ; Do not need to reboot to install ${APPNAMEFULL}
  !define MUI_ABORTWARNING ; Warn if aborting installer

  ; All pages
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP "header.bmp"
; --------------------------------
; Pages
  ; Finish Page
    !define MUI_WELCOMEFINISHPAGE_BITMAP "finish.bmp"
    ; Auth application link
; 	!define MUI_FINISHPAGE_LINK "Click here to apply for an optional player account."
;     !define MUI_FINISHPAGE_LINK_LOCATION "https://redeclipse.net/apply"
   ; Run game after install checkbox
    !define MUI_FINISHPAGE_RUN "$INSTDIR\redeclipse.bat"
    !define MUI_FINISHPAGE_RUN_TEXT "Run ${APPNAMEFULL}"

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
; --------------------------------
; Compute Estimated Size for Add/Remove Programs
  !define ARP "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}"
  !include "FileFunc.nsh"
; --------------------------------
; Installer Sections
Section "${APPNAMEFULL} (required)" GameFiles

  SectionIn RO

  SetOutPath $INSTDIR

  File /r /x ".git" /x ".gitattributes" /x ".gitignore" /x ".gitmodules" /x "redeclipse*win*.exe" "..\..\..\*.*"

  WriteRegStr HKLM "SOFTWARE\${APPNAMEFULL}" "Install_Dir" "$INSTDIR"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "DisplayName" "${APPNAMEFULL}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "DisplayVersion" "~REPVERSION~"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "DisplayIcon" "$INSTDIR\src\redeclipse.ico"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "Publisher" "${APPPUBLISHER}"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "InstallSource" "https://${APPURL}/download"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "InstallLocation" "$INSTDIR"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "HelpLink" "https://${APPURL}/docs"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "URLInfoAbout" "https://${APPURL}/"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

  ; Estimated Size for Add/Remove Programs
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${ARP}" "EstimatedSize" "$0"

  ; Prevents issues while updating
  AccessControl::GrantOnFile "$INSTDIR" "(BU)" "FullAccess"
  Pop $0

  ; Join links (redeclipse://host:port), per user so they need no administrator rights; the game registers itself
  ; again whenever it starts, so this only covers clicking a link before it has been run once
  StrCpy $1 "$INSTDIR\bin\x86\redeclipse.exe"
  IfFileExists "$INSTDIR\bin\amd64\redeclipse.exe" 0 +2
    StrCpy $1 "$INSTDIR\bin\amd64\redeclipse.exe"
  WriteRegStr HKCU "Software\Classes\${APPUNAME}" "" "URL:${APPNAMEFULL}"
  WriteRegStr HKCU "Software\Classes\${APPUNAME}" "URL Protocol" ""
  WriteRegStr HKCU "Software\Classes\${APPUNAME}\DefaultIcon" "" '"$1",0'
  WriteRegStr HKCU "Software\Classes\${APPUNAME}\shell\open\command" "" '"$1" "%1"'

SectionEnd

Section "Start Menu Shortcut" StartMenu

  SetOutPath "$INSTDIR"

  CreateShortCut "$INSTDIR\${APPNAMEFULL}.lnk" "$INSTDIR\redeclipse.bat" "" "$INSTDIR\src\redeclipse.ico" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\${APPNAMEFULL}.lnk" "$INSTDIR\redeclipse.bat" "" "$INSTDIR\src\redeclipse.ico" 0 SW_SHOWMINIMIZED

SectionEnd
; --------------------------------
; Uninstaller Section
Section "Uninstall"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAMEFULL}"
  DeleteRegKey HKLM "Software\${APPNAMEFULL}"

  ; Join links are only removed while they still point at this install, as another copy may have taken them since
  ReadRegStr $0 HKCU "Software\Classes\${APPUNAME}\shell\open\command" ""
  StrLen $1 '"$INSTDIR\'
  StrCpy $2 $0 $1
  StrCmp $2 '"$INSTDIR\' 0 +2
    DeleteRegKey HKCU "Software\Classes\${APPUNAME}"

  Delete "$SMPROGRAMS\${APPNAMEFULL}.lnk"
  RMDir /r "$INSTDIR"

SectionEnd
; --------------------------------
; Languages
  !insertmacro MUI_LANGUAGE "English"
  LangString DESC_GameFiles ${LANG_ENGLISH} "The ${APPNAMEFULL} game files. Required to play the game."
  LangString DESC_StartMenu ${LANG_ENGLISH} "Add a shortcut to your Start Menu"

  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${GameFiles} $(DESC_GameFiles)
    !insertmacro MUI_DESCRIPTION_TEXT ${StartMenu} $(DESC_StartMenu)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
