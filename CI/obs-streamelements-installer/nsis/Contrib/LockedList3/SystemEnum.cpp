/*
  SystemEnum C++ header for enumerating handles, modules, processes and applications
  with callback functions.
  Some code based on SystemInfo header by Zoltan Csizmadia, zoltan_csizmadia@yahoo.com.

  Title:   SystemEnum C++
  Author:  Afrow UK
  Version: 1.8
  Date:    5th August 2014

  * No MFC or CRT dependency.
  * NtQueryObject freezing fixed.
  * Callbacks used rather than CList/CMap collections.
  * TerminateThread used in the case of emergencies, but memory leak avoided.

  Changes:

  v1.8 - 5th August 2014
  * Improved application window caption lookup to find "main" windows (no owner).

  v1.7 - 30th November 2013
  * Fixed GetSystemHandleInformation() failing due to change in the number of handles between NtQuerySystemInformation() calls [special thanks to voidcast].

  v1.6 - 12th July 2012
  * Fixed incorrect buffer size allocation for Unicode builds in GetFileDescription().

  v1.5 - 28th May 2012
  * Fixed EnumApplications not returning FALSE when the callback returned FALSE.
  * Improved GetFileDescription to get first stored file description translation.
  * All enumeration functions now have a ENUM_OPTIONS* argument instead of an LPARAM argument.

  v1.4 - 25th April 2012
  * Fixed StartsWith() matching incorrectly for some strings.

  v1.3 - 11th July 2011
  * Fixed crash on Windows XP 32-bit and below.

  v1.2 - 2nd July 2011
  * Improved support for Windows x64 - now retrieves 64-bit processes but still cannot enumerate 64-bit modules (this is not possible from a 32-bit process).

  v1.1 - 7th February 2011
  * Fixed EnumSystemProcesses on Windows 2000.
  * Added IsSystemProcess function which takes a process id.

  v1.0 - 7th July 2010
  * Process file description now read if process caption does not exist.
  * Renamed EnumSystemModules to EnumSystemProcesses and added fEnumModules parameter.
  * Improved EnumSystemProcesses for retrieval of 64-bit processes.

  v0.9 - 4th June 2010
  * Added StartsWith string matcher.

  v0.8 - 4th April 2010
  * Now allocates SYSTEMENUM_CAPTION_SIZE characters for FILE_INFORMATION.ProcessDescription with GlobalAlloc().

  v0.7 - 29th March 2010
  * Fixed EnumApplications not setting FILE_INFORMATION.TotalFiles.
  * Added GetSystemHandlesCount(), GetSystemModulesCount(), GetProcessIdsCount() and GetApplicationsCount().
  * Improved window caption retrieval for EnumSystemModules().
  * Replaced ZeroMemory again to avoid CRT.
  * Added ResetEvent(g_hFinishNow).
  * Added call EnumProcessModulesEx if it exists.
  * Fixed memory access violation in GetHandleFilePath().

  v0.6 - 11th March 2010
  * Fixed ZeroMemory call: ZeroMemory(File.ProcessDescription, sizeof(File.ProcessFullPath)); -> ZeroMemory(File.ProcessFullPath, sizeof(File.ProcessFullPath));.
  * Fixed GetModuleFileNameExW not being loaded correctly for Unicode builds.

  v0.5 - 11th March 2010
  * Fixed Unicode build crash (my_zeromemory).

  v0.4 - 25th January 2010
  * Re-wrote GetFsFilePath().
  * Fixed GetHandleFilePath() only including services.exe rather than excluding it.
  * Now using GetProcessImageFileName() instead of GetModuleFileNameEx() if it exists (Windows XP and above).
  * Now calls EnumLockedFiles callback in EnumSystemModules() for the process image path as well as its modules.
  * Implemented Unicode.

  v0.3 - 24th July 2009
  * Array sizes changed from 128 to 256 and moved to a SYSTEMENUM_ARRAY_SIZE define in SystemEnum.h.

  v0.2 - 26th September 2007
  * Added EnumApplications.
  * WS_CAPTION is now used rather than WS_SYSMENU to identify application windows.
  * EnumWindowCaptions parameter struct deprecated.
  * Removed GetProcessImageFileName dependency (only available on Win XP).

  v0.1 - 10th July 2007
  * First version.

*/

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0400

#include <windows.h>
#include <tchar.h>
#include "SystemEnum.h"

typedef NTSTATUS (WINAPI* PNtQueryObject)(HANDLE, DWORD, VOID*, DWORD, VOID*);
typedef NTSTATUS (WINAPI* PNtQuerySystemInformation)(DWORD, VOID*, DWORD, ULONG*);
typedef NTSTATUS (WINAPI* PNtQueryInformationFile)(HANDLE, PVOID, PVOID, DWORD, DWORD);
typedef BOOL     (WINAPI* PEnumProcesses)(DWORD*, DWORD, DWORD*);
typedef BOOL     (WINAPI* PEnumProcessModules)(HANDLE, HMODULE*, DWORD, LPDWORD);
typedef BOOL     (WINAPI* PEnumProcessModulesEx)(HANDLE, HMODULE*, DWORD, LPDWORD, DWORD);
typedef DWORD    (WINAPI* PGetModuleFileNameEx)(HANDLE, HMODULE, LPTSTR, DWORD);
typedef DWORD    (WINAPI* PGetProcessImageFileName)(HANDLE, LPTSTR, DWORD);
typedef BOOL     (WINAPI* PQueryFullProcessImageName)(HANDLE, DWORD, LPTSTR, PDWORD);
typedef BOOL     (WINAPI* PWow64DisableWow64FsRedirection)(PVOID*);
typedef BOOL     (WINAPI* PIsWow64Process)(HANDLE, PBOOL);
static PNtQuerySystemInformation        NtQuerySystemInformation;
static PNtQueryObject                   NtQueryObject;
static PNtQueryInformationFile          NtQueryInformationFile;
static PEnumProcesses                   EnumProcesses;
static PEnumProcessModules              EnumProcessModules;
static PEnumProcessModulesEx            EnumProcessModulesEx;
static PGetModuleFileNameEx             GetModuleFileNameEx;
static PGetProcessImageFileName         GetProcessImageFileName;
static PQueryFullProcessImageName       QueryFullProcessImageName;
static PWow64DisableWow64FsRedirection  Wow64DisableWow64FsRedirection;
static PIsWow64Process                  IsWow64Process;

enum FILE_INFORMATION_CLASS
{
  FileNameInformation = 9
};

enum SYSTEM_INFO_CLASS
{
  SystemHandleInformation = 16
};

enum OBJECT_INFORMATION_CLASS
{
  ObjectBasicInformation,
  ObjectNameInformation,
  ObjectTypeInformation,
  ObjectAllInformation,
  ObjectDataInformation
};

struct UNICODE_STRING
{
  WORD  Length;
  WORD  MaximumLength;
  PWSTR Buffer;
};

struct FILE_NAME_INFORMATION
{
  ULONG FileNameLength;
  WCHAR FileName[1];
};

struct SYSTEM_HANDLE
{
  USHORT ProcessId;
  USHORT CreatorBackTraceIndex;
  UCHAR  ObjectTypeIndex;
  UCHAR  HandleAttributes;
  USHORT HandleValue;
  PVOID  Object;
  ULONG  GrantedAccess;
};

struct SYSTEM_HANDLE_INFORMATION
{
  ULONG         Count;
  SYSTEM_HANDLE Handles[1];
};

struct NTFUNC_THREAD_PARAMS
{
  HANDLE Handle;
  PTCHAR Buffer;
  WORD   BufferLength;
  PTCHAR OutBuffer;
  WORD   OutBufferLength;
  int    ObjectInformationClass;
};

struct ENUM_APPLICATIONS_PARAMS
{
  UINT              count;
  UINT              number;
  ENUM_OPTIONS*     opt;
  ENUM_APPLICATIONS lpEnumApplications;
};

HANDLE g_hFinishNow;
BOOL g_fIsWin2000;

// Get the NtDll and PSAPI function pointers.
static BOOL SystemFuncInit()
{
  if (g_hFinishNow == NULL)
    g_hFinishNow = CreateEvent(NULL, FALSE, FALSE, NULL);
  else
    ResetEvent(g_hFinishNow);

  if (
    NtQuerySystemInformation  == NULL ||
    NtQueryObject             == NULL ||
    NtQueryInformationFile    == NULL ||
    EnumProcesses             == NULL ||
    EnumProcessModules        == NULL ||
    GetModuleFileNameEx       == NULL
     )
  {
    HINSTANCE hDll;

    hDll = GetModuleHandle(TEXT("ntdll.dll"));
    NtQuerySystemInformation       = (PNtQuerySystemInformation)      GetProcAddress(hDll, "NtQuerySystemInformation");
    NtQueryObject                  = (PNtQueryObject)                 GetProcAddress(hDll, "NtQueryObject");
    NtQueryInformationFile         = (PNtQueryInformationFile)        GetProcAddress(hDll, "NtQueryInformationFile");

    hDll = GetModuleHandle(TEXT("kernel32.dll"));
#ifdef UNICODE
    QueryFullProcessImageName      = (PQueryFullProcessImageName)     GetProcAddress(hDll, "QueryFullProcessImageNameW");
#else
    QueryFullProcessImageName      = (PQueryFullProcessImageName)     GetProcAddress(hDll, "QueryFullProcessImageNameA");
#endif
    Wow64DisableWow64FsRedirection = (PWow64DisableWow64FsRedirection)GetProcAddress(hDll, "Wow64DisableWow64FsRedirection");
    IsWow64Process                 = (PIsWow64Process)                GetProcAddress(hDll, "IsWow64Process");

    hDll = LoadLibrary(TEXT("psapi.dll"));
    EnumProcesses                  = (PEnumProcesses)                 GetProcAddress(hDll, "EnumProcesses");
    EnumProcessModules             = (PEnumProcessModules)            GetProcAddress(hDll, "EnumProcessModules");
    EnumProcessModulesEx           = (PEnumProcessModulesEx)          GetProcAddress(hDll, "EnumProcessModulesEx");
#ifdef UNICODE
    GetModuleFileNameEx            = (PGetModuleFileNameEx)           GetProcAddress(hDll, "GetModuleFileNameExW");
    GetProcessImageFileName        = (PGetProcessImageFileName)       GetProcAddress(hDll, "GetProcessImageFileNameW");
#else
    GetModuleFileNameEx            = (PGetModuleFileNameEx)           GetProcAddress(hDll, "GetModuleFileNameExA");
    GetProcessImageFileName        = (PGetProcessImageFileName)       GetProcAddress(hDll, "GetProcessImageFileNameA");
#endif

    DWORD dwVersion = GetVersion();
    g_fIsWin2000 = LOBYTE(LOWORD(dwVersion)) == 5 && HIBYTE(LOWORD(dwVersion)) == 0;
  }
  return 
    NtQuerySystemInformation  != NULL &&
    NtQueryObject             != NULL &&
    NtQueryInformationFile    != NULL &&
    EnumProcesses             != NULL &&
    EnumProcessModules        != NULL &&
    GetModuleFileNameEx       != NULL;
}

// Is the given process a system process?
BOOL IsSystemProcess(DWORD dwProcessId)
{
  return dwProcessId == 0 || (g_fIsWin2000 ? dwProcessId == 8 : dwProcessId == 4);
}

// Converts a unicode string buffer to a multi byte string buffer.
static void WideCharToMultiByte(PWSTR wstr, DWORD wstrlen, PTCHAR mbstr, DWORD mbstrlen)
{
#ifdef UNICODE
  lstrcpy(mbstr, wstr);
#else
  WideCharToMultiByte(
    CP_UTF8,
    0,
    wstr,
    wstrlen,
    mbstr,
    mbstrlen,
    NULL,
    NULL);
#endif
}

// Does a string end with a certain string?
BOOL EndsWith(const PTCHAR in, const PTCHAR end)
{
  if (!*in || !*end)
    return FALSE;

  int inlen = lstrlen(in);
  int endlen = lstrlen(end);
  for (; inlen >= 0 && endlen >= 0; --inlen, --endlen)
    if (in[inlen] != end[endlen])
      return FALSE;

  return TRUE;
}

// Does a string start with a certain string.
BOOL StartsWith(const PTCHAR in, const PTCHAR start)
{
  if (!*in || !*start)
    return FALSE;

  for (int i = 0, j = 0; start[j] != NULL; i++, j++)
    if (in[i] != start[j] || in[i] == NULL)
      return FALSE;

  return TRUE;
}

// Gets the file name from a complete path.
void GetFileFromPath(const PTCHAR in, size_t inlen, PTCHAR out, size_t outlen)
{
  size_t i = 0;
  while (in[i] != NULL && i < inlen)
    i++;
  while (in[i] != TEXT('\\') && i > 0)
    i--;
  i++;
  for (int j = 0; j < outlen; j++, i++)
  {
    out[j] = in[i];
    if (in[i] == NULL)
      break;
  }
}

// Compares two strings up to a max number of characters.
static BOOL my_strncmp(PTCHAR s1, PTCHAR s2, size_t count)
{
  for (int i = 0; i < count; i++)
    if (s1[i] != s2[i])
      return FALSE;
  return TRUE;
}

// Convert a device file path into a DOS file path.
static BOOL GetFsFilePath(PTCHAR pszDeviceFilePath, PTCHAR pszFilePath)
{
  PTCHAR pszDevicePath = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*1024);
  TCHAR szDrive[3] = TEXT("A:");

  // Iterate through all drives.
  for (TCHAR chDrive = TEXT('A'); chDrive <= TEXT('Z'); chDrive++)
  {
    szDrive[0] = chDrive;

    // Query the drive letter for the device path.
    if (QueryDosDevice(szDrive, pszDevicePath, sizeof(TCHAR)*1024) != 0)
    {
      int len;

      // Network drive? Format is "\Device\LanmanRedirector\;X:\server\share".
      if (my_strncmp(TEXT("\\Device\\LanmanRedirector\\;"), pszDevicePath, 26) && pszDevicePath[28] == ':')
      {
        PTCHAR pszSharedPath = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*1024);

        len = lstrlen(pszDevicePath);
        int pos = 28;
        for (; pos < len; pos++)
          if (pszDevicePath[pos] == TEXT('\\'))
            break;

        ZeroMemory(pszSharedPath, sizeof(TCHAR)*1024);
        for (int i = 0; pos < len; i++, pos++)
          pszSharedPath[i] = pszDevicePath[pos];

        lstrcpy(pszDevicePath, TEXT("\\Device\\LanmanRedirector"));
        lstrcat(pszDevicePath, pszSharedPath);

        GlobalFree(pszSharedPath);
      }
      
      // Is this the drive letter we are looking for?
      len = lstrlen(pszDevicePath);
      if (my_strncmp(pszDevicePath, pszDeviceFilePath, len))
      {
        lstrcpy(pszDevicePath, (PTCHAR)pszDeviceFilePath + len);
        lstrcpy(pszFilePath, szDrive);
        lstrcat(pszFilePath, pszDevicePath);

        GlobalFree(pszDevicePath);

        return TRUE;
      }
    }
  }

  GlobalFree(pszDevicePath);

  return FALSE;
}

// Open a process for handle duplication.
static HANDLE OpenProcess(DWORD dwProcessId)
{
  return OpenProcess(PROCESS_DUP_HANDLE, TRUE, dwProcessId);
}

// Duplicate a handle.
static HANDLE DuplicateHandle(HANDLE hProcess, HANDLE hRemote)
{
  HANDLE hDup = NULL;
  DuplicateHandle(hProcess, hRemote, GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS);
  return hDup;
}

// Thread to call NtQueryInformationFile with FileNameInformation.
static DWORD WINAPI NtQueryInformationFileThread(void* threadParams)
{
  NTFUNC_THREAD_PARAMS* pParams = (NTFUNC_THREAD_PARAMS*)threadParams;
  DWORD iob[2];

  if (!NT_SUCCESS(NtQueryInformationFile(pParams->Handle, &iob, pParams->Buffer, pParams->BufferLength, FileNameInformation)))
    return FALSE;

  FILE_NAME_INFORMATION* pFileInfo = (FILE_NAME_INFORMATION*)pParams->Buffer;
  WideCharToMultiByte(pFileInfo->FileName, pFileInfo->FileNameLength, pParams->OutBuffer, pParams->OutBufferLength);

  return TRUE;
}

// Thread to call NtQueryObject with whatever.
static DWORD WINAPI NtQueryObjectThread(PVOID threadParams)
{
  NTFUNC_THREAD_PARAMS* pParams = (NTFUNC_THREAD_PARAMS*)threadParams;

  if (!NT_SUCCESS(NtQueryObject(pParams->Handle, pParams->ObjectInformationClass, pParams->Buffer, pParams->BufferLength, NULL)))
    return FALSE;

  UNICODE_STRING* pUnicodeString = (UNICODE_STRING*)pParams->Buffer;
  WideCharToMultiByte(pUnicodeString->Buffer, pUnicodeString->MaximumLength, pParams->OutBuffer, pParams->OutBufferLength);

  return TRUE;
}

// Disable WOW6432 file system redirection.
static void DisableFileSystemRedirection()
{
  PVOID pOldValue = NULL;
  if (Wow64DisableWow64FsRedirection)
    Wow64DisableWow64FsRedirection(&pOldValue);
}

// Gets a full file path from a file handle.
static BOOL GetHandleFilePath(HANDLE h, FILE_INFORMATION* file)
{
  BOOL fSuccess = FALSE;

  // Skip Visual Studio's JIT debugger (Machine Debug Manager, mdm.exe).
  // It starts using 100% CPU when we check one of its handles :S
  // Also skip services.exe.
  if (!EndsWith(file->ProcessFullPath, TEXT("\\mdm.exe")) && !EndsWith(file->ProcessFullPath, TEXT("\\services.exe")))
  {
    BOOL fRemote = file->ProcessId != GetCurrentProcessId();
    HANDLE hRemoteProcess = NULL;
    HANDLE hProcess = NULL;
    if (fRemote)
    {
      hRemoteProcess = OpenProcess(file->ProcessId);
      if (hRemoteProcess != NULL)
        hProcess = DuplicateHandle(hRemoteProcess, h);
    }
    else
      hProcess = h;

    if (hProcess != NULL)
    {
      NTFUNC_THREAD_PARAMS threadParams;
      threadParams.Handle = hProcess;
      threadParams.BufferLength = sizeof(TCHAR)*1024;
      threadParams.Buffer = (PTCHAR)GlobalAlloc(GPTR, threadParams.BufferLength);
      threadParams.OutBufferLength = sizeof(TCHAR)*1024;
      threadParams.OutBuffer = (PTCHAR)GlobalAlloc(GPTR, threadParams.OutBufferLength);

      // What type is it the handle?
      threadParams.ObjectInformationClass = ObjectTypeInformation;
      ZeroMemory(threadParams.Buffer, threadParams.BufferLength);
      ZeroMemory(threadParams.OutBuffer, threadParams.OutBufferLength);
      HANDLE hThread = CreateThread(NULL, 0, NtQueryObjectThread, (void*)&threadParams, 0, NULL);

      // Terminate threads that are frozen.
      if (WaitForSingleObject(hThread, 200) == WAIT_TIMEOUT)
        TerminateThread(hThread, 0);
      CloseHandle(hThread);

      // Only want files and directories!
      if (lstrcmpi(threadParams.OutBuffer, TEXT("File")) == 0 || lstrcmpi(threadParams.OutBuffer, TEXT("Directory")) == 0)
      {
        // First try using NtQueryInformationFile which does not freeze when it doesn't get access
        // but instead returns an empty string as the file name.
        ZeroMemory(threadParams.Buffer, threadParams.BufferLength);
        ZeroMemory(threadParams.OutBuffer, threadParams.OutBufferLength);
        hThread = CreateThread(NULL, 0, NtQueryInformationFileThread, (void*)&threadParams, 0, NULL);

        // Terminate threads that are frozen.
        if (WaitForSingleObject(hThread, 200) == WAIT_TIMEOUT)
          TerminateThread(hThread, 0);
        CloseHandle(hThread);

        // The buffer is empty, do not continue.
        if (*threadParams.OutBuffer)
        {
          // Get the full device path for the file now that we know we have access.
          threadParams.ObjectInformationClass = ObjectNameInformation;
          ZeroMemory(threadParams.Buffer, threadParams.BufferLength);
          ZeroMemory(threadParams.OutBuffer, threadParams.OutBufferLength);
          hThread = CreateThread(NULL, 0, NtQueryObjectThread, (void*)&threadParams, 0, NULL);

          // Terminate threads that are frozen.
          if (WaitForSingleObject(hThread, 200) == WAIT_TIMEOUT)
            TerminateThread(hThread, 0);
          CloseHandle(hThread);

          // Success?
          if (*threadParams.OutBuffer)
          {
            ZeroMemory(file->FullPath, sizeof(file->FullPath));
            if (GetFsFilePath(threadParams.OutBuffer, file->FullPath))
              fSuccess = TRUE;
          }
        }
      }
      
      // Close the duplicated remote handle.
      if (fRemote && hProcess != NULL)
        CloseHandle(hProcess);

      GlobalFree(threadParams.Buffer);
      GlobalFree(threadParams.OutBuffer);
    }

    // Close the remote process handle.
    if (fRemote && hRemoteProcess != NULL)
      CloseHandle(hRemoteProcess);
  }

  return fSuccess;
}

// Get process window captions.
static BOOL CALLBACK EnumWindowCaptionsProc(HWND hWnd, LPARAM lParam)
{
  FILE_INFORMATION* pFile = (FILE_INFORMATION*)lParam;
  DWORD dwProcessId = 0;
  GetWindowThreadProcessId(hWnd, &dwProcessId);
  
  if (dwProcessId == pFile->ProcessId)
  {
    DWORD dwStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
    if ((dwStyle & WS_VISIBLE) && (dwStyle & WS_CAPTION) && GetWindow(hWnd, GW_OWNER) == NULL)
    {
      pFile->ProcessHWND = hWnd;
      GetWindowText(hWnd, pFile->ProcessDescription, MAX_PATH);
      if (*pFile->ProcessDescription)
        return FALSE;
    }
  }

  return TRUE;
}

static SYSTEM_HANDLE_INFORMATION* GetSystemHandleInformation()
{
  DWORD size = 0;
  const DWORD maxSize = 16777216;
  SYSTEM_HANDLE_INFORMATION* pSysHandleInformation;

  if (!SystemFuncInit())
    return NULL;
  
  // Find the size to allocate.
  pSysHandleInformation = (SYSTEM_HANDLE_INFORMATION*)GlobalAlloc(GPTR, sizeof(SYSTEM_HANDLE_INFORMATION));
  if (pSysHandleInformation == NULL)
    return NULL;
  NtQuerySystemInformation(SystemHandleInformation, pSysHandleInformation, sizeof(SYSTEM_HANDLE_INFORMATION), &size);
  GlobalFree(pSysHandleInformation);

  while (size < maxSize)
  {
    // Allocate required memory.
    pSysHandleInformation = (SYSTEM_HANDLE_INFORMATION*)GlobalAlloc(GPTR, size);
    if (pSysHandleInformation == NULL)
      return NULL;

    // Query the objects (system wide).
    NTSTATUS res = NtQuerySystemInformation(SystemHandleInformation, pSysHandleInformation, size, NULL);
    if (NT_SUCCESS(res))
      break;

    GlobalFree(pSysHandleInformation);
    pSysHandleInformation = NULL;

    if (res != STATUS_INFO_LENGTH_MISMATCH)
      break;

    size *= 2;
  }

  return pSysHandleInformation;
}

// Get the process image file name.
static void GetProcessFileName(FILE_INFORMATION* pFile)
{
  ZeroMemory(pFile->ProcessFullPath, sizeof(pFile->ProcessFullPath));

  if (QueryFullProcessImageName != NULL)
  {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pFile->ProcessId);
    if (hProcess != NULL)
    {
      DWORD dwSize = MAX_PATH;
      QueryFullProcessImageName(hProcess, 0, pFile->ProcessFullPath, &dwSize);
      CloseHandle(hProcess);
    }
  }
  else if (GetProcessImageFileName != NULL)
  {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pFile->ProcessId);
    if (hProcess != NULL)
    {
      if (GetProcessImageFileName(hProcess, pFile->ProcessFullPath, MAX_PATH))
        GetFsFilePath(pFile->ProcessFullPath, pFile->ProcessFullPath);
      CloseHandle(hProcess);
    }
  }
  else
  {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pFile->ProcessId);
    if (hProcess != NULL)
    {
      GetModuleFileNameEx(hProcess, NULL, pFile->ProcessFullPath, MAX_PATH);
      CloseHandle(hProcess);
    }
  }
}

// Reads a files' file description.
static BOOL GetFileDescription(const PTCHAR pszFilePath, const PTCHAR pszDescription)
{
  BOOL bSuccess = FALSE;
  DWORD dwHandle = 0;
  DWORD dwSize = GetFileVersionInfoSize(pszFilePath, &dwHandle);

  if (dwSize > 0)
  {
    HANDLE hMem = GlobalAlloc(GPTR, dwSize);

    if (hMem != NULL)
    {
      PTCHAR pszBlock = (PTCHAR)GlobalAlloc(GPTR, 41 * sizeof(TCHAR));

      if (pszBlock)
      {
        if (GetFileVersionInfo(pszFilePath, dwHandle, dwSize, hMem))
        {
          struct LANGANDCODEPAGE
          {
            WORD wLanguage;
            WORD wCodePage;
          } *lpTranslate;

          UINT cbTranslate = 0;
          if (VerQueryValue(hMem, TEXT("\\VarFileInfo\\Translation"), (LPVOID*)&lpTranslate, &cbTranslate))
          {
            wsprintf(pszBlock, TEXT("\\StringFileInfo\\%04x%04x\\FileDescription"), lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);

			      LPVOID lpVersion = NULL;
            UINT uiVersion = 0;
            if (VerQueryValue(hMem, pszBlock, &lpVersion, &uiVersion))
            {
              lstrcpyn(pszDescription, (PTCHAR)lpVersion, MAX_PATH-1);	//-- leave one null char at the end.
              bSuccess = TRUE;
            }
          }
        }

        GlobalFree(pszBlock);
      }

      GlobalFree(hMem);
    }
  }

  return bSuccess;
}

// Get a files' process description.
static void GetProcessDescription(FILE_INFORMATION* pFile, BOOL bGetWindowCaption)
{
  ZeroMemory(pFile->ProcessDescription, MAX_PATH * sizeof(TCHAR));

  // Get process window caption first.
  if (bGetWindowCaption)
  {
    pFile->ProcessHWND = NULL;
    EnumWindows(EnumWindowCaptionsProc, (LPARAM)pFile);
  }

  // If no caption, get file description instead.
  if (!*pFile->ProcessDescription)
    if (!GetFileDescription(pFile->ProcessFullPath, pFile->ProcessDescription))
      wsprintf(pFile->ProcessDescription, TEXT("Process ID: %u"), pFile->ProcessId);
}

ULONG WINAPI GetSystemHandlesCount()
{
  ULONG count;

  SYSTEM_HANDLE_INFORMATION* pSysHandleInformation = GetSystemHandleInformation();
  if (pSysHandleInformation != NULL)
  {
    count = pSysHandleInformation->Count;
    GlobalFree(pSysHandleInformation);
  }
  else
  {
    count = 0;
  }

  return count;
}

BOOL WINAPI EnumSystemHandles(ENUM_FILES lpEnumFiles, ENUM_OPTIONS* pOpt)
{
  FILE_INFORMATION file;
  DWORD dwProcessIdLast = -1;

  SYSTEM_HANDLE_INFORMATION* pSysHandleInformation = GetSystemHandleInformation();
  if (pSysHandleInformation == NULL)
    return TRUE;

  DisableFileSystemRedirection();

  file.TotalFiles = pSysHandleInformation->Count;

  // Iterating through the objects.
  for (DWORD i = 0; i < pSysHandleInformation->Count; i++)
  {
    file.FileNumber = i+1;
    file.ProcessId = pSysHandleInformation->Handles[i].ProcessId;

    // New process Id to get the image file name of.
    if (file.ProcessId != dwProcessIdLast)
    {
      GetProcessFileName(&file);
      dwProcessIdLast = file.ProcessId;
    }

    // Get the file path of the handle.
    if (GetHandleFilePath((HANDLE)pSysHandleInformation->Handles[i].HandleValue, &file))
    {
      GetProcessDescription(&file, pOpt->GetWindowCaption);

      // Call the callback.
      if (!lpEnumFiles(file, pOpt->lParam))
      {
        GlobalFree(pSysHandleInformation);
        return FALSE;
      }
    }

    // Exit right now?
    if (WaitForSingleObject(g_hFinishNow, 0) != WAIT_TIMEOUT)
    {
      GlobalFree(pSysHandleInformation);
      return FALSE;
    }
  }

  GlobalFree(pSysHandleInformation);
  return TRUE;
}

static DWORD* GetProcessIds(DWORD* pdwSize)
{
  (*pdwSize) = SYSTEMENUM_ARRAY_SIZE;

  if (!SystemFuncInit())
    return NULL;

  DWORD* pdwProcessIds = (DWORD*)GlobalAlloc(GPTR, sizeof(DWORD)*(*pdwSize));
  if (EnumProcesses(pdwProcessIds, sizeof(DWORD)*(*pdwSize), pdwSize) == 0)
  {
    GlobalFree(pdwProcessIds);
    return NULL;
  }

  return pdwProcessIds;
}

UINT WINAPI GetSystemModulesCount()
{
  UINT count;

  DWORD size;
  DWORD* pdwProcessIds = GetProcessIds(&size);
  if (pdwProcessIds != NULL)
  {
    count = size / sizeof(DWORD);
    GlobalFree(pdwProcessIds);
  }
  else
  {
    count = 0;
  }

  return count;
}

static bool GetProcessFullPathByPID(DWORD pid, FILE_INFORMATION *pfile)
{
  // Open the process to get the process image file name.
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | (g_fIsWin2000 ? PROCESS_VM_READ : 0), FALSE, pid);
  if (hProcess == NULL)
	return false;

  // Get the process image file name.
  ZeroMemory(pfile->ProcessFullPath, sizeof(pfile->ProcessFullPath));
  if (QueryFullProcessImageName != NULL)
  {
    DWORD dwSize = MAX_PATH;
    QueryFullProcessImageName(hProcess, 0, pfile->ProcessFullPath, &dwSize);
  }
  else if (GetProcessImageFileName != NULL)
  {
    if (GetProcessImageFileName(hProcess, pfile->ProcessFullPath, MAX_PATH) != 0)
      GetFsFilePath(pfile->ProcessFullPath, pfile->ProcessFullPath);
  }
  else
  {
    GetModuleFileNameEx(hProcess, NULL, pfile->ProcessFullPath, MAX_PATH);
  }

  CloseHandle(hProcess);
	return true;
}

static void EnumProcessModulesWithCallback(HANDLE hProcess, FILE_INFORMATION *pfile, ENUM_FILES lpEnumFiles, ENUM_OPTIONS* pOpt, BOOL* pbAbort)
{
  DWORD dwSize = sizeof(HMODULE) * SYSTEMENUM_ARRAY_SIZE;
  HMODULE* phModules = (HMODULE*)GlobalAlloc(GPTR, dwSize);

  // Get a list of process modules.
  if (EnumProcessModulesEx == NULL || !EnumProcessModulesEx(hProcess, phModules, dwSize, &dwSize, LIST_MODULES_ALL))
  {
    dwSize = sizeof(HMODULE) * SYSTEMENUM_ARRAY_SIZE;
    if (!EnumProcessModules(hProcess, phModules, dwSize, &dwSize))
      dwSize = 0;
  }

  const DWORD dwCount = dwSize ? dwSize / sizeof(HMODULE) : 0;
  for (DWORD i = 0; i < dwCount; i++)
  {
    // Get the file path of the module.
    ZeroMemory(pfile->FullPath, sizeof(pfile->FullPath));
    if (GetModuleFileNameEx(hProcess, phModules[i], pfile->FullPath, MAX_PATH) && lstrcmp(pfile->FullPath, pfile->ProcessFullPath) != 0)
    {
      // Call the callback.
      if (!lpEnumFiles(*pfile, pOpt->lParam))
      {
        *pbAbort = TRUE;
        break;
      }
    }

    // Exit right now?
    if (WaitForSingleObject(g_hFinishNow, 0) != WAIT_TIMEOUT)
    {
      *pbAbort = TRUE;
      break;
    }
  }

  GlobalFree(phModules);
}

BOOL WINAPI EnumSystemProcesses(ENUM_FILES lpEnumFiles, ENUM_OPTIONS* pOpt, BOOL fEnumModules)
{
  DWORD size = 0;
  DWORD* pdwProcessIds = GetProcessIds(&size);
  if (pdwProcessIds == NULL)
    return TRUE;

  DisableFileSystemRedirection();

  BOOL bAbort = FALSE;
  for (DWORD i = 0; !bAbort && i < size / sizeof(DWORD); i++)
  {
    FILE_INFORMATION file;

    if (!GetProcessFullPathByPID(pdwProcessIds[i], &file))
      continue;

    // File process information.
    file.ProcessId = pdwProcessIds[i];
    file.FileNumber = i+1;
    file.TotalFiles = size / sizeof(DWORD);
    lstrcpy(file.FullPath, file.ProcessFullPath);
    GetProcessDescription(&file, pOpt->GetWindowCaption);

    // Call the callback.
    if (!lpEnumFiles(file, pOpt->lParam))
    {
      bAbort = TRUE;
      break;
    }

    // Enumerate modules if specified.
    if (fEnumModules)
    {
      // Open the process to get an array of process modules.
      HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pdwProcessIds[i]);
      if (hProcess)
      {
        EnumProcessModulesWithCallback(hProcess, &file, lpEnumFiles, pOpt, &bAbort);
        CloseHandle(hProcess);
      }
    }

    // Exit right now?
    if (WaitForSingleObject(g_hFinishNow, 0) != WAIT_TIMEOUT)
    {
      bAbort = TRUE;
      break;
    }
  }

  GlobalFree(pdwProcessIds);
  return bAbort ? FALSE : TRUE;
}

BOOL WINAPI EnumProcessIds(ENUM_PROCESS_IDS lpEnumProcessIds, ENUM_OPTIONS* pOpt)
{
  DWORD size = 0;
  DWORD* pdwProcessIds = GetProcessIds(&size);
  if (pdwProcessIds != NULL)
  {
    // Loop through the process ids.
    for (DWORD i = 0; i < size / sizeof(DWORD); i++)
    {
      if (!lpEnumProcessIds(pdwProcessIds[i], pOpt->lParam))
      {
        GlobalFree(pdwProcessIds);
        return FALSE;
      }
    }

    GlobalFree(pdwProcessIds);
  }
  return TRUE;
}

static BOOL CALLBACK EnumApplicationsCountProc(HWND hWnd, LPARAM lParam)
{
  DWORD dwStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
  if (dwStyle & WS_VISIBLE)
    (*(UINT*)lParam)++;
  return TRUE;
}

// Get windows.
static BOOL CALLBACK EnumApplicationsProc(HWND hWnd, LPARAM lParam)
{
  ENUM_APPLICATIONS_PARAMS* pParams = (ENUM_APPLICATIONS_PARAMS*)lParam;
  FILE_INFORMATION file;
  DWORD dwProcessId = 0;

  // Ignore hidden windows.
  DWORD dwStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
  if (!(dwStyle & WS_VISIBLE))
    return TRUE;

  file.FileNumber = pParams->number++;

  // Get window caption text and ignore those without any.
  ZeroMemory(file.ProcessDescription, MAX_PATH * sizeof(TCHAR));
  GetWindowText(hWnd, file.ProcessDescription, MAX_PATH);
  if (!*file.ProcessDescription)
    return TRUE;

  // Get the process' id.
  GetWindowThreadProcessId(hWnd, &dwProcessId);

  // Get the process image file path.
  file.ProcessId = dwProcessId;
  GetProcessFileName(&file);

  // Get the file description instead.
  if (!pParams->opt->GetWindowCaption)
    GetFileDescription(file.ProcessFullPath, file.ProcessDescription);

  file.TotalFiles = pParams->count;
  file.ProcessHWND = hWnd;

  return pParams->lpEnumApplications(file, pParams->opt->lParam);
}

UINT WINAPI GetApplicationsCount()
{
  UINT count = 0;
  EnumWindows(EnumApplicationsCountProc, (LPARAM)&count);
  return count;
}

BOOL WINAPI EnumApplications(ENUM_APPLICATIONS lpEnumApplications, ENUM_OPTIONS* pOpt)
{
  ENUM_APPLICATIONS_PARAMS params;
  params.count = 0;
  params.number = 0;

  if (!SystemFuncInit())
    return TRUE;

  DisableFileSystemRedirection();

  EnumWindows(EnumApplicationsCountProc, (LPARAM)&params.count);

  params.opt = pOpt;
  params.lpEnumApplications = lpEnumApplications;

  return EnumWindows(EnumApplicationsProc, (LPARAM)&params);
}

// Finish the enumeration functions ASAP.
void FinishEnumeratingNow()
{
  SetEvent(g_hFinishNow);
}

BOOL IsRunningX64()
{
  BOOL fIsWow64 = FALSE;
  return IsWow64Process && IsWow64Process(GetCurrentProcess(), &fIsWow64) && fIsWow64;
}
