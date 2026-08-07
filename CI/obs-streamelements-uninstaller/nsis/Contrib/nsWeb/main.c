///////////////////////////////////////////////////
// nsWeb: ///////////////////////////////////////// 
// plugin for displaying the Web Browser control //
// into a custom dialog or in a popup window //////
///////////////////////////////////////////////////

// Reduce our DLL with libctiny.lib
// Attention! this library is not portable (see readme.txt)
// You can remove this pragma if any errors
#pragma comment (lib, "libctiny.lib")
// Include Windows OS' main API
#include <windows.h>
// Include functions for the web browser control
#include <exdisp.h>
// Include function for Moniker, plus its library
#include <urlmon.h>
#pragma comment (lib, "urlmon.lib")
// Include WinInet API & library
#include <wininet.h>
#pragma comment (lib, "wininet.lib")
// Include the project resource header
#include "resource.h"
// Include the main.cpp's header
#include "main.h"
// Include NSIS' plugin header
#include "exdll.h"
// Include the web browser control header file
#include "nsWeb.h"

// Handle of the parent proc
WNDPROC WndProcOld;
// Handle of the instance
HINSTANCE hInst;
// Handle of the future child window
HWND hwndChild;
// Handle of our future dialog
HWND hWnd;
// Controlling the dialog behave
int done = 0;

// Procedure from our parent
BOOL CALLBACK ParentWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Process the messages from the parent and send those that we don't use
	BOOL bRes = CallWindowProc((WNDPROC)WndProcOld,hwnd,message,wParam,lParam);
	// don't know what, but this is when you go back, cancel or continue the installation,
	// close our dialog...
	if (message == WM_NOTIFY_OUTER_NEXT && !bRes)
	{
		// Update our handler...
		done++;
		// Notify the close message to our dialog and its controls...
		PostMessage(hWnd, WM_CLOSE, 0, 0);
	}
	return bRes;
}

// The dialog procedure
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			// Well... this is it
			EmbedBrowserObject(hDlg);
			return FALSE;
		}
	case WM_SIZE:
		{
			// Resize our browser to the inner child dialog
			ResizeBrowser(hDlg, LOWORD(lParam), HIWORD(lParam));
			return FALSE;
		}
	case WM_CLOSE:
		{
			// Remove the web browser resources
			UnEmbedBrowserObject(hDlg);
			return FALSE;
		}
	default:
		{
			// ... the other ones.....
			return FALSE;
		}
	}
	return TRUE;
}

// Function to detect an active internet connection
PLUGINFUNCTION(IsInet)
{
	// Var for handling the return value (to NSIS)
	char ret[5];
	// Var for handling the return value (from API function)
	DWORD iRet;
	// Add NSIS stuff ^_^
	EXDLL_INIT();
	// Pop it
	popstring(ret);
	// Call WinInet function:
	iRet = InternetAttemptConnect(0);
	// 0=No internet; 1=Yes internet :p
	setuservariable(_atoi(ret), (iRet != ERROR_SUCCESS ? "0" : "1"));
	// free the small string?
	ret[0] = 0;
	return;
}

// Function to create the web browser control:
// Will display an URL or HTML file
PLUGINFUNCTION(ShowWebInPage)
{
	// Struct of the dimensions of the inner dialog
	RECT r;
	// Handle of our future main dialog
	HWND hwnd;
	// Var for the user inputed URL, HTML file or plain HTML text
	char* sUrl;
	// "Just to be sure", allocate into its own memory block
	sUrl = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);
	// Add NSIS stuff =P
	EXDLL_INIT();
	// sUrl: Pop it
	popstring(sUrl);
	// Init the OLE thingy
	OleInitialize(NULL);
	// Get the handle of the child window from the parent's
	hwndChild = GetDlgItem(hWndParent, 1018);
	// Failed!! Do nothing:
	if (!hwndChild) return;
	// Function continues and call API to create our inner dialog:
	hwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWndParent, DlgProc);
	// Failed to create our inner dialog
	if (!hwnd)
	{
		// Notify the failure to the global handler
		done = 1;
	}
	else
	{
		// Notify the success thing
		done = 0;
		// Match with the parent's procedure
		WndProcOld = (WNDPROC)SetWindowLong(hWndParent, DWL_DLGPROC, (LONG)ParentWndProc);
		// Tell NSIS to remove old inner dialog and pass handle of the new inner dialog
		SendMessage(hWndParent, WM_NOTIFY_CUSTOM_READY, (WPARAM)hwnd, 0);
		// Pass the global handle from our local one:
		hWnd = hwnd;
		// Get the dimensions from the child dialog:
		GetWindowRect(hwndChild, &r);
		MapWindowPoints(0, hWndParent, (LPPOINT) &r, 2);
		// Move our dialog to match the same from the parent one:
		MoveWindow(hwnd, r.left, r.top, r.right-r.left, r.bottom-r.top, FALSE);
		// Pass the url for the web browser
		DisplayHTMLPage(hwnd, sUrl);
		// Show and Update the dialog and its controls
		ShowWindow(hwnd, SW_SHOWNA);
		UpdateWindow(hwnd);
	}
	// Loop of our dialog's messages
	while (!done)
    {
		MSG msg;
		int nResult = GetMessage(&msg, NULL, 0, 0);
		if (!IsDialogMessage(hwnd, &msg) && !IsDialogMessage(hWndParent, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }

	// Remove our string from the memory block
	HeapFree(GetProcessHeap(), 0, (char*)sUrl);

	// Remove the web browser resources (if any)
	UnEmbedBrowserObject(hwnd);

	// Remove OLE resource:
	OleUninitialize();

	// Return to the parent its original proc
    SetWindowLong(hWndParent, DWL_DLGPROC, (LONG)WndProcOld);

	// Destroy our inner dialog
    DestroyWindow(hwnd);

	return;
}

// Function to create the web browser control:
// Will display plain HTML text
PLUGINFUNCTION(ShowHTMLInPage)
{
	// Struct of the dimensions of the inner dialog
	RECT r;
	// Handle of our future main dialog
	HWND hwnd;
	// Var for the user inputed URL, HTML file or plain HTML text
	char* sUrl;
	// "Just to be sure", allocate into its own memory block
	sUrl = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);
	// Add NSIS stuff =P
	EXDLL_INIT();
	// sUrl: Pop it
	popstring(sUrl);
	// Init the OLE thingy
	OleInitialize(NULL);
	// Get the handle of the child window from the parent's
	hwndChild = GetDlgItem(hWndParent, 1018);
	// Failed!! Do nothing:
	if (!hwndChild) return;
	// Function continues and call API to create our inner dialog:
	hwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWndParent, DlgProc);
	// Failed to create our inner dialog
	if (!hwnd)
	{
		// Notify the failure to the global handler
		done = 1;
	}
	else
	{
		// Notify the success thing
		done = 0;
		// Match with the parent's procedure
		WndProcOld = (WNDPROC)SetWindowLong(hWndParent, DWL_DLGPROC, (LONG)ParentWndProc);
		// Tell NSIS to remove old inner dialog and pass handle of the new inner dialog
		SendMessage(hWndParent, WM_NOTIFY_CUSTOM_READY, (WPARAM)hwnd, 0);
		// Pass the global handle from our local one:
		hWnd = hwnd;
		// Get the dimensions from the child dialog:
		GetWindowRect(hwndChild, &r);
		MapWindowPoints(0, hWndParent, (LPPOINT) &r, 2);
		// Move our dialog to match the same from the parent one:
		MoveWindow(hwnd, r.left, r.top, r.right-r.left, r.bottom-r.top, FALSE);
		// Pass the plain HTML string to the web browser
		DisplayHTMLStr(hwnd, sUrl);
		// Show and Update the dialog and its controls
		ShowWindow(hwnd, SW_SHOWNA);
		UpdateWindow(hwnd);
	}
	// Loop of our dialog's messages
	while (!done)
    {
		MSG msg;
		int nResult = GetMessage(&msg, NULL, 0, 0);
		if (!IsDialogMessage(hwnd, &msg) && !IsDialogMessage(hWndParent, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }

	// Remove our string from the memory block
	HeapFree(GetProcessHeap(), 0, (char*)sUrl);

	// Remove the web browser resources (if any)
	UnEmbedBrowserObject(hwnd);

	// Remove OLE resource:
	OleUninitialize();

	// Return to the parent its original proc
    SetWindowLong(hWndParent, DWL_DLGPROC, (LONG)WndProcOld);

	// Destroy our inner dialog
    DestroyWindow(hwnd);

	return;
}

// Function to display (via popup) an URL or HTML file
// Not plain html text!!!!
PLUGINFUNCTION(ShowWebInPopUp)
{
	// Handle of our future module for mshtml.dll
	HINSTANCE hMsHtml;
	// Var for the user inputed URL or HTML file
	char* sUrl;
	// "Just to be sure", allocate into its own memory block
	sUrl = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);
	// Add NSIS stuff =]
	EXDLL_INIT();
	// Pop it
	popstring(sUrl);
	// Load its library
	hMsHtml = LoadLibrary("MSHTML.DLL");
	// didn't fail
	if (hMsHtml)
	{
		// Function prototype...
		SHOWHTMLDIALOGFN* pfnShowHTMLDialog;
		// Get it from the mshtml.dll
		pfnShowHTMLDialog = (SHOWHTMLDIALOGFN*)GetProcAddress(hMsHtml, "ShowHTMLDialog");
		// Something were wrong?
		if (!pfnShowHTMLDialog) 
		{
			// just free the library resource
			FreeLibrary(hMsHtml);
		}
		// Ok, no errors.... 
		else
		{
			// Here is the stuff to display the dialog html
			IMoniker* moniker = NULL;
			// But first, convert the string into Unicode
			BSTR wUrl = CreateUnicodeStr(sUrl);
			// Moniker thing :/
			if (CreateURLMoniker(NULL, wUrl, &moniker) != S_OK)
			{
				// Since the thing failed, free the stuff...
				// both unicode and library resources
				FreeUnicodeStr(wUrl);
				FreeLibrary(hMsHtml);
			}
			// Finally... show them :)
			pfnShowHTMLDialog(NULL, moniker, NULL, NULL, NULL);
			// I'll free everything
			FreeUnicodeStr(wUrl);
			FreeLibrary(hMsHtml);
		 }
	}
	// Remove our string from the memory block
	HeapFree(GetProcessHeap(), 0, (char*)sUrl);
	return;
}

BOOL WINAPI DllMain(HANDLE hInstance, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	// Asign the current instance into our global handle
	hInst = hInstance;
	return TRUE;
}
