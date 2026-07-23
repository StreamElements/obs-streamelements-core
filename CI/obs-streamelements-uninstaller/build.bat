cd /d %0\..

if "%1"=="full" goto :full
if "%1"=="64" goto :64
if "%1"=="32" goto :32
goto :dev

:dev
    "c:\Program Files (x86)\nsis\makensis" /DSKIP_32BIT_CONTENT /DSKIP_64BIT_CONTENT main.nsi
    goto :EOF

:32
    "c:\Program Files (x86)\nsis\makensis" /DSKIP_64BIT_CONTENT main.nsi
    goto :EOF

:64
    "c:\Program Files (x86)\nsis\makensis" /DSKIP_32BIT_CONTENT main.nsi
    goto :EOF

:full
    "c:\Program Files (x86)\nsis\makensis" main.nsi
    goto :EOF
