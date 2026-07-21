Name `Aero Test`
XPStyle on
OutFile AeroTestClassicUI.exe
RequestExecutionLevel user
InstallDir $PROGRAMFILES\Blah
BrandingText `We Like Aero`

Page Directory
LicenseData ${__FILE__}
Page License
Page InstFiles
Page License

Function .onGUIInit
  Aero::Apply
FunctionEnd

Section
SectionEnd