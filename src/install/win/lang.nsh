; Red Eclipse
; Windows Installer Translations

; Languages Supported:
!insertmacro MUI_LANGUAGE "English" ;first language is the default language
!insertmacro MUI_LANGUAGE "French"
;!insertmacro MUI_LANGUAGE "German"
;!insertmacro MUI_LANGUAGE "Spanish"
;!insertmacro MUI_LANGUAGE "SpanishInternational"


; Language Strings:
LangString DESC_GameFiles_Title ${LANG_ENGLISH} "Red Eclipse (required)"
  LangString DESC_GameFiles ${LANG_ENGLISH} "The Red Eclipse game files. Required to play the game."
LangString DESC_StartMenu_Title ${LANG_ENGLISH} "Start Menu Shortcuts"
  LangString DESC_StartMenu ${LANG_ENGLISH} "Add shortcuts to your Start Menu"
  LangString DESC_Uninstall ${LANG_ENGLISH} "Remove Red Eclipse"
LangString DESC_RunCheckLabel ${LANG_ENGLISH} "Run Red Eclipse"
LangString DESC_AuthLinkLabel ${LANG_ENGLISH} "Click here to apply for an optional player account."

LangString DESC_GameFiles_Title ${LANG_FRENCH} "red eclipse (Required)"
  LangString DESC_GameFiles ${LANG_FRENCH} "Les fichiers du jeu Red Eclipse. Nécessaire pour jouer le jeu."
LangString DESC_StartMenu_Title ${LANG_FRENCH} "start menu shortcuts"
  LangString DESC_StartMenu ${LANG_FRENCH} "Ajouter des raccourcis à votre Menu Démarrer"
  LangString DESC_Uninstall ${LANG_FRENCH} "remove red eclipse"
LangString DESC_RunCheckLabel ${LANG_FRENCH} "run red eclipse"
LangString DESC_AuthLinkLabel ${LANG_FRENCH} "click here to apply for an optional player account"


; Link strings to locations
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN ; Add the custom strings to sections
  !insertmacro MUI_DESCRIPTION_TEXT ${GameFiles} $(DESC_GameFiles) ; Describes the Red Eclipse (required) section
  !insertmacro MUI_DESCRIPTION_TEXT ${StartMenu} $(DESC_StartMenu) ; Describes the Start Menu Shortcuts section
  !insertmacro MUI_DESCRIPTION_TEXT ${Uninstall} $(DESC_Uninstall) ; Describes the Uninstall section
!insertmacro MUI_FUNCTION_DESCRIPTION_END