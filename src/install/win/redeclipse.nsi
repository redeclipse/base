Name "RedEclipse"

OutFile "redeclipse_1.4_win.exe"

InstallDir "$PROGRAMFILES\Red Eclipse"

InstallDirRegKey HKLM "Software\Red Eclipse" "Install_Dir"

SetCompressor /SOLID lzma
XPStyle on

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Red Eclipse (required)"

  SectionIn RO
  
  SetOutPath $INSTDIR
  
  File /r "..\..\..\*.*"
  
  WriteRegStr HKLM "SOFTWARE\Red Eclipse" "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "DisplayName" "Red Eclipse 1.4"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Red Eclipse"
  
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$INSTDIR\Red Eclipse.lnk"                "$INSTDIR\redeclipse.bat" "" "$INSTDIR\bin\x86\redeclipse.exe" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Red Eclipse\Red Eclipse.lnk" "$INSTDIR\redeclipse.bat" "" "$INSTDIR\bin\x86\redeclipse.exe" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Red Eclipse\Uninstall.lnk"   "$INSTDIR\uninstall.exe"   "" "$INSTDIR\uninstall.exe" 0
  
SectionEnd

Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Red Eclipse"
  DeleteRegKey HKLM SOFTWARE\RedEclipse

  RMDir /r "$SMPROGRAMS\Red Eclipse"
  RMDir /r "$INSTDIR"

SectionEnd
