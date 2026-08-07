#ifndef _LOCKEDLIST_
#define _LOCKEDLIST_

#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#define NSISFUNC(name) extern "C" void __declspec(dllexport) name(HWND hWndParent, int string_size, TCHAR* variables, stack_t** stacktop, extra_parameters* extra)
BOOL g_bInited;
#define DLL_INIT() \
{ \
  if (!g_bInited) \
  { \
    g_hWndParent = hWndParent; \
    EXDLL_INIT(); \
    extra->RegisterPluginCallback(g_hInstance, PluginCallback); \
    g_bInited = TRUE; \
  } \
}

#endif