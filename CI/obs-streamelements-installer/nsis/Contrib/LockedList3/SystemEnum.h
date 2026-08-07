#ifndef _SYSTEMENUM_
#define _SYSTEMENUM_

#ifndef WINNT
#error You need Windows NT to use this source code. Use /D "WINNT" compiler switch.
#endif

#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef GetWindowLongPtr
#define GetWindowLongPtr GetWindowLong
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef LIST_MODULES_ALL
#define LIST_MODULES_ALL 0x03
#endif

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

#define SYSTEMENUM_ARRAY_SIZE 512
#define SYSTEMENUM_CAPTION_SIZE 1024

struct FILE_INFORMATION
{
  DWORD  ProcessId;
  TCHAR  FullPath[MAX_PATH];
  TCHAR  ProcessDescription[MAX_PATH];
  HWND   ProcessHWND;
  TCHAR  ProcessFullPath[MAX_PATH];
  UINT   FileNumber;
  UINT   TotalFiles;
};

#ifndef _WIN64
struct FILE_INFORMATION64
{
  DWORD  ProcessId;
  WCHAR  FullPath[MAX_PATH];
  WCHAR  ProcessDescription[MAX_PATH];
  HWND__* POINTER_64 ProcessHWND;
  WCHAR  ProcessFullPath[MAX_PATH];
  UINT   FileNumber;
  UINT   TotalFiles;
};
#endif

struct ENUM_OPTIONS
{
  LPARAM lParam;
  BOOL GetWindowCaption;
};

typedef BOOL (CALLBACK*ENUM_FILES)(FILE_INFORMATION&, LPARAM);
typedef BOOL (CALLBACK*ENUM_PROCESS_IDS)(DWORD, LPARAM);
typedef BOOL (CALLBACK*ENUM_APPLICATIONS)(FILE_INFORMATION&, LPARAM);

BOOL WINAPI EnumSystemHandles(ENUM_FILES, ENUM_OPTIONS*);
BOOL WINAPI EnumSystemProcesses(ENUM_FILES, ENUM_OPTIONS*, BOOL);
BOOL WINAPI EnumProcessIds(ENUM_PROCESS_IDS, ENUM_OPTIONS*);
BOOL WINAPI EnumApplications(ENUM_APPLICATIONS, ENUM_OPTIONS*);

ULONG WINAPI GetSystemHandlesCount();
UINT WINAPI GetSystemModulesCount();
UINT WINAPI GetProcessIdsCount();
UINT WINAPI GetApplicationsCount();

BOOL IsSystemProcess(DWORD);
void GetFileFromPath(const PTCHAR, size_t, PTCHAR, size_t);
void FinishEnumeratingNow();
BOOL EndsWith(const PTCHAR, const PTCHAR);
BOOL StartsWith(const PTCHAR, const PTCHAR);
BOOL IsRunningX64();

#endif