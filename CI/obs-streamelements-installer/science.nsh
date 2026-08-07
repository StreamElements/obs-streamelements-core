Var /GLOBAL g_scienceStartSeconds
Var /GLOBAL g_scienceLastEventStartSeconds
Var /GLOBAL g_scienceSessionId
Var /GLOBAL g_scienceHostMachineUniqueId

Function scienceInit
    SetPluginUnload alwaysoff
    NSISConfig::GenerateGloballyUniqueIdentifier
    Pop $g_scienceSessionId

    SetPluginUnload alwaysoff
    Push "4"
    NSISHTTP::HttpSetAsyncRequestsConcurrency

    SetPluginUnload alwaysoff
    NSISHTTP::GetSecondsSinceEpochStart
    Pop $g_scienceStartSeconds
    StrCpy $g_scienceLastEventStartSeconds "$g_scienceStartSeconds"
    Pop $g_scienceLastEventStartSeconds

    SetPluginUnload alwaysoff
    NSISConfig::GetComputerSystemUniqueIdentifier
    Pop $g_scienceHostMachineUniqueId
FunctionEnd

Function scienceGetDuration
    SetPluginUnload alwaysoff
    NSISHTTP::GetSecondsSinceEpochStart
    Pop $R2
    
    IntOp $R0 $R2 - $g_scienceStartSeconds
    IntOp $R1 $R2 - $g_scienceLastEventStartSeconds

    StrCpy $g_scienceLastEventStartSeconds "$R2"

    Push $R0
    Push $R1
FunctionEnd

Function scienceFlush
    SetPluginUnload alwaysoff
    NSISHTTP::HttpFlushAllAsyncRequests
FunctionEnd
