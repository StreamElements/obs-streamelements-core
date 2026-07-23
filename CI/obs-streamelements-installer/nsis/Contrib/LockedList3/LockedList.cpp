/* LockedList NSIS plug-in

  Title:   LockedList plug-in
  Author:  Afrow UK
  Version: 3.0.0.4
  Date:    19th April 2015

  Description:

   Plug-in that displays a list of open programs that are locking files
   that need to be modified/deleted etc by the installer.

  Changes:

  3.0.0.4 - 19th April 2014
  * ANSI build did not convert Unicode characters from LockedList64.dll
    to ANSI.

  3.0.0.3 - 7th August 2014
  * FindProcess did not always push "no" (/yesno) or an empty string
    onto the stack when no processes were running.

  3.0.0.2 - 5th August 2014
  * Added CloseProcess function.
  * Improved application window caption lookup to find "main" windows
    (no owner).

  3.0.0.1 - 7th December 2013
  * Added 64-bit modules counting via LockedList64 for the progress bar
    and silent search.

  3.0.0.0 - 1st December 2013
  * Fixed GetSystemHandleInformation() failing due to change in the
    number of handles between NtQuerySystemInformation() calls [special
    thanks to voidcast].
  * 64-bit module support via LockedList64.dll [special thanks to Ilya
    Kotelnikov].

  2.6.1.4 - 12th July 2012
  * Fixed a crash in SystemEnum (v1.6) for the Unicode build.

  2.6.1.3 - 12th July 2012
  * Fixed Back button triggering auto-next during scan when /autonext is
    used.

  2.6.1.2 - 1st July 2012
  * Kills processes with no windows when using autoclose with
    SilentSearch.

  2.6.1.1 - 3rd May 2012
  * Added autoclose code to SilentSearch.
  * Fixed some bugs in SystemEnum (v1.5).

  2.6.1.0 - 25th April 2012
  * Fixed StartsWith() matching incorrectly for some strings.

  2.6.0.2 - 22nd April 2012
  * Fixed window message loop halting page leave until mouse move on
    Windows 2000.

  2.6.0.1 - 23rd March 2012
  * Fixed clipboard list copy for the Unicode build.
  * Fixed crashes and infinite looping after repeatedly going back to
    the LockedList page.

  v2.6 - 9th January 2012
  * Added missing calls to EnableDebugPriv() in FindProcess and
    EnumProcesses.

  v2.5 - 11th July 2011
  * Fixed crash on Windows XP 32-bit and below.

  v2.4 - 2nd July 2011
  * Improved support for Windows x64 - now retrieves 64-bit processes
    but still cannot enumerate 64-bit modules (this is not possible from
    a 32-bit process).
  * Fixed infinite loop which sometimes occurred on Cancel button click.

  v2.3 - 7th February 2011
  * Added /ignorebtn [button_id] switch to specify a new Ignore button.
    This button can be added to the UI using Resource Hacker
    (recommended) or at run time using the System plug-in.
  * /autonext now also applies when all open programs have been closed
    while the dialog is visible.
  * Fixed EnumSystemProcesses on Windows 2000.
  * Fixed System being listed on Windows 2000.

  v2.2 - 19th October 2010
  * Fixed AddCustom not adding items.
  * No longer returns processes with no file path.

  v2.1 - 24th August 2010
  * Added /autonext to automatically go to the next page when no items
    are found.

  v2.0 - 23rd August 2010
  * Fixed IsFileLocked() returning true for missing directories (thanks
    ukreator).
  * Replaced "afxres.h" include with <Windows.h> in LockedList.rc.

  v1.9 - 23rd July 2010
  * Now using ExtractIconEx instead of ExtractIcon for all icons (thanks
  jiake).

  v1.8 - 17th July 2010
  * Fixed programs not being closable.
  * RC2: Removed debug message box.

  v1.7 - 10th July 2010
  * Process file description now retreived by SystemEnum if no process
    caption found.
  * Added EnumProcesses plug-in function.
  * SilentSearch now uses a callback function instead of the stack.
  * SilentSearch /thread changed to /async.
  * Previously added processes now stored in an array for look up to
    prevent repetitions rather than looked up in the list view control.
  * Added FindProcess plug-in function.
  * Now gets 64-bit processes (but not modules).
  * RC2: Added version information resource.
  * RC3: Added /yesno switch to FindProcess plug-in function.
  * RC4: Fixed FindProcess plug-in function case sensitivity (now case
    insensitive).

  v1.6 - 4th June 2010
  * Fixed processes getting repeated in the list.
  * Fixed list not auto scrolling to absolute bottom.
  * Next button text restored when using /ignore and no processes are
    found.
  * Added AddFolder plug-in function.
  * File description displayed for processes without a window caption.
  * Process Id displayed for processes without a window caption or file
    description.

  v1.5 - 28th April 2010
  * Fixed IsFileLocked plug-in function.
  * Fixed /noprograms plug-in switch.

  v1.4 - 22nd April 2010
  * Removed DLL manifest to fix Microsoft VC90 CRT dependency.
  * Now using ANSI pluginapi.lib for non Unicode build.
  * Switched from my_atoi() to pluginapi myatoi().

  v1.3 - 4th April 2010
  * Increased FILE_INFORMATION.ProcessDescription to 1024 characters to
    fix buffer overflow crash.
  * Fixed IsFileLocked() failing if first plug-in call (EXDLL_INIT()
    missing).

  v1.2 - 2nd April 2010
  * Added 'ignore' dialog result if /ignore was used and there were
    programs running.
  * Added additional argument for /autoclose and /autoclosesilent to set
    Next button text
  * /ignore no longer used to specify Next button text for /autoclose
    and /autoclosesilent.
  * Added IsFileLocked NSIS function.
  * Fixed possible memory leaks if plug-in arguments were passed
    multiple times.

  v1.1 - 31st March 2010
  * Reverted back to using my_atoi() (Unicode NSIS myatoi() has a bug).
  * Added AddCustom plug-in function.
  * Fixed possible memory access violation in AddItem().
  * Improved Copy List context menu item code.
  * Fixed Copy List not showing correct process id's.
  * Fixed memory leak from not freeing allocated memory for list view
    item paramaters.
  * RC2: Fixed AddCustom not working (non debug builds).

  v1.0 - 30th March 2010
  * Fixed CRT dependency.
  * Improved percent complete calculations.
  * Now pushes /next to stack in between stack items.
  * Fixed memory leak in AddItem().
  * Fixed crashes caused by using AddFile plug-in function.
  * General code cleanup.
  * RC2: Excluded process id's #0 and #4 from searches (System Idle
    Process and System).
  * RC3: Fixed 6 possible memory access violations.
  * RC3: Removed debug MessageBox.
  * RC3: Unicode plug-in build name changed to LockedList.dll.
  * RC4: Removed unused includes.
  * RC5: Fixed memory access violation when using SilentSearch.

  v0.9 - 11th March 2010
  * Fixed memory access violation in g_apszParams.
  * Various fixes and changes in SystemEnum (see SystemEnum.cpp).
  * Added /menuitems "close_text" "copy_list_text".
  * Implemented new NSIS plugin API (/NOUNLOAD no longer necessary).
  * Now includes current process in search when using SilentSearch.
  * Implemented Unicode build.
  * RC2: Fixed crash if no search criteria was provided (division by
    zero).
  * RC3: Fixed Unicode build crash (my_zeromemory) (and SystemEnum
    v0.5).
  * RC4: Fixed garbage process appearing (SystemEnum v0.6).
  * RC4: Fixed Unicode build not returning correct processes (SystemEnum
    v0.6).

  v0.8 - 24th July 2009
  * Increased array sizes for processes and process modules from 128 to
    256.

  v0.7 - 26th February 2008
  * Re-wrote /autoclose code and fixed crashing.
  * Added AddClass and AddCaption functions.
  * Fixed Copy List memory read access error.
  * Made thread exiting faster for page leave.
  * Progress bar and % work better.
  * Processing mouse cursor redrawn.
  * Ignore button text only set when list is not empty.
  * RC2: Fixed /autoclose arguments.

  v0.6 - 12th February 2008
  * Added /autoclose "close_text" "kill_text" "failed_text" and
    /autoclosesilent "failed_text".
    The /ignore switch can be used along with this to set the Next
    button text.
  * Added /colheadings "application_text" "process_text"

  v0.5 - 25th November 2007
  * Fixed memory leak causing crash when re-visiting dialog. Caused
    by duplicate call to GlobalFree on the same pointer.

  v0.4 - 27th September 2007
  * Module or file names can now be just the file name as opposed to
    the full path.
  * Folder paths are converted to full paths (some are short DOS
    paths) before comparison.
  * Fixed typo in AddModule function (g_uiModulesCount>g_uiFilesCount).
    Thanks kalverson.
  * List view is now scrolled into view while items are added.
  * List changed to multiple columns.
  * Debug privileges were not being set under SilentSearch.
  * Added /ignore switch that prevents the Next button being
    disabled.
  * Added AddApplications to add all running applications to the
    list.
  * Added processing mouse cursor.
  * Added right-click context menu with Close and Copy List options.
  * Added progress bar.
  * Added default program icon for processes without an icon.
  * Added code to resize controls for different dialog sizes.

  v0.3 - 13th July 2007
  * Added LVS_EX_LABELTIP style to list view control for long item
    texts.
  * Width of list header changed from width-6 to
    width-GetSystemMetrics(SM_CXHSCROLL).
  * Added WM_SYSMENU existence check when obtaining window captions.
  * Files/modules lists memory is now freed when using SilentSearch.
  * Files and Modules lists count now reset after a search.
  * Added reference to Unload function to read-me.

  v0.2 - 12th July 2007
  * Added two new examples.
  * Fixed pointer error in FileListInfo struct causing only first
    module/file added to be used.
  * Fixed caption repetition over multiple processes.
  * Fixed stack overflow in DlgProc. Special thanks, Roman Prysiazhniuk
    for locating the source.
  * Better percent complete indication.

  v0.1 - 10th July 2007
  * First version.

*/

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <shlwapi.h>
#include "resource.h"
#include "pluginapi.h"
#include "LockedList.h"
#include "SystemEnum.h"

//#pragma comment(lib, "shlwapi.lib")

HINSTANCE   g_hInstance;
BOOL        g_fDebugPrivEnabled = FALSE;

// Ensures the NSIS installer has debug privileges.
static BOOL EnableDebugPriv()
{
  if (!g_fDebugPrivEnabled)
  {
	  HANDLE hToken;
	  LUID sedebugnameValue;
	  TOKEN_PRIVILEGES tkp;

	  // Enable the SeDebugPrivilege
	  if (OpenProcessToken(GetCurrentProcess(),
		  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
	    if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue))
      {
	      tkp.PrivilegeCount = 1;
	      tkp.Privileges[0].Luid = sedebugnameValue;
	      tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	      if (AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL))
          g_fDebugPrivEnabled = TRUE;
      }
    }
  		
	  CloseHandle(hToken);
  }

  return g_fDebugPrivEnabled;
}

enum EnumCallback : UINT
{
  EnumCallback_EnumLockedFilesProc = 0
};

struct ENUM64_OPTIONS
{
  EnumCallback Callback;
  ENUM_OPTIONS* Options;
};

#ifndef _WIN64

#define TEXT_HEADING         TEXT("Please close the following programs before continuing with setup...")
#define TEXT_NOPROGRAMS      TEXT("No programs that have to be closed are running")
#define TEXT_SEARCHING       TEXT("Searching, please wait...")
#define TEXT_SEARCHING_0     TEXT("0% ") ## TEXT_SEARCHING
#define TEXT_ENDSEARCH       TEXT("Cancelling search, please wait...")
#define TEXT_ENDMONITOR      TEXT("Ending program monitoring, please wait...")
#define TEXT_MSGBOX_CLOSE    TEXT("To continue with setup, the listed processes must be closed.\r\nWould you like to close them now?")
#define TEXT_MSGBOX_KILL     TEXT("Some processes could not be closed safely.\r\nWould you like to kill them?\r\n\r\nWARNING: Any unsaved data will be lost!")
#define TEXT_MSGBOX_ACFAILED TEXT("Not all processes appear to be closed.\r\nPlease close them manually to proceed with setup.")
#define TEXT_COLHEADING_1    TEXT("Application")
#define TEXT_COLHEADING_2    TEXT("Process")
#define TEXT_MENUITEM_1      TEXT("Close")
#define TEXT_MENUITEM_2      TEXT("Copy List")

#define ICON_INFO   TEXT("info.ico")
#define ICON_SEARCH TEXT("search.ico")

#define OUT_OK          TEXT("ok")
#define OUT_ERROR       TEXT("error")
#define OUT_NEXT        TEXT("next")
#define OUT_IGNORE      TEXT("ignore")
#define OUT_BACK        TEXT("back")
#define OUT_CANCEL      TEXT("cancel")
#define OUT_DONE        TEXT("done")
#define OUT_WAIT        TEXT("wait")
#define OUT_YES         TEXT("yes")
#define OUT_NO          TEXT("no")

struct CUSTOM_ITEM
{
  INT    iFunctionAddress;
  HICON  hIcon;
  PTCHAR pszApplication;
  PTCHAR pszProcess;
};

enum OUT_WHERE
{
  OutListBox,
  OutCallback
};

enum LIST_TYPE
{
  ListTypeFiles,
  ListTypeModules,
  ListTypeClasses,
  ListTypeCaptions,
  ListTypeApplications,
  ListTypeCustom
};

struct OUT_WHERE_INFO
{
  OUT_WHERE outWhere;
  int iFunctionAddress;
};

struct LIST_INFO
{
  LONG_PTR       lpList;
  UINT           uiListCount;
  LIST_TYPE      listType;
  OUT_WHERE_INFO outWhereInfo;
  ULONG          ulTotalCurrent;
  ULONG          ulTotalCount;
  DWORD          dwCurrentProcessId;
};

enum LISTITEM_TYPE
{
  ListItemProcess,
  ListItemCustom
};

struct LISTITEM_INFO
{
  LONG_PTR      lpData;
  LISTITEM_TYPE listItemType;
};

struct LISTITEM_INFO_PROCESS
{
  HWND  hWnd;
  DWORD dwProcessId;
};

struct LISTITEM_INFO_CUSTOM
{
  CUSTOM_ITEM* pCustomItem;
};

struct FIND_PROCESS_PROC_ARGS
{
  PTCHAR pszProcessExe;
  BOOL fYesNo;
  BOOL fSuccess;
};

HWND        g_hWndParent;
extra_parameters* g_pExtraParameters = NULL;

HWND        g_hDialog;
HWND        g_hList;
HWND        g_hNext;
HWND        g_hBack;
HWND        g_hCancel;
HWND        g_hProgress;
DLGPROC     ParentDlgProcOld;
WNDPROC     ListWndProcOld;
BOOL        g_fDone = FALSE, g_fMonitoringDone = FALSE;
BOOL        g_fCancelClicked = FALSE;
BOOL        g_fBackClicked = FALSE;
BOOL        g_fIgnoreClicked = FALSE;
HCURSOR     g_hProcessingCursor;
PTCHAR      g_pszNext = NULL;
UINT        g_uiIgnoreBtn = 0;

BOOL        g_fAutoClose = FALSE;
BOOL        g_fAutoClosing = FALSE;
BOOL        g_fShowRunningApplications = FALSE;
BOOL        g_fUserIcons = FALSE;
BOOL        g_fAutoNext = FALSE;
UINT        g_uiRunning = 0;

#define MAX_LIST_COUNT 128
PTCHAR      g_apszFilesList[MAX_LIST_COUNT];
PTCHAR      g_apszModulesList[MAX_LIST_COUNT];
PTCHAR      g_apszClassesList[MAX_LIST_COUNT];
PTCHAR      g_apszCaptionsList[MAX_LIST_COUNT];
CUSTOM_ITEM g_aCustomItemsList[MAX_LIST_COUNT];
PTCHAR      g_apszFoldersList[MAX_LIST_COUNT];
UINT        g_uiFilesCount = 0;
UINT        g_uiModulesCount = 0;
UINT        g_uiClassesCount = 0;
UINT        g_uiCaptionsCount = 0;
UINT        g_uiCustomItemsCount = 0;
UINT        g_uiFoldersCount = 0;

#define MAX_PROCESSES 256
DWORD       g_adwProcessIds[MAX_PROCESSES];
PTCHAR      g_apszProcesses[MAX_PROCESSES];
UINT        g_uiProcessCount = 0;

UINT        g_uiPercentComplete = 0;
HANDLE      g_hThreadFiles = NULL;
HANDLE      g_hThreadAutoClose = NULL;

#define PARAM_HEADING 0
#define PARAM_NOPROGRAMS 1
#define PARAM_SEARCHING 2
#define PARAM_ENDSEARCH 3
#define PARAM_ENDMONITOR 4
#define PARAM_IGNORE 5
#define PARAM_AUTOCLOSE_FAILED 6
#define PARAM_AUTOCLOSE_CLOSE 7
#define PARAM_AUTOCLOSE_KILL 8
#define PARAM_COLHEADING_APPLICATION 9
#define PARAM_COLHEADING_PROCESS 10
#define PARAM_MENUITEM1 11
#define PARAM_MENUITEM2 12
#define MAX_PARAMS 13
PTCHAR  g_apszParams[MAX_PARAMS];

// Ilya Kotelnikov> few forward declarations
static BOOL CallLockedList64(PTCHAR pszFunction, PTCHAR pszArguments);
static BOOL EnumSystemProcesses64(EnumCallback, ENUM_OPTIONS*, BOOL);
static UINT GetSystemModulesCount64();

// Ilya Kotelnikov> global vars to handle x64 helper process.
WNDPROC CallbackWindow64ProcOld = NULL;
LRESULT CALLBACK CallbackWindow64WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
ENUM64_OPTIONS* g_pEnumSystemProcesses64Options = NULL;
UINT g_uGetSystemModulesCount64 = 0;

static LPARAM GetListItemInfo(LPARAM lParam, LISTITEM_TYPE listItemType)
{
  if (lParam == NULL)
    return NULL;

  LISTITEM_INFO* pListItemInfo = (LISTITEM_INFO*)lParam;
  if (pListItemInfo->listItemType != listItemType)
    return NULL;

  return pListItemInfo->lpData;
}

#define GetListItemInfoProcess(lParam) ((LISTITEM_INFO_PROCESS*)GetListItemInfo(lParam, ListItemProcess))
#define GetListItemInfoCustom(lParam) ((LISTITEM_INFO_CUSTOM*)GetListItemInfo(lParam, ListItemCustom))

// Function: Search using wildcards.
static int wildcmp(const TCHAR* wild, const TCHAR* string)
{
  // Written by Jack Handy - jakkhandy@hotmail.com
  // http://www.codeproject.com/KB/string/wildcmp.aspx

  const TCHAR* cp = NULL, *mp = NULL;

  while ((*string) && (*wild != TEXT('*')))
  {
    if ((*wild != *string) && (*wild != TEXT('?')))
      return 0;
    wild++;
    string++;
  }

  while (*string)
  {
    if (*wild == TEXT('*'))
    {
      if (!*++wild)
        return 1;
      mp = wild;
      cp = string+1;
    }
    else if ((*wild == *string) || (*wild == TEXT('?')))
    {
      wild++;
      string++;
    }
    else
    {
      wild = mp;
      string = cp++;
    }
  }

  while (*wild == TEXT('*'))
    wild++;
  return !*wild;
}

// Loop through all processes to see if they are running.
BOOL CALLBACK EnumProcessIdsProc(DWORD dwProcessId, LPARAM lParam)
{
  if (lParam == dwProcessId)
    return FALSE;
  return TRUE;
}

// Closes an application.
static void CloseApplication(HWND hWnd)
{
  SetActiveWindow(hWnd);
  SendMessageTimeout(hWnd, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, 0);
}

// Kills an application.
static void KillApplication(DWORD dwProcessId)
{
  HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, dwProcessId);
  if (hProcess != NULL)
  {
    TerminateProcess(hProcess, 0xFFFFFFFF);
    WaitForSingleObject(hProcess, 1000);
    CloseHandle(hProcess);
  }
}

// Enable or disable installer page buttons.
static void EnableButtons(BOOL enable)
{
  EnableWindow(g_hNext, enable);
  EnableWindow(g_hBack, enable);
  EnableWindow(g_hCancel, enable);
}

// Handles the parent dialog.
static BOOL CALLBACK ParentDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  BOOL bRes = CallWindowProc((WNDPROC)ParentDlgProcOld, hWnd, uMsg, wParam, lParam);
  if (!bRes)
  {
    if (uMsg == WM_NOTIFY_OUTER_NEXT)
    {
      // Don't auto-next if the user clicks Back or Cancel!
      g_fAutoNext = FALSE;

      // While the user waits, better disable the buttons.
      EnableButtons(FALSE);

      // Auto close.
      if (wParam == 1 && g_uiRunning > 0 && g_fAutoClose)
      {
        g_fAutoClosing = TRUE;
      }
      else
      {
        // Determine which button was pressed.
        if (wParam == -1)
          g_fBackClicked = TRUE;
        else if (wParam == NOTIFY_BYE_BYE)
          g_fCancelClicked = TRUE;
        g_fDone = TRUE;

        // End threads ASAP.
        FinishEnumeratingNow();
        PostMessage(g_hDialog, WM_CLOSE, 0, 0);
      }
    }
    else if (uMsg == WM_COMMAND && g_uiIgnoreBtn > 0 && HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == g_uiIgnoreBtn)
    {
      g_fAutoClose = FALSE;
      g_fIgnoreClicked = TRUE;
      PostMessage(g_hWndParent, WM_NOTIFY_OUTER_NEXT, 1, 0);
    }
  }
  return bRes;
}

// List window procedure.
LRESULT CALLBACK ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_SETCURSOR:
    {
      if (g_hProcessingCursor)
      {
        SetCursor(g_hProcessingCursor);
        return TRUE;
      }
    }
  }
  return CallWindowProc(ListWndProcOld, hWnd, uMsg, wParam, lParam);
}

// Dialog procedure.
static LRESULT CALLBACK DlgProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_CONTEXTMENU:
    {
      if (wParam != (WPARAM)g_hList)
        break;

      PTCHAR pszText = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      if (pszText == NULL)
        break;
      
      LVITEM lvi;
      lvi.mask = LVIF_TEXT;
      lvi.iSubItem = 1;
      lvi.pszText = pszText;
      lvi.cchTextMax = g_stringsize;

      // Check if there are any items in the list to show the context menu for.
      BOOL fShowMenu = FALSE;
      int count = ListView_GetItemCount(g_hList);
      for (lvi.iItem = 0; lvi.iItem < count; lvi.iItem++)
      {
        ListView_GetItem(g_hList, &lvi);
        if (*lvi.pszText)
        {
          fShowMenu = TRUE;
          break;
        }
      }
      if (!fShowMenu)
      {
        GlobalFree(pszText);
        break;
      }

      // Get the point of mouse click on the list.
      POINT pt;
      if (lParam == ((UINT)-1))
      {
        RECT r;
        GetWindowRect(g_hList, &r);
        pt.x = r.left;
        pt.y = r.top;
      }
      else
      {
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
      }

      // Determine which item was clicked on.
      LVHITTESTINFO lvhti;
      lvhti.flags = LVHT_ONITEM;
      lvhti.pt.x = pt.x;
      lvhti.pt.y = pt.y;
      ScreenToClient(g_hList, &lvhti.pt);

      // Get the param of the list view item clicked on.
      LISTITEM_INFO_PROCESS* pListItemInfoProcess = NULL;
      if ((lvi.iItem = ListView_HitTest(g_hList, &lvhti)) != -1)
      {
        lvi.mask = LVIF_PARAM;
        lvi.iSubItem = 0;
        ListView_GetItem(g_hList, &lvi);

        pListItemInfoProcess = GetListItemInfoProcess(lvi.lParam);
      }

      // Display the context menu.
      HMENU hMenu = CreatePopupMenu();
      if (pListItemInfoProcess != NULL && IsWindow(pListItemInfoProcess->hWnd))
        AppendMenu(hMenu, MF_STRING, 1, g_apszParams[PARAM_MENUITEM1] ? g_apszParams[PARAM_MENUITEM1] : TEXT_MENUITEM_1);
      AppendMenu(hMenu, MF_STRING, 2, g_apszParams[PARAM_MENUITEM2] ? g_apszParams[PARAM_MENUITEM2] : TEXT_MENUITEM_2);
      int sel = TrackPopupMenu(hMenu, TPM_NONOTIFY|TPM_RETURNCMD, pt.x, pt.y, 0, g_hList, 0);
      DestroyMenu(hMenu);

      // Close selected application.
      if (sel == 1)
      {
        if (IsWindow(pListItemInfoProcess->hWnd))
          CloseApplication(pListItemInfoProcess->hWnd);
      }
      // Copy list to clipboard.
      else if (sel == 2)
      {
        count = ListView_GetItemCount(g_hList);

        PTCHAR pszSubText = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
        if (pszSubText)
        {
          PTCHAR pszProcessId = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*5);
          if (pszProcessId)
          {
            PTCHAR pszProcess = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
            if (pszProcess)
            {
              PTCHAR pszFormat = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
              if (pszFormat)
              {
                PTCHAR pszList = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize*count);
                if (pszList)
                {
                  lvi.mask = LVIF_TEXT;
                  lvi.iSubItem = 0;
                  lvi.pszText = pszText;
                  lvi.cchTextMax = g_stringsize;

                  ZeroMemory(pszList, sizeof(TCHAR)*g_stringsize*count);
                  
                  // Calculate string lengths for tabulation.
                  int captionLen = 0;
                  for (lvi.iItem = 0; lvi.iItem < count; lvi.iItem++)
                  {
                    int len = 0;
                    ListView_GetItem(g_hList, &lvi);
                    if ((len = lstrlen(lvi.pszText)) > captionLen)
                      captionLen = len;
                  }
                  wsprintf(pszFormat, TEXT("%%-2d [%%-4s] - %%-%ds - %%s\r\n"), captionLen);

                  // Add the items to the string.
                  for (lvi.iItem = 0; lvi.iItem < count; lvi.iItem++)
                  {
                    lvi.mask = LVIF_TEXT;
                    lvi.iSubItem = 1;
                    lvi.pszText = pszSubText;
                    lvi.cchTextMax = g_stringsize;
                    ListView_GetItem(g_hList, &lvi);

                    if (*lvi.pszText)
                    {
                      lvi.mask = LVIF_TEXT | LVIF_PARAM;
                      lvi.iSubItem = 0;
                      lvi.pszText = pszText;
                      lvi.cchTextMax = g_stringsize;
                      ListView_GetItem(g_hList, &lvi);

                      pListItemInfoProcess = GetListItemInfoProcess(lvi.lParam);
                      if (pListItemInfoProcess != NULL)
                        wsprintf(pszProcessId, TEXT("%u"), pListItemInfoProcess->dwProcessId);
                      else
                        lstrcpy(pszProcessId, TEXT("?"));
                      
                      wsprintf(pszProcess, pszFormat, lvi.iItem+1, pszProcessId, pszText, pszSubText);
                      lstrcat(pszList, pszProcess);
                    }
                  }

                  // Write to the clipboard.
                  OpenClipboard(g_hWndParent);
                  EmptyClipboard();
#ifdef UNICODE
                  SetClipboardData(CF_UNICODETEXT, pszList);
#else
                  SetClipboardData(CF_TEXT, pszList);
#endif
                  CloseClipboard();

                  GlobalFree(pszList);
                } // if (pszList)
                GlobalFree(pszFormat);
              } // if (pszFormat)
              GlobalFree(pszProcess);
            } // if (pszProcess)
            GlobalFree(pszProcessId);
          } // if (pszProcessId)
          GlobalFree(pszSubText);
        } // if (pszSubText)
      }

      GlobalFree(pszText);
    }
    break;
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORDLG:
  case WM_CTLCOLORBTN:
  case WM_CTLCOLORLISTBOX:
    return SendMessage(g_hWndParent, uMsg, wParam, lParam);
  }
  return FALSE;
}

// Determines if the given custom item is locked by calling an NSIS callback function.
static BOOL IsCustomItemRunning(CUSTOM_ITEM* pCustomItem)
{
  BOOL fRunning = FALSE;
  PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);

  pushstring(pCustomItem->pszProcess);
  if (g_pExtraParameters->ExecuteCodeSegment(pCustomItem->iFunctionAddress, g_hWndParent) == 0)
    if (popstring(pszParam) == 0)
      fRunning = lstrcmpi(pszParam, TEXT("true")) == 0;

  GlobalFree(pszParam);
  return fRunning;
}

// Adds a process id to the g_adwProcessIds array. If the process id is already in the array, returns FALSE.
static BOOL AddProcess(DWORD dwProcessId)
{
  for (UINT i = 0; i < g_uiProcessCount; i++)
    if (g_adwProcessIds[i] == dwProcessId)
      return FALSE;

  g_adwProcessIds[g_uiProcessCount] = dwProcessId;
  g_uiProcessCount++;

  return TRUE;
}

// Adds a process to the g_adwProcessIds/g_apszProcesses arrays. If the process id is already in the arrays, returns FALSE.
static BOOL AddProcess(DWORD dwProcessId, PTCHAR pszProcess)
{
  for (UINT i = 0; i < g_uiProcessCount; i++)
    if (g_adwProcessIds[i] == dwProcessId && lstrcmp(g_apszProcesses[i], pszProcess) == 0)
      return FALSE;

  g_adwProcessIds[g_uiProcessCount] = dwProcessId;
  g_apszProcesses[g_uiProcessCount] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
  lstrcpy(g_apszProcesses[g_uiProcessCount], pszProcess);
  g_uiProcessCount++;
  
  return TRUE;
}

// Invoke the callback function. Returns TRUE if enumeration should continue; FALSE otherwise.
static BOOL AddItem(const PTCHAR pszProcessDescription, const PTCHAR pszProcessFullPath, int iFunctionAddress, LISTITEM_INFO* pListItemInfo)
{
  if (pListItemInfo->listItemType != ListItemProcess || AddProcess(((LISTITEM_INFO_PROCESS*)pListItemInfo->lpData)->dwProcessId))
  {
    BOOL bResult = FALSE;
    PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
    
    pushstring(pszProcessDescription);
    pushstring(pszProcessFullPath);
    wsprintf(pszParam, TEXT("%u"), ((LISTITEM_INFO_PROCESS*)pListItemInfo->lpData)->dwProcessId);
    pushstring(pszParam);

    if (g_pExtraParameters->ExecuteCodeSegment(iFunctionAddress, g_hWndParent) == 0 && popstring(pszParam) == 0)
    {
      // Continue enumeration.
      if (lstrcmpi(pszParam, TEXT("true")) == 0)
      {
        bResult = TRUE;
      }
      // Auto close process and then continue.
      else if (lstrcmpi(pszParam, TEXT("autoclose")) == 0)
      {
        BOOL fRunning;
        int iAutoClose = 0;
        bResult = TRUE;

        do
        {
          if (pListItemInfo->listItemType == ListItemProcess)
          {
            LISTITEM_INFO_PROCESS* pListItemInfoProcess = (LISTITEM_INFO_PROCESS*)pListItemInfo->lpData;

            // Close via WM_CLOSE.
            BOOL fIsWindow;
            if (iAutoClose == 0 && (fIsWindow = IsWindow(pListItemInfoProcess->hWnd)))
            {
              CloseApplication(pListItemInfoProcess->hWnd);
              iAutoClose = 1;
              Sleep(2000);
            }
            // Try termination.
            else if ((iAutoClose == 1 || !fIsWindow) && pListItemInfoProcess->dwProcessId > 0)
            {
              KillApplication(pListItemInfoProcess->dwProcessId);
              Sleep(500);
            }
            // Auto close failed.
            else
            {
              pushstring(pszProcessDescription);
              pushstring(pszProcessFullPath);
              pushstring(TEXT("-1"));

              g_pExtraParameters->ExecuteCodeSegment(iFunctionAddress, g_hWndParent);
              bResult = FALSE;
              break;
            }

            ENUM_OPTIONS opt;
            opt.GetWindowCaption = FALSE;
            opt.lParam = pListItemInfoProcess->dwProcessId;

            fRunning = !EnumProcessIds(EnumProcessIdsProc, &opt);
          }
          else
          {
            fRunning = IsCustomItemRunning(((LISTITEM_INFO_CUSTOM*)pListItemInfo->lpData)->pCustomItem);
          }
        }
        while (fRunning);
      }
    }

    LocalFree(pszParam);
    return bResult;
  }

  return TRUE;
}

// Add item to the list box.
static BOOL AddItem(PTCHAR pszText, size_t cchText, PTCHAR pszSubText, size_t cchSubText, HICON hIcon, LISTITEM_INFO* pListItemInfo)
{
  if (pListItemInfo->listItemType != ListItemProcess || AddProcess(((LISTITEM_INFO_PROCESS*)pListItemInfo->lpData)->dwProcessId, pszText))
  {
    LVITEM lvi;
    lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
    lvi.pszText = pszText;
    lvi.cchTextMax = cchText;
    lvi.iItem = ListView_GetItemCount(g_hList)-1;
    lvi.iSubItem = 0;
    lvi.lParam = (LPARAM)pListItemInfo;
    lvi.state = 0;
    lvi.stateMask = 0;

    // Add the executable icon to the image list.
    if (hIcon)
    {
      HIMAGELIST hImageList = ListView_GetImageList(g_hList, LVSIL_SMALL);
      lvi.iImage = ImageList_AddIcon(hImageList, hIcon);
      DestroyIcon(hIcon);
    }
    else
    {
      lvi.iImage = 2;
    }
    
    // Add the item.
    lvi.iItem = ListView_InsertItem(g_hList, &lvi);

    // Set its sub item texts.
    lvi.mask = LVIF_TEXT;
    lvi.pszText = pszSubText;
    lvi.cchTextMax = cchSubText;
    lvi.iSubItem = 1;
    ListView_SetItem(g_hList, &lvi);

    // Scroll the item into view.
    ListView_EnsureVisible(g_hList, g_uiProcessCount, FALSE);
  }

  return TRUE;
}

// Add process to the list box or call the callback function.
static BOOL AddItem(FILE_INFORMATION& file, OUT_WHERE_INFO outWhereInfo, PTCHAR szTemp, int cchTemp)
{
  // Get process image file name.
  if (*file.ProcessFullPath)
  {
    GetFileFromPath(file.ProcessFullPath, sizeof(file.ProcessFullPath), szTemp, cchTemp);

    // Skip explorer.exe.
    if (lstrcmpi(szTemp, TEXT("explorer.exe")) == 0)
      return TRUE;
  }
  else
  {
    lstrcpy(szTemp, TEXT(""));
  }

  // Create the item param.
  LISTITEM_INFO* pListItemInfo = (LISTITEM_INFO*)GlobalAlloc(GPTR, sizeof(LISTITEM_INFO));
  LISTITEM_INFO_PROCESS* pListItemInfoProcess = (LISTITEM_INFO_PROCESS*)GlobalAlloc(GPTR, sizeof(LISTITEM_INFO_PROCESS));
  pListItemInfoProcess->hWnd = file.ProcessHWND;
  pListItemInfoProcess->dwProcessId = file.ProcessId;
  pListItemInfo->listItemType = ListItemProcess;
  pListItemInfo->lpData = (LONG_PTR)pListItemInfoProcess;

  // Add the item to the list box.
  if (outWhereInfo.outWhere == OutListBox)
  {
    // Get the icon for this process.
    HICON hIcon;
    if (*file.ProcessFullPath)
      ExtractIconEx(file.ProcessFullPath, 0, NULL, &hIcon, 1);
    else
      hIcon = NULL;

    return AddItem(file.ProcessDescription, SYSTEMENUM_CAPTION_SIZE, szTemp, cchTemp, hIcon, pListItemInfo);
  }

  // Call the callback.
  BOOL bResult = AddItem(file.ProcessDescription, file.ProcessFullPath, outWhereInfo.iFunctionAddress, pListItemInfo);
  GlobalFree(pListItemInfoProcess);
  GlobalFree(pListItemInfo);
  return bResult;
}

// Callback for finding a process by name.
BOOL CALLBACK FindProcessProc(FILE_INFORMATION& file, LPARAM lParam)
{
  // Skip System Idle Process and System.
  if (IsSystemProcess(file.ProcessId))
    return TRUE;

  // Note that lstrcmp(file.FullPath, file.ProcessFullPath) == 0

  FIND_PROCESS_PROC_ARGS* pstArgs = (FIND_PROCESS_PROC_ARGS*)lParam;

  if (EndsWith(CharLower(file.FullPath), pstArgs->pszProcessExe))
  {
    if (pstArgs->fYesNo)
    {
      pushstring(OUT_YES);
    }
    else
    {
      pushstring(file.ProcessDescription);
      pushstring(file.ProcessFullPath);
      wsprintf(pstArgs->pszProcessExe, TEXT("%u"), file.ProcessId);
      pushstring(pstArgs->pszProcessExe);
    }

    pstArgs->fSuccess = TRUE;
    return FALSE;
  }

  return TRUE;
}

// Callback for closing or terminating a process by name.
BOOL CALLBACK CloseProcessProc(FILE_INFORMATION& file, LPARAM lParam)
{
  // Skip System Idle Process and System.
  if (IsSystemProcess(file.ProcessId))
    return TRUE;

  // Note that lstrcmp(file.FullPath, file.ProcessFullPath) == 0

  FIND_PROCESS_PROC_ARGS* pstArgs = (FIND_PROCESS_PROC_ARGS*)lParam;

  if (EndsWith(CharLower(file.FullPath), pstArgs->pszProcessExe))
  {
    if (pstArgs->fYesNo)
      KillApplication(file.ProcessId);
    else
      CloseApplication(file.ProcessHWND);

    pstArgs->fSuccess = TRUE;
  }

  return TRUE;
}

// Callback for enumerating system processes.
BOOL CALLBACK EnumSystemProcessesProc(FILE_INFORMATION& file, LPARAM lParam)
{
  static TCHAR szTemp[16];
  
  // Skip System Idle Process and System.
  if (IsSystemProcess(file.ProcessId))
    return TRUE;

  pushstring(file.ProcessDescription);
  pushstring(file.ProcessFullPath);
  wsprintf(szTemp, TEXT("%u"), file.ProcessId);
  pushstring(szTemp);

  if (g_pExtraParameters->ExecuteCodeSegment((int)lParam, g_hWndParent) == 0 && popstring(szTemp) == 0 && lstrcmpi(szTemp, TEXT("true")) == 0)
    return TRUE;

  return FALSE;
}

// Callback for enumerating locked files.
BOOL CALLBACK EnumLockedFilesProc(FILE_INFORMATION& file, LPARAM lParam)
{
  LIST_INFO* pFiles = (LIST_INFO*)lParam;
  static TCHAR szTemp[512];

  UINT iNewPercentComplete = pFiles->ulTotalCount == 0 ? 100 : 1000000 / pFiles->ulTotalCount * (pFiles->ulTotalCurrent + file.FileNumber) / 10000;

  // Update the progress.
  if (g_uiPercentComplete != iNewPercentComplete)
  {
    LVITEM lvi;
    lvi.mask = LVIF_TEXT;
    lvi.iItem = ListView_GetItemCount(g_hList)-1;
    lvi.iSubItem = 0;
    lvi.pszText = szTemp;
    lvi.cchTextMax = sizeof(szTemp);

    wsprintf(szTemp, TEXT("%u%% %s"), iNewPercentComplete, (g_apszParams[PARAM_SEARCHING] ? g_apszParams[PARAM_SEARCHING] : TEXT_SEARCHING));
    SendMessage(g_hProgress, PBM_SETPOS, iNewPercentComplete, 0);
    ListView_SetItem(g_hList, &lvi);

    g_uiPercentComplete = iNewPercentComplete;
  }

  if (pFiles->listType == ListTypeCustom)
  {
    CUSTOM_ITEM* pCustomItem = (CUSTOM_ITEM*)pFiles->lpList;
    if (IsCustomItemRunning(pCustomItem))
    {
      // Create the item param.
      LISTITEM_INFO* pListItemInfo = (LISTITEM_INFO*)GlobalAlloc(GPTR, sizeof(LISTITEM_INFO));
      LISTITEM_INFO_CUSTOM* pListItemInfoCustom = (LISTITEM_INFO_CUSTOM*)GlobalAlloc(GPTR, sizeof(LISTITEM_INFO_CUSTOM));
      pListItemInfoCustom->pCustomItem = pCustomItem;
      pListItemInfo->listItemType = ListItemCustom;
      pListItemInfo->lpData = (LONG_PTR)pListItemInfoCustom;

      // Add the item to the list box.
      if (pFiles->outWhereInfo.outWhere == OutListBox)
        return AddItem(pCustomItem->pszApplication, g_stringsize, pCustomItem->pszProcess, g_stringsize, pCustomItem->hIcon, pListItemInfo);

      // Call the callback.
      BOOL bResult = AddItem(pCustomItem->pszApplication, pCustomItem->pszProcess, pFiles->outWhereInfo.iFunctionAddress, pListItemInfo);
      GlobalFree(pListItemInfoCustom);
      GlobalFree(pListItemInfo);
      return bResult;
    }
  }
  else
  {
    // Skip System Idle Process and System.
    if (IsSystemProcess(file.ProcessId))
      return TRUE;

    // Skip current process unless outputting to callback.
    if (pFiles->outWhereInfo.outWhere != OutCallback && file.ProcessId == pFiles->dwCurrentProcessId)
      return TRUE;

    // Match folders.
    if (pFiles->listType == ListTypeFiles || pFiles->listType == ListTypeModules)
    {
      for (UINT i = 0; i < g_uiFoldersCount; i++)
      {
        if (StartsWith(CharLower(file.FullPath), g_apszFoldersList[i]) && !AddItem(file, pFiles->outWhereInfo, szTemp, sizeof(szTemp)))
          return FALSE;
      }
    }

    PTCHAR* ppszList = (PTCHAR*)pFiles->lpList;
    for (UINT i = 0; i < pFiles->uiListCount; i++)
    {
      if (g_fDone)
        return FALSE;

      // Match paths (path ends with).
      if (pFiles->listType == ListTypeFiles || pFiles->listType == ListTypeModules)
      {
        if (!EndsWith(CharLower(file.FullPath), ppszList[i]))
          continue;
      }
      // Match captions (using wildcards).
      else if (pFiles->listType == ListTypeCaptions)
      {
        lstrcpy(szTemp, file.ProcessDescription);
        if (!wildcmp(ppszList[i], CharLower(szTemp)))
          continue;
      }
      // Match class names (using wildcards).
      else if (pFiles->listType == ListTypeClasses && IsWindow(file.ProcessHWND))
      {
        GetClassName(file.ProcessHWND, szTemp, sizeof(szTemp));
        if (!wildcmp(ppszList[i], CharLower(szTemp)))
          continue;
      }

      if (!AddItem(file, pFiles->outWhereInfo, szTemp, sizeof(szTemp)))
        return FALSE;
    }
  }

  return TRUE;
}

// Free allocated memory.
static void FreeGlobalAllocs()
{
  // Free allocated lists memory.
  for (int i = g_uiFilesCount - 1; i >= 0; i--)
  {
    if (g_apszFilesList[i] != NULL)
    {
      GlobalFree(g_apszFilesList[i]);
      g_apszFilesList[i] = NULL;
    }
  }

  for (int i = g_uiModulesCount - 1; i >= 0; i--)
  {
    if (g_apszModulesList[i] != NULL)
    {
      GlobalFree(g_apszModulesList[i]);
      g_apszModulesList[i] = NULL;
    }
  }

  for (int i = g_uiClassesCount - 1; i >= 0; i--)
  {
    if (g_apszClassesList[i] != NULL)
    {
      GlobalFree(g_apszClassesList[i]);
      g_apszClassesList[i] = NULL;
    }
  }

  for (int i = g_uiCaptionsCount - 1; i >= 0; i--)
  {
    if (g_apszCaptionsList[i] != NULL)
    {
      GlobalFree(g_apszCaptionsList[i]);
      g_apszCaptionsList[i] = NULL;
    }
  }

  for (int i = g_uiCustomItemsCount - 1; i >= 0; i--)
  {
    if (g_aCustomItemsList[i].pszApplication != NULL)
    {
      GlobalFree(g_aCustomItemsList[i].pszApplication);
      g_aCustomItemsList[i].pszApplication = NULL;

      GlobalFree(g_aCustomItemsList[i].pszProcess);
      g_aCustomItemsList[i].pszProcess = NULL;

      if (g_aCustomItemsList[i].hIcon != NULL)
      {
        DestroyIcon(g_aCustomItemsList[i].hIcon);
        g_aCustomItemsList[i].hIcon = NULL;
      }
    }
  }

  for (int i = g_uiFoldersCount - 1; i >= 0; i--)
  {
    if (g_apszFoldersList[i] != NULL)
    {
      GlobalFree(g_apszFoldersList[i]);
      g_apszFoldersList[i] = NULL;
    }
  }

  g_uiFilesCount = g_uiModulesCount = g_uiClassesCount = g_uiCaptionsCount = g_uiCustomItemsCount = g_uiFoldersCount = 0;

  // Free allocated parameters memory.
  for (int i = 0; i < MAX_PARAMS; i++)
  {
    if (g_apszParams[i])
    {
      GlobalFree(g_apszParams[i]);
      g_apszParams[i] = NULL;
    }
  }

  // Free process arrays.
  for (int i = 0; i < MAX_PROCESSES; i++)
  {
    g_adwProcessIds[i] = 0;
    if (g_apszProcesses[i] != NULL)
    {
      GlobalFree(g_apszProcesses[i]);
      g_apszProcesses[i] = NULL;
    }
  }

  if (g_pszNext != NULL)
  {
    GlobalFree(g_pszNext);
    g_pszNext = NULL;
  }

  g_uiPercentComplete = 0;
  g_uiProcessCount = 0;
}

// Clears all lists and resets variable values to defaults.
static void ResetState()
{
  // Free allocated memory.
  FreeGlobalAllocs();

  // Reset global variables.
  g_hDialog = NULL;
  g_fDone = g_fAutoClose = g_fUserIcons = g_fShowRunningApplications = g_fAutoNext = g_fAutoClosing = g_fCancelClicked = g_fBackClicked = g_fIgnoreClicked = FALSE;
  g_uiIgnoreBtn = g_uiRunning = 0;
  g_hThreadFiles = NULL;
}

// Execute the system enumerations.
static BOOL SystemEnum(OUT_WHERE_INFO outWhereInfo)
{
  POINT pt;

  LIST_INFO files;
  files.outWhereInfo = outWhereInfo;
  files.dwCurrentProcessId = GetCurrentProcessId();

  ENUM_OPTIONS opt;
  opt.GetWindowCaption = outWhereInfo.outWhere != OutCallback;
  opt.lParam = (LPARAM)&files;

  if (outWhereInfo.outWhere != OutCallback)
  {
    // Get processing cursor.
    g_hProcessingCursor = (HCURSOR)LoadImage(NULL, IDC_APPSTARTING, IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    // Redraw cursor the easy way.
    GetCursorPos(&pt);
    SetCursorPos(pt.x-1, pt.y);
  }

  ULONG ulHandlesCount = 0;
  ULONG ulModulesCount = 0;
  ULONG ulClassesCount = 0;
  ULONG ulCaptionsCount = 0;
  ULONG ulApplicationsCount = 0;

  // Get number of handles if checking files/folders.
  if (g_uiFilesCount || g_uiFoldersCount)
    ulHandlesCount = GetSystemHandlesCount();

  // Get number of modules if checking modules/folders.
  if (g_uiModulesCount || g_uiFoldersCount)
    ulModulesCount = GetSystemModulesCount() + GetSystemModulesCount64();

  // Get number of applications if checking window classes.
  if (g_uiClassesCount)
    ulClassesCount = GetApplicationsCount();

  // Get number of applications if checking window captions.
  if (g_uiCaptionsCount)
  {
    if (ulClassesCount > 0)
      ulCaptionsCount = ulClassesCount;
    else
      ulCaptionsCount = GetApplicationsCount();
  }

  // Get number of applications if showing all applications.
  if (g_fShowRunningApplications)
  {
    if (ulClassesCount > 0)
      ulApplicationsCount = ulClassesCount;
    else if (ulCaptionsCount > 0)
      ulApplicationsCount = ulCaptionsCount;
    else
      ulApplicationsCount = GetApplicationsCount();
  }

  // Add them all up for progress indication.
  files.ulTotalCount = g_uiCustomItemsCount + ulHandlesCount + ulModulesCount + ulClassesCount + ulCaptionsCount + ulApplicationsCount;
  files.ulTotalCurrent = 0;

  // Zero process arrays.
  for (int i = 0; i < MAX_PROCESSES; i++)
  {
    g_adwProcessIds[i] = 0;
    g_apszProcesses[i] = NULL;
  }

  // List custom items first.
  if (g_uiCustomItemsCount && !g_fDone)
  {
    files.uiListCount    = 1;
    files.listType       = ListTypeCustom;
    files.ulTotalCurrent += 0;

    FILE_INFORMATION file;
    file.TotalFiles = g_uiCustomItemsCount;
    for (UINT i = 0; i < file.TotalFiles; i++)
    {
      file.FileNumber = i+1;
      files.lpList = (LONG_PTR)&g_aCustomItemsList[i];
      if (!EnumLockedFilesProc(file, (LPARAM)&files) && outWhereInfo.outWhere == OutCallback)
        return FALSE;
    }
  }

  // List all running applications first.
  if (g_fShowRunningApplications && !g_fDone)
  {
    files.lpList         = NULL;
    files.uiListCount    = 1;
    files.listType       = ListTypeApplications;
    files.ulTotalCurrent += g_uiCustomItemsCount;
    if (!EnumApplications(EnumLockedFilesProc, &opt) && outWhereInfo.outWhere == OutCallback)
      return FALSE;
  }

  // List all running applications first.
  if (g_uiCaptionsCount && !g_fDone)
  {
    files.lpList         = (LONG_PTR)g_apszCaptionsList;
    files.uiListCount    = g_uiCaptionsCount;
    files.listType       = ListTypeCaptions;
    files.ulTotalCurrent += ulApplicationsCount;
    if (!EnumApplications(EnumLockedFilesProc, &opt) && outWhereInfo.outWhere == OutCallback)
      return FALSE;
  }

  // List all running applications first.
  if (g_uiClassesCount && !g_fDone)
  {
    files.lpList         = (LONG_PTR)g_apszClassesList;
    files.uiListCount    = g_uiClassesCount;
    files.listType       = ListTypeClasses;
    files.ulTotalCurrent += ulCaptionsCount;
    if (!EnumApplications(EnumLockedFilesProc, &opt) && outWhereInfo.outWhere == OutCallback)
      return FALSE;
  }

  // List modules.
  if ((g_uiFoldersCount || g_uiModulesCount) && !g_fDone)
  {
    files.lpList         = (LONG_PTR)g_apszModulesList;
    files.uiListCount    = g_uiModulesCount;
    files.listType       = ListTypeModules;
    files.ulTotalCurrent += ulClassesCount;
	  if (!EnumSystemProcesses(EnumLockedFilesProc, &opt, TRUE) && outWhereInfo.outWhere == OutCallback)
      return FALSE;
    //-- Ilya Kotelnikov> enum x64 processes if requested by using a helper process.
	  if (!EnumSystemProcesses64(EnumCallback_EnumLockedFilesProc, &opt, TRUE) && outWhereInfo.outWhere == OutCallback)
		  return FALSE;
  }

  // List files.
  if ((g_uiFoldersCount || g_uiFilesCount) && !g_fDone)
  {
    files.lpList         = (LONG_PTR)g_apszFilesList;
    files.uiListCount    = g_uiFilesCount;
    files.listType       = ListTypeFiles;
    files.ulTotalCurrent += ulModulesCount;
    if (!EnumSystemHandles(EnumLockedFilesProc, &opt) && outWhereInfo.outWhere == OutCallback)
      return FALSE;
  }
  
  if (outWhereInfo.outWhere != OutCallback)
  {
    // Set the progress bar position to max now that we're done.
    SendMessage(g_hProgress, PBM_SETPOS, 100, 0);

    // Redraw cursor the easy way.
    g_hProcessingCursor = NULL;
    GetCursorPos(&pt);
    SetCursorPos(pt.x-1, pt.y);
  }

  return TRUE;
}

// Loop through files and modules.
DWORD WINAPI FilesThreadSilent(LPVOID lpParameter)
{
  OUT_WHERE_INFO outWhereInfo;
  outWhereInfo.outWhere = OutCallback;
  outWhereInfo.iFunctionAddress = (int)lpParameter;

  BOOL bResult = SystemEnum(outWhereInfo);
  ResetState();

  return bResult;
}

#define EndProgramMonitoringRequested() g_fDone || g_uiRunning == 0 || g_fAutoClosing

#define BreakableSleep(Condition, dwMilliseconds, dwBreakCheckInterval) \
  for (int i = 0; i < dwMilliseconds / dwBreakCheckInterval; i++)\
  {\
    if (Condition)\
      break;\
    Sleep(dwBreakCheckInterval);\
  }

// Loop through files and modules.
DWORD WINAPI FilesThread(LPVOID)
{
  LVITEM lvi;

  OUT_WHERE_INFO outWhereInfo;
  outWhereInfo.outWhere = OutListBox;
  outWhereInfo.iFunctionAddress = 0;

  SystemEnum(outWhereInfo);

  g_uiRunning = ListView_GetItemCount(g_hList)-1;
  lvi.iSubItem = 0;

  // Set Next button text.
  if (g_uiRunning == 0)
  {
    EnableWindow(g_hNext, TRUE);
    if (g_fAutoNext)
    {
      //g_fDone = TRUE;
      g_fAutoClose = FALSE;
      PostMessage(g_hWndParent, WM_NOTIFY_OUTER_NEXT, 1, 0);
    }
  }
  else if (g_apszParams[PARAM_IGNORE])
  {
    if (*g_apszParams[PARAM_IGNORE])
      SetWindowText(g_hNext, g_apszParams[PARAM_IGNORE]);
    EnableWindow(g_hNext, TRUE);
  }

  // Remove "Searching" item from list.
  ListView_DeleteItem(g_hList, g_uiRunning);

  // Not finished searching but the user wants to leave the page.
  if (g_fDone)
  {
    lvi.mask = LVIF_TEXT | LVIF_IMAGE;
    lvi.iItem = g_uiRunning;
    lvi.iImage = 0;
    lvi.pszText = (g_apszParams[PARAM_ENDSEARCH] ? g_apszParams[PARAM_ENDSEARCH] : TEXT_ENDSEARCH);
    ListView_InsertItem(g_hList, &lvi);
    ListView_EnsureVisible(g_hList, g_uiRunning, FALSE);
  }
  // The list is not empty, so monitor running programs.
  else if (g_uiRunning > 0)
  {
    PTCHAR pszCaption = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*256);
    GetWindowText(g_hWndParent, pszCaption, 256);

    lvi.mask = LVIF_PARAM;

    while (g_uiRunning > 0 && !g_fDone)
    {
      // Close programs on list?
      if (g_fAutoClosing == TRUE)
      {
        if (g_fAutoClose == TRUE && MessageBox(g_hWndParent, (g_apszParams[PARAM_AUTOCLOSE_CLOSE] && g_apszParams[PARAM_AUTOCLOSE_CLOSE][0] ? g_apszParams[PARAM_AUTOCLOSE_CLOSE] : TEXT_MSGBOX_CLOSE), pszCaption, MB_ICONEXCLAMATION | MB_YESNO) == IDNO)
        {
          EnableButtons(TRUE);
          g_fAutoClosing = FALSE;
        }
        else
        {
          for (lvi.iItem = 0; (UINT)lvi.iItem < g_uiRunning; lvi.iItem++)
          {
            ListView_GetItem(g_hList, &lvi);
            LISTITEM_INFO_PROCESS* pListItemInfoProcess = GetListItemInfoProcess(lvi.lParam);
            if (pListItemInfoProcess != NULL && IsWindow(pListItemInfoProcess->hWnd))
              CloseApplication(pListItemInfoProcess->hWnd);
          }
          BreakableSleep(EndProgramMonitoringRequested(), 1000, 10);
          g_fAutoClosing = 2;
        }
      }
      // Kill programs on list?
      else if (g_fAutoClosing == 2)
      {
        if (g_fAutoClose == TRUE && MessageBox(g_hWndParent, (g_apszParams[PARAM_AUTOCLOSE_KILL] && g_apszParams[PARAM_AUTOCLOSE_KILL][0] ? g_apszParams[PARAM_AUTOCLOSE_KILL] : TEXT_MSGBOX_KILL), pszCaption, MB_ICONEXCLAMATION | MB_YESNO) == IDNO)
        {
          EnableButtons(TRUE);
          g_fAutoClosing = FALSE;
        }
        else
        {
          for (lvi.iItem = 0; (UINT)lvi.iItem < g_uiRunning; lvi.iItem++)
          {
            ListView_GetItem(g_hList, &lvi);
            LISTITEM_INFO_PROCESS* pListItemInfoProcess = GetListItemInfoProcess(lvi.lParam);
            if (pListItemInfoProcess != NULL && pListItemInfoProcess->dwProcessId > 0)
              KillApplication(pListItemInfoProcess->dwProcessId);
          }
          g_fAutoClosing = 3;
        }
      }
      // Failed to close and kill programs.
      else if (g_fAutoClosing == 3)
      {
        MessageBox(g_hWndParent, (g_apszParams[PARAM_AUTOCLOSE_FAILED] && g_apszParams[PARAM_AUTOCLOSE_FAILED][0] ? g_apszParams[PARAM_AUTOCLOSE_FAILED] : TEXT_MSGBOX_ACFAILED), pszCaption, MB_ICONSTOP | MB_OK);
        EnableButtons(TRUE);
        g_fAutoClosing = FALSE;
      }

      // Loop through programs on list.
      for (lvi.iItem = 0; (UINT)lvi.iItem < g_uiRunning; lvi.iItem++)
      {
        if (g_fDone)
          break;

        ListView_GetItem(g_hList, &lvi);
        if (lvi.lParam != NULL)
        {
          LISTITEM_INFO* pListItemInfo = (LISTITEM_INFO*)lvi.lParam;

          // Check if process is still running.
          BOOL fRunning;
          if (pListItemInfo->listItemType == ListItemProcess)
          {
            ENUM_OPTIONS opt;
            opt.GetWindowCaption = FALSE;
            opt.lParam = ((LISTITEM_INFO_PROCESS*)pListItemInfo->lpData)->dwProcessId;

            fRunning = !EnumProcessIds(EnumProcessIdsProc, &opt);
          }
          else
          {
            fRunning = IsCustomItemRunning(((LISTITEM_INFO_CUSTOM*)pListItemInfo->lpData)->pCustomItem);
          }

          // Delete the process from the list.
          if (!fRunning && ListView_DeleteItem(g_hList, lvi.iItem))
          {
            // Free list view item param.
            if (lvi.lParam != NULL)
            {
              LISTITEM_INFO* pListItemInfo = (LISTITEM_INFO*)lvi.lParam;
              GlobalFree((HGLOBAL)pListItemInfo->lpData);
              GlobalFree(pListItemInfo);
            }

            // Decrement running process count.
            g_uiRunning--;
            lvi.iItem--;
          }
        } //if (lvi.lParam != NULL

      } // for (lvi.iItem =

      // Sleep for 1 second.
      BreakableSleep(EndProgramMonitoringRequested(), 1000, 10);

    } // while (g_uiRunning && !g_fDone
    
    GlobalFree(pszCaption);

    // Process monitoring was under way, but the user wants to leave the page (g_fDone = TRUE).
    if (g_uiRunning > 0)
    {
      lvi.mask = LVIF_TEXT | LVIF_IMAGE;
      lvi.iItem = g_uiRunning;
      lvi.iImage = 0;
      lvi.pszText = (g_apszParams[PARAM_ENDMONITOR] ? g_apszParams[PARAM_ENDMONITOR] : TEXT_ENDMONITOR);
      ListView_InsertItem(g_hList, &lvi);
    }
  } // if (g_uiRunning > 0

  // No programs found in the search,
  // or all programs once on the list have been closed.
  if (!g_fDone)
  {
    lvi.mask = LVIF_TEXT | LVIF_IMAGE;
    lvi.iItem = g_uiRunning;
    lvi.iImage = 0;
    lvi.pszText = (g_apszParams[PARAM_NOPROGRAMS] ? g_apszParams[PARAM_NOPROGRAMS] : TEXT_NOPROGRAMS);
    ListView_InsertItem(g_hList, &lvi);

    if (g_fAutoClosing || g_fAutoNext)
    {
      g_fAutoClose = FALSE;
      PostMessage(g_hWndParent, WM_NOTIFY_OUTER_NEXT, 1, 0);
    }
    else
    {
      EnableWindow(g_hNext, TRUE);
      if (g_pszNext && *g_pszNext)
        SetWindowText(g_hNext, g_pszNext);
    }
  } // if (!g_fDone

  ListView_EnsureVisible(g_hList, g_uiRunning, FALSE);

  g_fMonitoringDone = TRUE;
  PostMessage(g_hDialog, WM_CLOSE, 0, 0);

  return FALSE;
}

// Creates our dialog.
static void LoadDialog()
{
  for (int i = 0; i < MAX_PARAMS; i++)
    g_apszParams[i] = NULL;

  // Get plugin parameters.
  PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
  while (popstring(pszParam) == 0)
  {
    // Get heading text.
    if (lstrcmpi(pszParam, TEXT("/heading")) == 0)
    {
      if (g_apszParams[PARAM_HEADING] == NULL)
        g_apszParams[PARAM_HEADING] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_HEADING]);
    }
    // Get no programs item text.
    else if (lstrcmpi(pszParam, TEXT("/noprograms")) == 0)
    {
      if (g_apszParams[PARAM_NOPROGRAMS] == NULL)
        g_apszParams[PARAM_NOPROGRAMS] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_NOPROGRAMS]);
    }
    // Get searching item text.
    else if (lstrcmpi(pszParam, TEXT("/searching")) == 0)
    {
      if (g_apszParams[PARAM_SEARCHING] == NULL)
        g_apszParams[PARAM_SEARCHING] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_SEARCHING]);
    }
    // Get cancelling search item text.
    else if (lstrcmpi(pszParam, TEXT("/endsearch")) == 0)
    {
      if (g_apszParams[PARAM_ENDSEARCH] == NULL)
        g_apszParams[PARAM_ENDSEARCH] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_ENDSEARCH]);
    }
    // Get cancelling process monitoring item text.
    else if (lstrcmpi(pszParam, TEXT("/endmonitor")) == 0)
    {
      if (g_apszParams[PARAM_ENDMONITOR] == NULL)
        g_apszParams[PARAM_ENDMONITOR] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_ENDMONITOR]);
    }
    // Get caption.
    else if (lstrcmpi(pszParam, TEXT("/caption")) == 0)
    {
      popstring(pszParam);
      SetWindowText(g_hWndParent, pszParam);
    }
    // Get menu texts.
    else if (lstrcmpi(pszParam, TEXT("/menuitems")) == 0)
    {
      if (g_apszParams[PARAM_MENUITEM1] == NULL)
        g_apszParams[PARAM_MENUITEM1] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_MENUITEM1]);

      if (g_apszParams[PARAM_MENUITEM2] == NULL)
        g_apszParams[PARAM_MENUITEM2] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_MENUITEM2]);
    }
    // Allow next button?
    else if (lstrcmpi(pszParam, TEXT("/ignore")) == 0)
    {
      if (g_apszParams[PARAM_IGNORE] == NULL)
        g_apszParams[PARAM_IGNORE] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_IGNORE]);

      if (g_pszNext == NULL)
        g_pszNext = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
    }
    // Auto close/kill on next click?
    else if (lstrcmpi(pszParam, TEXT("/autoclose")) == 0 || lstrcmpi(pszParam, TEXT("/autoclosesilent")) == 0)
    {
      g_fAutoClose = (lstrcmpi(pszParam, TEXT("/autoclose")) == 0 ? TRUE : 2);
      if (g_fAutoClose == TRUE)
      {
        if (g_apszParams[PARAM_AUTOCLOSE_CLOSE] == NULL)
          g_apszParams[PARAM_AUTOCLOSE_CLOSE] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
        popstring(g_apszParams[PARAM_AUTOCLOSE_CLOSE]);

        if (g_apszParams[PARAM_AUTOCLOSE_KILL] == NULL)
          g_apszParams[PARAM_AUTOCLOSE_KILL] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
        popstring(g_apszParams[PARAM_AUTOCLOSE_KILL]);
      }

      if (g_apszParams[PARAM_AUTOCLOSE_FAILED] == NULL)
        g_apszParams[PARAM_AUTOCLOSE_FAILED] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_AUTOCLOSE_FAILED]);

      if (g_apszParams[PARAM_IGNORE] == NULL)
        g_apszParams[PARAM_IGNORE] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_IGNORE]);
    }
    // Column headings text?
    else if (lstrcmpi(pszParam, TEXT("/colheadings")) == 0)
    {
      if (g_apszParams[PARAM_COLHEADING_APPLICATION] == NULL)
        g_apszParams[PARAM_COLHEADING_APPLICATION] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_COLHEADING_APPLICATION]);

      if (g_apszParams[PARAM_COLHEADING_PROCESS] == NULL)
        g_apszParams[PARAM_COLHEADING_PROCESS] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      popstring(g_apszParams[PARAM_COLHEADING_PROCESS]);
    }
    // Use user icons rather than shell icons.
    else if (lstrcmpi(pszParam, TEXT("/usericons")) == 0)
    {
      g_fUserIcons = TRUE;
    }
    // Automatically click Next if no programs are running.
    else if (lstrcmpi(pszParam, TEXT("/autonext")) == 0)
    {
      g_fAutoNext = TRUE;
    }
    // Add new ignore button.
    else if (lstrcmpi(pszParam, TEXT("/ignorebtn")) == 0)
    {
      g_uiIgnoreBtn = (UINT)popint();
    }
    // End of parameters.
    else
    {
      pushstring(pszParam);
      break;
    }
  }
  GlobalFree(pszParam);

  g_hDialog = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG), g_hWndParent, (DLGPROC)DlgProc);
}

// Displays our dialog.
static void ShowDialog()
{
  RECT dialog_r, r;
  LVCOLUMN lvc;
  LVITEM lvi;
  HICON hIconInfo, hIconSearch, hIconDefault;
  HIMAGELIST hImageList;

  // Set dialog font to that of the parent window font.
  SendMessage(g_hDialog, WM_SETFONT, (WPARAM)SendMessage(g_hWndParent, WM_GETFONT, 0, 0), TRUE);

  // Get the sizes of the UI.
  GetWindowRect(GetDlgItem(g_hWndParent, IDC_BLACKRECT), &dialog_r);
  MapWindowPoints(HWND_DESKTOP, g_hWndParent, (LPPOINT)&dialog_r, 2);

  // Set our window size to fit the UI size.
  MoveWindow(
    g_hDialog,
    dialog_r.left,
    dialog_r.top,
    dialog_r.right - dialog_r.left,
    dialog_r.bottom - dialog_r.top,
    FALSE
  );

  g_hProgress = GetDlgItem(g_hDialog, IDC_PROGRESS);
  g_hList = GetDlgItem(g_hDialog, IDC_LIST);
  g_hNext = GetDlgItem(g_hWndParent, 1);
  g_hCancel = GetDlgItem(g_hWndParent, 2);
  g_hBack = GetDlgItem(g_hWndParent, 3);

  // Disable next button while searching.
  EnableWindow(g_hNext, FALSE);

  // Resize controls.
  HWND hHeading = GetDlgItem(g_hDialog, IDC_HEADING);
  GetWindowRect(hHeading, &r);
  SetWindowPos(hHeading, 0, 0, 0, dialog_r.right - dialog_r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
  GetWindowRect(g_hProgress, &r);
  SetWindowPos(g_hProgress, 0, 0, 0, dialog_r.right - dialog_r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
  GetWindowRect(g_hList, &r);
  MapWindowPoints(HWND_DESKTOP, g_hWndParent, (LPPOINT)&r, 2);
  SetWindowPos(g_hList, 0, 0, 0, dialog_r.right - dialog_r.left, dialog_r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);

  // Add columns.
  GetClientRect(g_hList, &r);
  lvc.mask = LVCF_TEXT | LVCF_WIDTH;
  lvc.cx = int((r.right - GetSystemMetrics(SM_CXHSCROLL))/3*2)-1;
  lvc.pszText = (g_apszParams[PARAM_COLHEADING_APPLICATION] && g_apszParams[PARAM_COLHEADING_APPLICATION][0] ? g_apszParams[PARAM_COLHEADING_APPLICATION] : TEXT_COLHEADING_1);
  ListView_InsertColumn(g_hList, 0, &lvc);
  lvc.cx = int((r.right - GetSystemMetrics(SM_CXHSCROLL))/3*1)-1;
  lvc.pszText = (g_apszParams[PARAM_COLHEADING_PROCESS] && g_apszParams[PARAM_COLHEADING_PROCESS][0] ? g_apszParams[PARAM_COLHEADING_PROCESS] : TEXT_COLHEADING_2);
  ListView_InsertColumn(g_hList, 1, &lvc);

  // Set heading text.
  SetWindowText(GetDlgItem(g_hDialog, IDC_HEADING), g_apszParams[PARAM_HEADING] ? g_apszParams[PARAM_HEADING] : TEXT_HEADING);

  // Full row select always.
  ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

  // Save Next button text.
  if (g_pszNext != NULL)
    GetWindowText(g_hNext, g_pszNext, g_stringsize);

  // Get info icon.
  if (g_fUserIcons)
    hIconInfo = (HICON)LoadImage(NULL, ICON_INFO, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE|LR_LOADFROMFILE);
  else
	  ExtractIconEx(TEXT("shell32.dll"), 15, NULL, &hIconInfo, 1);
  if (!hIconInfo)
    hIconInfo = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_INSTALLERICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);

  // Get searching icon.
  if (g_fUserIcons)
    hIconSearch = (HICON)LoadImage(NULL, ICON_SEARCH, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE|LR_LOADFROMFILE);
  else
	  ExtractIconEx(TEXT("shell32.dll"), 22, NULL, &hIconSearch, 1);
  if (!hIconSearch)
    hIconSearch = hIconInfo;

  // Get default app icon.
	ExtractIconEx(TEXT("shell32.dll"), 2, NULL, &hIconDefault, 1);

  // Create the icons image list.
  hImageList = ImageList_Create(16, 16, ILC_MASK|ILC_COLOR32, 8, 56);

  // Add icons to the image list.
  ImageList_AddIcon(hImageList, hIconInfo);
  ImageList_AddIcon(hImageList, hIconSearch);
  ImageList_AddIcon(hImageList, hIconDefault);
  DestroyIcon(hIconInfo);
  if (hIconSearch != hIconInfo)
    DestroyIcon(hIconSearch);
  DestroyIcon(hIconDefault);

  // Set the image list.
  ListView_SetImageList(g_hList, hImageList, LVSIL_SMALL);

  // Add 'Searching, please wait...'
  lvi.mask = LVIF_PARAM | LVIF_TEXT | LVIF_IMAGE;
  lvi.iItem = 0;
  lvi.iSubItem = 0;
  lvi.pszText = (g_apszParams[PARAM_SEARCHING] ? g_apszParams[PARAM_SEARCHING] : TEXT_SEARCHING_0);
  lvi.lParam = NULL;
  lvi.iImage = 1;
  ListView_InsertItem(g_hList, &lvi);
  ListView_EnsureVisible(g_hList, 0, FALSE);

  ParentDlgProcOld = (DLGPROC)SetWindowLongPtr(g_hWndParent, DWLP_DLGPROC, (LONG_PTR)ParentDlgProc);
  ListWndProcOld = (WNDPROC)SetWindowLongPtr(g_hList, GWLP_WNDPROC, (LONG_PTR)ListWndProc);

  // Tell NSIS to remove old inner dialog and pass handle of the new inner dialog.
  SendMessage(g_hWndParent, WM_NOTIFY_CUSTOM_READY, (WPARAM)g_hDialog, 0);
  ShowWindow(g_hDialog, SW_SHOW);

  g_fDone = g_fMonitoringDone = FALSE;

  // Begin the thread of doom.
  g_hThreadFiles = CreateThread(NULL, 0, FilesThread, NULL, 0, NULL);

  // Loop until the user clicks on a button.
  while (!g_fDone || !g_fMonitoringDone)
  {
    MSG msg;
    if (GetMessage(&msg, NULL, 0, 0) && !IsDialogMessage(g_hDialog, &msg) && !IsDialogMessage(g_hWndParent, &msg))
    {
      if (msg.message == WM_QUIT)
        break;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  CloseHandle(g_hThreadFiles);
  g_hThreadFiles = NULL;

  // Free list view item params.
  lvi.iItem = ListView_GetItemCount(g_hList);
  lvi.mask = LVIF_PARAM;
  while (--lvi.iItem >= 0)
  {
    ListView_GetItem(g_hList, &lvi);
    if (lvi.lParam != NULL)
    {
      LISTITEM_INFO* pListItemInfo = (LISTITEM_INFO*)lvi.lParam;
      GlobalFree((HGLOBAL)pListItemInfo->lpData);
      GlobalFree(pListItemInfo);
    }
  }

  // Return page button result.
  if (g_fIgnoreClicked)
    pushstring(OUT_IGNORE);
  else if (g_fCancelClicked)
    pushstring(OUT_CANCEL);
  else if (g_fBackClicked)
    pushstring(OUT_BACK);
  else if (g_apszParams[PARAM_IGNORE] && g_uiProcessCount > 0)
    pushstring(OUT_IGNORE);
  else
    pushstring(OUT_NEXT);

  // Set window dialog procedure back to NSIS's.
  SetWindowLongPtr(g_hList, GWLP_WNDPROC, (LONG_PTR)ListWndProcOld);
  SetWindowLongPtr(g_hWndParent, DWLP_DLGPROC, (LONG_PTR)ParentDlgProcOld);
  DestroyWindow(g_hDialog);

  // Done.
  ResetState();
}

// Plugin callback for new plugin API.
static UINT_PTR PluginCallback(enum NSPIM msg)
{
  if (msg == NSPIM_UNLOAD)
    FreeGlobalAllocs();
  return 0;
}

// NSIS Function: Add a file.
NSISFUNC(AddFile)
{
  DLL_INIT();

  if (g_uiFilesCount < MAX_LIST_COUNT)
  {
    g_apszFilesList[g_uiFilesCount] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*MAX_PATH);
    popstring(g_apszFilesList[g_uiFilesCount]);
    CharLower(g_apszFilesList[g_uiFilesCount]);
    g_uiFilesCount++;
  }
  else
    pushstring(OUT_ERROR);
}

// NSIS Function: Add a module.
NSISFUNC(AddModule)
{
  DLL_INIT();

  if (g_uiModulesCount < MAX_LIST_COUNT)
  {
    g_apszModulesList[g_uiModulesCount] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*MAX_PATH);
    popstring(g_apszModulesList[g_uiModulesCount]);
    CharLower(g_apszModulesList[g_uiModulesCount]);
    g_uiModulesCount++;
  }
  else
    pushstring(OUT_ERROR);
}

// NSIS Function: Add a window class.
NSISFUNC(AddClass)
{
  DLL_INIT();

  if (g_uiClassesCount < MAX_LIST_COUNT)
  {
    g_apszClassesList[g_uiClassesCount] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*MAX_PATH);
    popstring(g_apszClassesList[g_uiClassesCount]);
    CharLower(g_apszClassesList[g_uiClassesCount]);
    g_uiClassesCount++;
  }
  else
    pushstring(OUT_ERROR);
}

// NSIS Function: Add a window caption.
NSISFUNC(AddCaption)
{
  DLL_INIT();

  if (g_uiCaptionsCount < MAX_LIST_COUNT)
  {
    g_apszCaptionsList[g_uiCaptionsCount] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*MAX_PATH);
    popstring(g_apszCaptionsList[g_uiCaptionsCount]);
    CharLower(g_apszCaptionsList[g_uiCaptionsCount]);
    g_uiCaptionsCount++;
  }
  else
    pushstring(OUT_ERROR);
}

// NSIS Function: Show all open programs.
NSISFUNC(AddApplications)
{
  DLL_INIT();

  g_fShowRunningApplications = TRUE;
}

// NSIS Function: Add a custom item.
NSISFUNC(AddCustom)
{
  DLL_INIT();

  BOOL fSuccess = FALSE;
  if (g_uiCustomItemsCount < MAX_LIST_COUNT)
  {
    PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);

    if (popstring(pszParam) == 0)
    {
      g_aCustomItemsList[g_uiCustomItemsCount].pszApplication = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);
      g_aCustomItemsList[g_uiCustomItemsCount].pszProcess = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*g_stringsize);

      // Get custom item icon.
      if (lstrcmpi(pszParam, TEXT("/icon")) == 0)
      {
        if (popstring(pszParam) == 0)
        {
          // Icon: LoadImage() otherwise ExtractIconEx().
          g_aCustomItemsList[g_uiCustomItemsCount].hIcon = NULL;
          if (EndsWith(pszParam, TEXT(".ico")))
            g_aCustomItemsList[g_uiCustomItemsCount].hIcon = (HICON)LoadImage(NULL, pszParam, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE|LR_LOADFROMFILE);
          else
            ExtractIconEx(pszParam, 0, NULL, &g_aCustomItemsList[g_uiCustomItemsCount].hIcon, 1);

          // Get custom item application.
          if (popstring(g_aCustomItemsList[g_uiCustomItemsCount].pszApplication) == 0)
            fSuccess = TRUE;
        }
      }
      else
      {
        // Get custom item application.
        lstrcpy(g_aCustomItemsList[g_uiCustomItemsCount].pszApplication, pszParam);
        fSuccess = TRUE;
      }

      // So far so good?
      if (fSuccess)
      {
        // Get custom item process and function address.
        if (popstring(g_aCustomItemsList[g_uiCustomItemsCount].pszProcess) == 0 &&
          popstring(pszParam) == 0 &&
          (g_aCustomItemsList[g_uiCustomItemsCount].iFunctionAddress = myatoi(pszParam)-1) > 0)
        {
          g_uiCustomItemsCount++;
          g_pExtraParameters = extra;
        }
        else
          fSuccess = FALSE;
      }
      
      // Something went wrong. Free allocated memory.
      if (!fSuccess)
      {
        GlobalFree(g_aCustomItemsList[g_uiCustomItemsCount].pszApplication);
        GlobalFree(g_aCustomItemsList[g_uiCustomItemsCount].pszProcess);
      }
    }

    GlobalFree(pszParam);
  }

  if (!fSuccess)
    pushstring(OUT_ERROR);
}

// NSIS Function: Add a folder.
NSISFUNC(AddFolder)
{
  DLL_INIT();

  if (g_uiFoldersCount < MAX_LIST_COUNT)
  {
    g_apszFoldersList[g_uiFoldersCount] = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*MAX_PATH);
    popstring(g_apszFoldersList[g_uiFoldersCount]);
    CharLower(g_apszFoldersList[g_uiFoldersCount]);
    lstrcat(g_apszFoldersList[g_uiFoldersCount], TEXT("\\"));
    g_uiFoldersCount++;
  }
  else
  {
    pushstring(OUT_ERROR);
  }
}

// NSIS Function: Create dialog but do not show it.
NSISFUNC(InitDialog)
{
  DLL_INIT();

  if (!EnableDebugPriv())
  {
    pushstring(OUT_ERROR);
    return;
  }

  if (g_hDialog == NULL)
  {
    LoadDialog();

    // Return page HWND.
    TCHAR szHWND[32];
    wsprintf(szHWND, TEXT("%d"), g_hDialog);
    pushstring(szHWND);
  }
  else
  {
    pushstring(OUT_ERROR);
  }
}

// NSIS Function: Display the initialised dialog.
NSISFUNC(Show)
{
  if (g_hDialog != NULL)
    ShowDialog();
  else
    pushstring(OUT_ERROR);
}

// NSIS Function: Display the dialog.
NSISFUNC(Dialog)
{
  if (!EnableDebugPriv())
  {
    pushstring(OUT_ERROR);
    return;
  }

  g_hWndParent = hWndParent;
  EXDLL_INIT();

  LoadDialog();
  ShowDialog();
}

// NSIS Function: Search without a dialog.
NSISFUNC(SilentSearch)
{
  DLL_INIT();

  if (!EnableDebugPriv() || g_hThreadFiles != NULL)
  {
    pushstring(OUT_ERROR);
    return;
  }

  PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*string_size);

  if (popstring(pszParam) == 0)
  {
    BOOL fAsync = FALSE;
    int iFunctionAddress = 0;

    if (lstrcmpi(pszParam, TEXT("/async")) == 0)
    {
      fAsync = TRUE;
      if (popstring(pszParam) == 0)
        iFunctionAddress = myatoi(pszParam)-1;
    }
    else
    {
      iFunctionAddress = myatoi(pszParam)-1;
    }

    if (iFunctionAddress > 0)
    {
      g_pExtraParameters = extra;

      if (fAsync)
      {
        g_hThreadFiles = CreateThread(NULL, 0, FilesThreadSilent, (LPVOID)iFunctionAddress, 0, NULL);
        pushstring(OUT_OK);
      }
      else
      {
        pushstring(FilesThreadSilent((LPVOID)iFunctionAddress) ? OUT_DONE : OUT_CANCEL);
      }
    }
    else
    {
      pushstring(OUT_ERROR);
    }
  }
  else
  {
    pushstring(OUT_ERROR);
  }

  GlobalFree(pszParam);
}

// NSIS Function: Wait for thread to finish.
NSISFUNC(SilentWait)
{
  EXDLL_INIT();

  if (g_hThreadFiles)
  {
    BOOL fWait = TRUE;
    PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*string_size);

    if (popstring(pszParam) == 0)
    {
      // Wait for so many milliseconds.
      if (lstrcmpi(pszParam, TEXT("/time")) == 0)
      {
        fWait = FALSE;
        popstring(pszParam);

        if (g_hThreadFiles != NULL)
        {
          if (WaitForSingleObject(g_hThreadFiles, myatoi(pszParam)) == WAIT_TIMEOUT)
          {
            pushstring(OUT_WAIT);
          }
          else
          {
            DWORD dwExitCode = -1;
            GetExitCodeThread(g_hThreadFiles, &dwExitCode);
            pushstring(dwExitCode == -1 ? OUT_ERROR : dwExitCode == FALSE ? OUT_CANCEL : OUT_DONE);
          }
        }
      }
      else
      {
        pushstring(pszParam);
      }
    }

    GlobalFree(pszParam);

    // Wait until the thread has finished.
    if (fWait)
    {
      WaitForSingleObject(g_hThreadFiles, INFINITE);
      CloseHandle(g_hThreadFiles);
      g_hThreadFiles = NULL;
    }
  }
  else
  {
    pushstring(OUT_ERROR);
  }
}

// NSIS Function: What is the % complete?
NSISFUNC(SilentPercentComplete)
{
  EXDLL_INIT();

  if (g_hThreadFiles)
  {
    TCHAR szPercentComplete[4];
    wsprintf(szPercentComplete, TEXT("%u"), g_uiPercentComplete);
    pushstring(szPercentComplete);
  }
  else
  {
    pushstring(OUT_ERROR);
  }
}

// NSIS Function: Tests if a file is in use.
NSISFUNC(IsFileLocked)
{
  EXDLL_INIT();

  PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*string_size);

  if (popstring(pszParam) == 0)
  {
    BOOL fLocked = FALSE;

    HANDLE hFile = CreateFile(pszParam, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
      DWORD dwError = GetLastError();
      if (dwError != ERROR_PATH_NOT_FOUND && dwError != ERROR_FILE_NOT_FOUND)
        fLocked = TRUE;
    }
    else
    {
      CloseHandle(hFile);
    }

    pushstring(fLocked ? TEXT("true") : TEXT("false"));
  }

  GlobalFree(pszParam);
}

// NSIS Function: Finds a process by name.
NSISFUNC(FindProcess)
{
  EXDLL_INIT();

  if (!EnableDebugPriv())
  {
    pushstring(OUT_ERROR);
    return;
  }

  PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*string_size);
  if (pszParam)
  {
    BOOL fYesNo = FALSE;

    if (popstring(pszParam) == 0)
    {
      if (lstrcmpi(pszParam, TEXT("/yesno")) == 0)
      {
        fYesNo = TRUE;

        if (popstring(pszParam) != 0)
        {
          GlobalFree(pszParam);
          return;
        }
      }

      if (*pszParam)
      {
        CharLower(pszParam);

        // Add a forward stroke to the front.
        int len = lstrlen(pszParam);
        for (int i = len; i > 0; i--)
          pszParam[i] = pszParam[i-1];
        pszParam[0] = TEXT('\\');

        FIND_PROCESS_PROC_ARGS stArgs;
        stArgs.fSuccess = FALSE;
        stArgs.fYesNo = fYesNo;
        stArgs.pszProcessExe = pszParam;

        ENUM_OPTIONS stOpt;
        stOpt.GetWindowCaption = FALSE;
        stOpt.lParam = (LPARAM)&stArgs;

        EnumSystemProcesses(FindProcessProc, &stOpt, FALSE);

        if (!stArgs.fSuccess)
        {
          if (fYesNo)
            pushstring(OUT_NO);
          else
            pushstring(TEXT(""));
        }
      }
      else
      {
        pushstring(OUT_ERROR);
      }
    }
    else
    {
      pushstring(OUT_ERROR);
    }

    GlobalFree(pszParam);
  }
}

// NSIS Function: Closes or terminates a process by name.
NSISFUNC(CloseProcess)
{
  EXDLL_INIT();

  if (!EnableDebugPriv())
  {
    pushstring(OUT_ERROR);
    return;
  }

  BOOL fSuccess = FALSE;
  PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*string_size);
  if (pszParam)
  {
    BOOL fKill = FALSE;

    if (popstring(pszParam) == 0)
    {
      if (lstrcmpi(pszParam, TEXT("/kill")) == 0)
      {
        fKill = TRUE;

        if (popstring(pszParam) != 0)
        {
          GlobalFree(pszParam);
          return;
        }
      }

      if (*pszParam)
      {
        CharLower(pszParam);

        // Add a forward stroke to the front.
        int len = lstrlen(pszParam);
        for (int i = len; i > 0; i--)
          pszParam[i] = pszParam[i-1];
        pszParam[0] = TEXT('\\');

        FIND_PROCESS_PROC_ARGS stArgs;
        stArgs.pszProcessExe = pszParam;
        stArgs.fYesNo = fKill;
        stArgs.fSuccess = FALSE;

        ENUM_OPTIONS stOpt;
        stOpt.GetWindowCaption = !fKill;
        stOpt.lParam = (LPARAM)&stArgs;

        EnumSystemProcesses(CloseProcessProc, &stOpt, FALSE);
        if (stArgs.fSuccess)
          fSuccess = TRUE;
      }
    }

    GlobalFree(pszParam);
  }
  
  pushstring(fSuccess ? OUT_OK : OUT_ERROR);
}

// NSIS Function: Enumerates all process names using a callback function.
NSISFUNC(EnumProcesses)
{
  EXDLL_INIT();
  
  if (!EnableDebugPriv())
  {
    pushstring(OUT_ERROR);
    return;
  }

  PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR)*string_size);
  
  if (popstring(pszParam) == 0)
  {
    g_pExtraParameters = extra;

    ENUM_OPTIONS stOpt;
    stOpt.GetWindowCaption = FALSE;
    stOpt.lParam = myatoi(pszParam) - 1;

    pushstring(EnumSystemProcesses(EnumSystemProcessesProc, &stOpt, FALSE) ? OUT_DONE : OUT_CANCEL);
  }
  else
    pushstring(OUT_ERROR);

  GlobalFree(pszParam);
}

#endif //_WIN64

// Entry point for DLL.
BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  g_hInstance = hInst;
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Ilya Kotelnikov> export DLL function to enum x64 modules and notify
//	the caller: 32-bit nsis process with the plugin loaded.
// Changes by Stuart Welch
//
// Call example: rundll32.exe LockedList64.dll,EnumSystemProcesses64 hWndCaller fGetWindowCaption fEnumModules
//
//		hWndCaller - handle to the callback window which runs inside the caller 32-bit process.
//    fGetWindowCaption - sets Boolean value of ENUM_OPTIONS.GetWindowCaption for EnumSystemProcesses() call.
//    fEnumModules - sets 3rd Boolean argument value for EnumSystemProcesses() call.
//
#ifdef _WIN64

BOOL CALLBACK EnumSystemProcesses64Proc(FILE_INFORMATION& file, LPARAM lParam)
{
	HWND hCallerWindow = (HWND)lParam;

	COPYDATASTRUCT cds;
	cds.dwData = 1;
	cds.cbData = sizeof(FILE_INFORMATION);
	cds.lpData = const_cast<FILE_INFORMATION*>(&file);
  return SendMessage(hCallerWindow, WM_COPYDATA, NULL, (LPARAM)(LPVOID)&cds) && IsWindow(hCallerWindow);
}

extern "C" void __declspec(dllexport) EnumSystemProcesses64W(HWND hwnd, HINSTANCE hinst, LPWSTR lpwszCmdLine, int nCmdShow)
{
  int nCmdLine = 0;
  LPWSTR* ppwszCmdLine = CommandLineToArgvW(lpwszCmdLine, &nCmdLine);

  if (nCmdLine > 0)
  {
    HWND hCallerWindow = (HWND)wcstoul(ppwszCmdLine[0], NULL, 0);
    if (IsWindow(hCallerWindow) && EnableDebugPriv())
    {
      BOOL fGetWindowCaption = nCmdLine > 1 && lstrcmpW(ppwszCmdLine[1], L"1") == 0;
      BOOL fEnumModules = nCmdLine > 2 && lstrcmpW(ppwszCmdLine[2], L"1") == 0;
      
      ENUM_OPTIONS opt;
      opt.GetWindowCaption = fGetWindowCaption;
      opt.lParam = (LPARAM)hCallerWindow;
      EnumSystemProcesses(EnumSystemProcesses64Proc, &opt, fEnumModules);
    }
  }
}

extern "C" void __declspec(dllexport) GetSystemModulesCount64W(HWND hwnd, HINSTANCE hinst, LPWSTR lpwszCmdLine, int nCmdShow)
{
  int nCmdLine = 0;
  LPWSTR* ppwszCmdLine = CommandLineToArgvW(lpwszCmdLine, &nCmdLine);

  if (nCmdLine > 0)
  {
    HWND hCallerWindow = (HWND)wcstoul(ppwszCmdLine[0], NULL, 0);
    if (IsWindow(hCallerWindow) && EnableDebugPriv())
    {
      UINT uCount = GetSystemModulesCount();

	    COPYDATASTRUCT cds;
	    cds.dwData = 2;
	    cds.cbData = sizeof(UINT);
	    cds.lpData = const_cast<UINT*>(&uCount);
      SendMessage(hCallerWindow, WM_COPYDATA, NULL, (LPARAM)(LPVOID)&cds) && IsWindow(hCallerWindow);
    }
  }
}

#else	//not _WIN64 code

static BOOL EnumSystemProcesses64(EnumCallback Callback, ENUM_OPTIONS* pOpt, BOOL fEnumModules)
{
  if (!IsRunningX64())
    return TRUE;

  ENUM64_OPTIONS pOpt64;
  pOpt64.Callback = Callback;
  pOpt64.Options = pOpt;

  TCHAR szArguments[8];
  wsprintf(szArguments, TEXT("%d %d"), pOpt->GetWindowCaption ? 1 : 0, fEnumModules ? 1 : 0);

  g_pEnumSystemProcesses64Options = &pOpt64;
  BOOL res = CallLockedList64(TEXT("EnumSystemProcesses64"), szArguments);
  g_pEnumSystemProcesses64Options = NULL;

  return res;
}

static UINT GetSystemModulesCount64()
{
  if (!IsRunningX64())
    return 0;

  g_uGetSystemModulesCount64 = 0;
  CallLockedList64(TEXT("GetSystemModulesCount64"), TEXT(""));
  return g_uGetSystemModulesCount64;
}

static BOOL CallLockedList64(PTCHAR pszFunction, PTCHAR pszArguments)
{
  //-- build path to LockedList64.dll and ensure it exists (and hence user wants to check x64 modules).
  TCHAR szThisFilePath[MAX_PATH];
  ZeroMemory(szThisFilePath, sizeof(szThisFilePath));
  GetModuleFileName(g_hInstance, szThisFilePath, MAX_PATH);
  PathRemoveFileSpec(szThisFilePath);

  //-- the caller doesn't want x64 support?
  LPCTSTR szPlugin64Name = TEXT("LockedList64.dll");
  PathAppend(szThisFilePath, szPlugin64Name);
  if (!PathFileExists(szThisFilePath))
    return TRUE;

  //-- create a callback window: we will be getting found files using it (one by one).
  HWND hCallbackWindow = CreateWindow(TEXT("STATIC"), TEXT("LockedList x64 Checker"), WS_POPUP, 0, 0, 100, 100, HWND_MESSAGE, NULL, g_hInstance, NULL);
  if (IsWindow(hCallbackWindow))
    CallbackWindow64ProcOld = (WNDPROC)SetWindowLongPtr(hCallbackWindow, GWLP_WNDPROC, (LONG_PTR)CallbackWindow64WndProc);

  //-- prepare rundll32 command line and execute it (pass the callback window handle as a argument).
  PTCHAR pszParams = (PTCHAR)LocalAlloc(LPTR, sizeof(TCHAR) * (lstrlen(szPlugin64Name) + lstrlen(pszFunction) + lstrlen(pszArguments) + 32));
  wsprintf(pszParams, TEXT("%s,%s 0x%p %s"), szPlugin64Name, pszFunction, hCallbackWindow, pszArguments);

  //-- get clear plugins folder: it will be current dir for the new process.
  PathRemoveFileSpec(szThisFilePath);

  SHELLEXECUTEINFO sei;
  ZeroMemory(&sei, sizeof(sei));
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS; //| SEE_MASK_FLAG_NO_UI;
  sei.hwnd = NULL;
  sei.lpFile = TEXT("rundll32.exe");
  sei.lpParameters = pszParams;
  sei.lpDirectory = szThisFilePath;
  sei.nShow = SW_SHOWNORMAL;

  if (ShellExecuteEx(&sei) && sei.hProcess)
  {
    //-- for the process end but handle all the thread messages (to make the callback window working).
    while (TRUE)
    {
      DWORD dwWaitRes = MsgWaitForMultipleObjects(1, &sei.hProcess, FALSE, INFINITE, QS_ALLINPUT);
      if (dwWaitRes <= WAIT_OBJECT_0 || dwWaitRes > WAIT_OBJECT_0 + 1)
        break;

      MSG msg;
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      dwWaitRes = WaitForMultipleObjects(1, &sei.hProcess, FALSE, 0);	//-- test the handle only also not to loop forever.
      if (dwWaitRes != WAIT_TIMEOUT)
        break;
    }

    CloseHandle(sei.hProcess);
  }

  if (IsWindow(hCallbackWindow))
  {
    SetWindowLongPtr(hCallbackWindow, GWLP_WNDPROC, (LONG_PTR)CallbackWindow64ProcOld);	
    DestroyWindow(hCallbackWindow);
  }
  
  LocalFree(pszParams);
  return TRUE;
}

// Callback window procedure for LockedList64.
LRESULT CALLBACK CallbackWindow64WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_COPYDATA:
    {
      COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;

      // Adds a 64-bit module to the list for EnumSystemProcesses64.
      if (pcds->dwData == 1 && pcds->cbData == sizeof(FILE_INFORMATION64) && g_pEnumSystemProcesses64Options)
      {
        FILE_INFORMATION64 *pfile = (FILE_INFORMATION64*)pcds->lpData;
        
        FILE_INFORMATION file;
        file.ProcessId = pfile->ProcessId;
        file.ProcessHWND = pfile->ProcessHWND;
        file.FileNumber = pfile->FileNumber;
        file.TotalFiles = pfile->TotalFiles;

#ifdef UNICODE
        lstrcpy(file.FullPath, pfile->FullPath);
        lstrcpy(file.ProcessDescription, pfile->ProcessDescription);
        lstrcpy(file.ProcessFullPath, pfile->ProcessFullPath);
#else
				WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, pfile->FullPath, lstrlenW(pfile->FullPath), file.FullPath, sizeof(file.FullPath), NULL, NULL);
				WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, pfile->ProcessDescription, lstrlenW(pfile->ProcessDescription), file.ProcessDescription, sizeof(file.ProcessDescription), NULL, NULL);
				WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, pfile->ProcessFullPath, lstrlenW(pfile->ProcessFullPath), file.ProcessFullPath, sizeof(file.ProcessFullPath), NULL, NULL);
#endif
        
        // Can inspect ->Options->Callback here if we want to call another callback function.
        return EnumLockedFilesProc(file, g_pEnumSystemProcesses64Options->Options->lParam);
      }

      // Saves the number of 64-bit modules for GetSystemModulesCount64.
      if (pcds->dwData == 2 && pcds->cbData == sizeof(UINT))
      {
        g_uGetSystemModulesCount64 = *(UINT*)pcds->lpData;
        return TRUE;
      }
    }
  }

  return CallWindowProc(CallbackWindow64ProcOld, hWnd, uMsg, wParam, lParam);
}

#endif //end of not _WIN64 code.
