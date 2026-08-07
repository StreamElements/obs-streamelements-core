~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  LockedList plug-in 3.0.0.4 by Afrow UK

   An NSIS plug-in to display or get a list of 32-bit programs that are
   locking a selection of files that have to be uninstalled or
   overwritten.

   Last build: 19th April 2015

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  ANSI NSIS build:
    Extract Plugins\LockedList.dll to your NSIS\Plugins folder.

  Unicode NSIS build:
    Extract Unicode\Plugins\LockedList.dll to your NSIS\Unicode\Plugins
    folder.

  See Examples\LockedList for an example of use.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 More information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  The main plug-in functions loop through system handles and process
  modules to find a file that your installer has to overwrite or delete.

  As of v0.4, LockedList supports listing of currently open
  applications.

  As of v0.7, the plug-in also supports listing of applications from
  window classes and captions.

  As of v0.9, a Unicode build is included.

  As of v3.0.0.0, 64-bit module enumeration (for ::AddModule) is now
  supported. Simply extract a copy of LockedList64.dll to $PLUGINSDIR
  on 64-bit machines (see the LockedListKernel32.nsi example script).
  The same LockedList64.dll is used for both ANSI and Unicode
  installers.

  For the LockedList dialog, processes on the list will be removed when
  they have been closed or terminated, enabling the Next button if the
  list becomes empty.

  LockedList also has support for silent installers with NSIS stack
  interaction as opposed to using a dialog. Silent searching can also be
  performed asynchronously so that other tasks can be performed while
  the plug-in searches (such as a progress bar).

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Known issues/limitations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  * The plug-in uses API's only available on Windows NT4 and upwards and
    therefore you cannot use the plug-in on a version of Windows that is
    older than Windows NT4.

    WinVer.nsh will help in this area...

    !include WinVer.nsh
    ...

      ${If} ${AtLeastWinNt4}
        ; Call LockedList plugin.
      ${EndIf}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Adding paths of locked files or modules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  These functions must be called before displaying the dialog
  or performing a silent search.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::AddFile "path\to\myfile.ext"

  This adds an ordinary file. These files are searched for case
  insensitively by enumerating open file handles.

  String matching is done from the end of the string, therefore you can
  also specify just the file name (with a leading backstroke) like so:
    "\myfile.ext"

  See Examples\LockedList\LockedListTest.nsi for an example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::AddModule "path\to\mylibrary.dll"
  LockedList::AddModule "path\to\mycontrol.ocx"
  LockedList::AddModule "path\to\myapp.exe"

  This adds a module file. This includes DLLs, OCXs and EXEs. These
  files are searched for case insensitively by enumerating running
  process modules. To enumerate 64-bit DLLs and OCXs you must extract
  LockedList64.dll to $PLUGINSDIR first.

  String matching is done from the end of the string, therefore you can
  also specify just the file name (with a leading backstroke) like so:
    "\mylibrary.dll"

  See Examples\LockedList\LockedListShell32.nsi for an example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::AddFolder "path\to\myfolder"

  This adds a folder, causing both files and modules to be enumerated.
  Please use carefully as this can result in many processes being found.

  See Examples\LockedList\LockedListFolder.nsi for a (bad) example

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::AddClass "class_with_wildcards"

  This adds an application by window class. You can use wildcards such
  as * and ? for searching.

  See Examples\LockedList\LockedListClass.nsi for an example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::AddCaption "caption_with_wildcards"

  This adds an application by window caption/title. You can use
  wildcards such as * and ? for searching.

  See Examples\LockedList\LockedListCaption.nsi for an example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::AddApplications

  Adds all applications currently running.

  See Examples\LockedList\LockedListApplications.nsi for an example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  GetFunctionAddress $CallbackFunctionAddress AddCustomCallback
  LockedList::AddCustom [/icon "path\to\file.ext"] "application_name" \
                        "process_name" $CallbackFunctionAddress

  Adds a custom item to the list with a callback function. The callback
  function is used to check if the custom item should remain listed or
  not.

  /icon specifies the full path to an icon to use on the list. It can be
  an icon file (.ico) or resource (.exe, .dll).

  "application_name" is what will be displayed under the Application
  list box column for the custom item. "process_name" will be displayed
  under the Process column.

  $CallbackFunctionAddress is any variable containing the callback
  function's address, which is retreived by using GetFunctionAddress.

  The plug-in will Push "process_name" onto the stack before calling the
  callback function. The callback function must Push "true" if the
  custom item should remain listed or Push "false" otherwise. You can
  also use the LockedList::IsFileLocked function inside your callback
  which pushes the correct stack values.

  See Examples\LockedList\LockedListCustom.nsi for an example.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Displaying the search dialog
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  LockedList::Dialog [optional_params]
   Pop $Var

  This is the normal way to display the dialog.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::InitDialog [optional_params]
   Pop $HWNDVar
  LockedList::Show
   Pop $Var

  This method allows you to modify controls on the dialog with
  SendMessage, SetCtlColors etc by using the $HWNDVar between
  the InitDialog and Show calls and also in the page's leave
  function.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  At this point, $Var will contain "error" on display error,
  "next" if the next button was pressed,
  "back" if the back button was pressed or
  "cancel" if the cancel button was pressed.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  These [optional_params] apply to both LockedList::Dialog and
  LockedList::InitDialog. The parameter names are case insensitive.

  /heading    "text"  - Set page heading text.

  /caption    "text"  - Set dialog caption text.

  /colheadings        - Set the column heading texts in the processes
   "application_text"   list. An empty string `` will use the default
   "process_text"       text.

  /noprograms "text"  - Item text when no programs to be closed are
                        running.

  /searching  "text"  - Item text while searching is in progress.

  /endsearch  "text"  - Item text when user clicks back or cancel
                        during a search.

  /endmonitor "text"  - Item text when user clicks back or cancel after
                        a search (at which point the list of programs is
                        being monitored for closing).

  /usericons          - Program will use icons "search.ico" and
                        "info.ico" in the current working directory
                        instead of using icons from shell32.dll for the
                        searching list. If no icons are found, the
                        installer icon is used.

  /ignore             - Allow the user to click Next even if there are
   "next_button_text"   items on the list. "next_button_text" sets the
                        Next button text. Use "" to use the default
                        Next button text. This parameter is ignored if
                        /autoclose or /autoclosesilent is used.

  /autoclose          - Close all running processes on exit with the
   "close_text"         confirmation message box "close_text".
   "kill_text"          Processes that cannot be closed safely with
   "failed_text"        WM_CLOSE are killed with the confirmation
   "next_button_text"   message box "kill_text". If processes are still
                        running then the "failed_text" message box is
                        displayed. An empty string "" will use the
                        default text. "next_button_text" sets the Next
                        button text. Use "" to use the default Next
                        button text.

  /autoclosesilent    - Same as the above switch except the close and
   "failed_text"        kill confirmation boxes are not displayed. If
   "next_button_text"   some processes cannot be killed, the
                        "failed_text" message is still displayed and the
                        user is prevented from continuing with setup. An
                        empty string "" will use the default text.
                        "next_button_text" sets the Nextbutton text. Use
                        "" to use the default Next button text.

  /menuitems          - Sets the list context menu item texts.
   "close_text"
   "copy_list_text"

  /autonext           - Moves to the next page automatically if no
                        processes are found.

  Examples:

  LockedList::Dialog /caption `I like cheese` /heading `I do really`
   Pop $Var

  LockedList::Dialog /autoclose `` `` `Couldn't kill 'em all, oops!`
   Pop $Var

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Searching silently
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  GetFunctionAddress $AddrVar SilentSearchCallback
  LockedList::SilentSearch [/async] $AddrVar
   Pop $Var

  Begins the search silently using the given callback function. Specify
  /async to allow the search to commence asynchronously. You can then
  check the progress of the asynchronous search with the SilentWait and
  SilentPercentComplete functions listed below.

  $Var will contain "ok" if /async was used and the search started
  successfully. If not using /async, $Var will contain "done" on search
  completion or "cancel" on search cancellation.

  The callback function is given 3 stack items:
    Process id, full path, description.

  The callback function must push "true" to continue enumeration or
  "false" to cancel the search. Pushing "autoclose" will close the
  current process before continuing the search.

  An example:
    Function SilentSearchCallback
      Pop $R0 ; process id
      Pop $R1 ; file path
      Pop $R2 ; description
      ; do stuff here
      Push true ; continue enumeration
    FunctionEnd

  Note: If "autoclose" was pushed and the auto-close failed, the
  callback function will be called again with a process id of "-1". This
  can be used to display a message to the user, if required.

  See Examples\LockedList\LockedListTest.nsi for a full example.
  See Examples\LockedList\LockedListAutoCloseSilent.nsi for an 
  auto-close example.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Searching silently asynchronously
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  These can only be used after calling SilentSearch with /async (see
  above).

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::SilentWait [/time #]
   Pop $Var

  If SilentSearch /async was used, this function will wait until the
  thread has finished, or optionally, return in # milliseconds when
  using /time #.

  $Var will contain either "wait" or "done" depending on whether or not
  the searching has finished. If the search was cancelled (by pushing
  "false" in the callback function), $Var will be "cancel".

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::SilentPercentComplete
   Pop $Var

  $Var will contain the current completion percentage, i.e. 65 for 65%.
  This can be used in a progress message.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Other functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  LockedList::IsFileLocked "file_path"
   Pop $Var

  At this point, $Var is "true" or "false". This function can be used in
  the AddCustom callback function for example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  GetFunctionAddress $AddrVar EnumProcessesCallback
  LockedList::EnumProcesses $AddrVar
   Pop $Var

  Enumerates all running processes using a callback function.

  The callback function is given 3 stack items:
    Process id, full path, description.

  The callback function must push "true" to continue enumeration or
  "false" to cancel enumeration. $Var will contain "done" on search
  completion or "cancel" on search cancellation.

  See Examples\LockedList\LockedListEnumProcesses.nsi for an example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::FindProcess [/yesno] process.exe
   Pop $Var
   [Pop $Var2
    Pop $Var3]

  Finds a process by executable name (you must include the .exe). If you
  specify /yesno then the function will push "yes" or "no" onto the
  stack. Otherwise, by default, the function will place an empty string
  on the stack if no processes are found, or 3 stack items otherwise:
    Process id, full path, description.

  See Examples\LockedList\LockedListFindProcess.nsi for an example.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  LockedList::CloseProcess [/kill] process.exe
   Pop $Var

  Closes a process by executable name (you must include the .exe) by
  sending WM_CLOSE to the application main window. Specify /kill to
  terminate the process forcefully instead.

  See Examples\LockedList\LockedListCloseProcess.nsi for an example.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Change log
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
  * Increased FILE_INFORMATION.ProcessCaption to 1024 characters to fix
    buffer overflow crash.
  * Fixed IsFileLocked() failing if first plug-in call (EXDLL_INIT()
    missing).

  v1.2 - 2nd April 2010
  * Added 'ignore' dialog result if /ignore was used and there were
    programs running.
  * Added additional argument for /autoclose and /autoclosesilent to
    set Next button text
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
  * Fixed memory access violation in g_pszParams.
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

  v0.7 - 24th February 2008
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
    /autoclosesilent "failed_text". The /ignore switch can be used along
    with this to set the Next button text.
  * Added /colheadings "application_text" "process_text"

  v0.5 - 25th November 2007
  * Fixed memory leak causing crash when re-visiting dialog. Caused by
    duplicate call to GlobalFree on the same pointer.

  v0.4 - 27th September 2007
  * Module or file names can now be just the file name as opposed to
    the full path.
  * Folder paths are converted to full paths (some are short DOS paths)
    before comparison.
  * Fixed typo in AddModule function (ModulesCount>FilesCount). Thanks
    kalverson.
  * List view is now scrolled into view while items are added.
  * List changed to multiple columns.
  * Debug privileges were not being set under SilentSearch.
  * Added /ignore switch that prevents the Next button being disabled.
  * Added AddApplications to add all running applications to the list.
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
  * Fixed pointer error in FileList struct causing only first
    module/file added to be used.
  * Fixed caption repetition over multiple processes.
  * Fixed stack overflow in DlgProc. Special thanks, Roman Prysiazhniuk
    for locating the source.
  * Better percent complete indication.

  v0.1 - 10th July 2007
  * First build.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 License
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copyright (c) 2013 Afrow Soft Ltd

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; 
   you must not claim that you wrote the original software.
   If you use this software in a product, an acknowledgment in the
   product documentation would be appreciated but is not required.
2. Altered versions must be plainly marked as such,
   and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any distribution.