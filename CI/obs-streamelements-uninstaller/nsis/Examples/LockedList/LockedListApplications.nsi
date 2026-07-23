Name `LockedList Test`
OutFile LockedListTest.exe
RequestExecutionLevel user

BrandingText `Nullsoft Install System v2.46`

!include MUI2.nsh

!define MUI_CUSTOMFUNCTION_GUIINIT onGUIInit
!insertmacro MUI_PAGE_WELCOME
Page Custom LockedListShow
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Function onGUIInit
  Aero::Apply
FunctionEnd

Function LockedListShow
  !insertmacro MUI_HEADER_TEXT `LockedList Test` `Using AddApplications`
  LockedList::AddApplications
  LockedList::Dialog /ignore Ignore
  Pop $R0
FunctionEnd

Section
SectionEnd