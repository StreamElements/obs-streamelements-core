# Microsoft Developer Studio Project File - Name="myCrasher" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=myCrasher - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "myCrasher.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "myCrasher.mak" CFG="myCrasher - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "myCrasher - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "myCrasher - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "myCrasher - Win32 Release"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release60"
# PROP Intermediate_Dir ".\Release60"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /I "../curl-7.10.7/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MD /W3 /GX /Zi /I "../curl-7.10.7/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /D WINVER=0x400 /Yu"stdafx.h" /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ../lib/BugSplat60.lib /nologo /subsystem:windows /pdb:"..\bin\myCrasher.pdb" /debug /machine:I386 /nodefaultlib:"msvcrt" /out:"..\bin\myCrasher.exe" /pdbtype:sept
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 shlwapi.lib BugSplat.lib /nologo /subsystem:windows /pdb:"..\..\bin\myCrasher60.pdb" /debug /machine:I386 /out:"..\..\bin\myCrasher60.exe" /pdbtype:sept /libpath:"../../lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "myCrasher - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug60"
# PROP Intermediate_Dir ".\Debug60"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /ZI /Od /I "../curl-7.10.7/include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /Yu"stdafx.h" /GZ /c
# ADD CPP /nologo /MDd /W3 /GX /ZI /Od /I "../curl-7.10.7/include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D WINVER=0x400 /FR /Yu"stdafx.h" /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_AFXDLL" /d "_DEBUG"
# ADD RSC /l 0x409 /d "_AFXDLL" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ../lib/BugSplat60D.lib /nologo /subsystem:windows /pdb:"..\bin\myCrasherD.pdb" /debug /machine:I386 /nodefaultlib:"msvcrtd" /out:"..\bin\myCrasherD.exe" /pdbtype:sept
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 shlwapi.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib BugSplatD.lib /nologo /subsystem:windows /pdb:"..\..\bin\myCrasher60D.pdb" /debug /machine:I386 /out:"..\..\bin\myCrasher60D.exe" /pdbtype:sept /libpath:"../../lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "myCrasher - Win32 Release"
# Name "myCrasher - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CmdLine.cpp
DEP_CPP_CMDLI=\
	".\CmdLine.h"\
	".\StdAfx.h"\
	
# End Source File
# Begin Source File

SOURCE=.\myCrasher.cpp
DEP_CPP_MYCRA=\
	".\CmdLine.h"\
	".\myCrasher.h"\
	".\myCrasherDlg.h"\
	".\StdAfx.h"\
	
# End Source File
# Begin Source File

SOURCE=.\myCrasher.rc
# End Source File
# Begin Source File

SOURCE=.\myCrasherDlg.cpp
DEP_CPP_MYCRAS=\
	"..\..\inc\BugSplat.h"\
	".\CmdLine.h"\
	".\myCrasher.h"\
	".\myCrasherDlg.h"\
	".\StdAfx.h"\
	
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
DEP_CPP_STDAF=\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "myCrasher - Win32 Release"

# ADD CPP /nologo /GX /Yc"stdafx.h"

!ELSEIF  "$(CFG)" == "myCrasher - Win32 Debug"

# ADD CPP /nologo /GX /Yc"stdafx.h" /GZ

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\CmdLine.h
# End Source File
# Begin Source File

SOURCE=.\myCrasher.h
# End Source File
# Begin Source File

SOURCE=.\myCrasherDlg.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\myCrasher.ico
# End Source File
# Begin Source File

SOURCE=.\res\myCrasher.rc2
# End Source File
# End Group
# End Target
# End Project
