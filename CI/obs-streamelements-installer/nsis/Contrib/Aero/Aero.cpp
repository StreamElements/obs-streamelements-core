/*
	Aero NSIS plug-in by Stuart Welch <afrowuk@afrowsoft.co.uk>
*/

#include <windows.h>
#include "aero.h"
#include "pluginapi.h"

HANDLE  g_hInstance;
HWND    g_hWndParent, g_hBack, g_hNext, g_hCancel, g_hBrandingText;
PWCHAR  g_pwszBack, g_pwszNext, g_pwszCancel;
WNDPROC ParentDlgProcOld;
MARGINS g_margins;
RECT    g_rectAero, g_rectWindow;
RECT    g_rectBrandingText, g_rect1035, g_rect1045, g_rect1256, g_rectCancel, g_rectBack, g_rectNext;
HBRUSH  g_hbAero;
HTHEME  g_hWindowTheme, g_hButtonTheme;
BOOL    g_fDrawBrandingText, g_fDrawButtonText, g_fClassicUI, g_fRTL;

static UINT_PTR PluginCallback(enum NSPIM msg)
{
	if (msg == NSPIM_GUIUNLOAD)
	{
		if (g_hbAero != NULL)
			DeleteObject(g_hbAero);
		if (g_hWindowTheme != NULL)
			CloseThemeData(g_hWindowTheme);
		if (g_hButtonTheme != NULL)
			CloseThemeData(g_hButtonTheme);
		if (g_pwszBack != NULL)
			GlobalFree(g_pwszBack);
		if (g_pwszNext != NULL)
			GlobalFree(g_pwszNext);
		if (g_pwszCancel != NULL)
			GlobalFree(g_pwszCancel);
		if (BufferedPaintUnInit != NULL)
			BufferedPaintUnInit();
	}
	return 0;
}

#define GetButtonTextBuffer(hWnd) (hWnd == g_hBack ? g_pwszBack : hWnd == g_hCancel ? g_pwszCancel : g_pwszNext)

// Saves the given ANSI text to the given Unicode char buffer.
static void SaveButtonText(PTCHAR pszText, PWCHAR* ppwszBuffer)
{
	if (*ppwszBuffer != NULL)
		GlobalFree(*ppwszBuffer);

	int cchLen = lstrlen(pszText);
	*ppwszBuffer = (PWCHAR)GlobalAlloc(GPTR, sizeof(WCHAR) * (cchLen + 1));
	if (*ppwszBuffer)
#ifdef UNICODE
		lstrcpy(*ppwszBuffer, pszText);
#else
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pszText, cchLen, *ppwszBuffer, cchLen);
#endif
}

// Reloads the given Unicode char buffer with the text from the given window before clearing the given window's text.
static void RefreshSavedButtonText(HWND hWnd, PWCHAR* ppwszBuffer)
{
	if (*ppwszBuffer != NULL)
		GlobalFree(*ppwszBuffer);

	int cchLen = GetWindowTextLength(hWnd) + 1;
	*ppwszBuffer = (PWCHAR)GlobalAlloc(GPTR, sizeof(WCHAR) * cchLen);
	if (*ppwszBuffer)
	{
		GetWindowTextW(hWnd, *ppwszBuffer, cchLen);
		SetWindowText(hWnd, TEXT(""));
	}
}

static int GetButtonState(HWND hWnd)
{
	int iState;

	if (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_DISABLED)
		iState = PBS_DISABLED;
	else
		iState = PBS_NORMAL;

	return iState;
}

static void AeroDisable(HWND hWnd)
{
	// Restore control positions.
	SetWindowPos(g_hBrandingText, 0, g_rectBrandingText.left, g_rectBrandingText.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	SetWindowPos(g_hBack, NULL, g_rectBack.left, g_rectBack.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	SetWindowPos(g_hNext, NULL, g_rectNext.left, g_rectNext.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	SetWindowPos(g_hCancel, NULL, g_rectCancel.left, g_rectCancel.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	if (g_fClassicUI)
	{
		ShowWindow(GetDlgItem(hWnd, -1), SW_SHOW);
	}
	else
	{
		SetWindowPos(GetDlgItem(hWnd, 1035), NULL, g_rect1035.left, g_rect1035.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, 1045), NULL, g_rect1045.left, g_rect1045.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, 1256), NULL, g_rect1256.left, g_rect1256.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}

	DeleteObject(g_hbAero);
	g_hbAero = NULL;

	// Restore button texts to what we saved.
	if (g_fDrawButtonText)
	{
		SetWindowTextW(g_hBack, g_pwszBack);
		SetWindowTextW(g_hNext, g_pwszNext);
		SetWindowTextW(g_hCancel, g_pwszCancel);
	}

	// Ensure Aero is disabled completely by using 0 margins.
	MARGINS m;
	m.cxLeftWidth = m.cxRightWidth = m.cyBottomHeight = m.cyTopHeight = 0;
	DwmExtendFrameIntoClientArea(hWnd, &m);
}

static LRESULT CALLBACK Windows10ParentDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ERASEBKGND:
		
		FillRect((HDC)wParam, &g_rectWindow, GetSysColorBrush(COLOR_BTNFACE));
		FillRect((HDC)wParam, &g_rectAero, g_hbAero);
		return TRUE;
	
	case WM_CTLCOLORBTN:

		if ((HWND)lParam == g_hBack || (HWND)lParam == g_hNext || (HWND)lParam == g_hCancel)
			return (LRESULT)g_hbAero;
		break;

	case WM_CTLCOLORSTATIC:

		if (g_fDrawBrandingText && (HWND)lParam == g_hBrandingText)
			return (LRESULT)g_hbAero;
		break;
	}

	return CallWindowProc(ParentDlgProcOld, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK ParentDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DWMCOMPOSITIONCHANGED:

		BOOL fAero;
		if (SUCCEEDED(DwmIsCompositionEnabled(&fAero)) && fAero)
		{      
			// Hide/reposition controls when Aero is enabled.
			SetWindowPos(GetDlgItem(hWnd, 1028), NULL, 0, g_rectAero.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			SetWindowPos(g_hNext, NULL, g_rectNext.left + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_X : MODERN_UI_BUTTON_NUDGE_X), g_rectNext.top + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_Y : MODERN_UI_BUTTON_NUDGE_Y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			SetWindowPos(g_hCancel, NULL, g_rectCancel.left - (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_X : MODERN_UI_BUTTON_NUDGE_X), g_rectCancel.top + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_Y : MODERN_UI_BUTTON_NUDGE_Y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			SetWindowPos(g_hBack, NULL, g_rectBack.left + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_X : MODERN_UI_BUTTON_NUDGE_X), g_rectBack.top + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_Y : MODERN_UI_BUTTON_NUDGE_Y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			if (g_fClassicUI)
			{
				ShowWindow(GetDlgItem(hWnd, -1), SW_HIDE);
			}
			else
			{
				SetWindowPos(GetDlgItem(hWnd, 1035), NULL, 0, g_rectAero.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
				SetWindowPos(GetDlgItem(hWnd, 1045), NULL, 0, g_rectAero.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
				SetWindowPos(GetDlgItem(hWnd, 1256), NULL, 0, g_rectAero.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}

			// Refresh our saved button texts.
			if (g_fDrawButtonText)
			{
				RefreshSavedButtonText(g_hBack, &g_pwszBack);
				RefreshSavedButtonText(g_hNext, &g_pwszNext);
				RefreshSavedButtonText(g_hCancel, &g_pwszCancel);
			}

			g_hbAero = CreateSolidBrush(COLOR_AERO);
			DwmExtendFrameIntoClientArea(hWnd, &g_margins);
			InvalidateRect(hWnd, NULL, TRUE);
		}
		else
		{
			AeroDisable(hWnd);
		}

	// Fall through on purpose: OpenThemeData can fail on WM_DWMCOMPOSITIONCHANGED.
	case WM_THEMECHANGED:
    
		if (g_fDrawBrandingText)
		{
			if (g_hWindowTheme != NULL)
				CloseThemeData(g_hWindowTheme);
			g_hWindowTheme = OpenThemeData(hWnd, THEME_WINDOW);

			// If no theme loaded, don't draw branding text.
			if (g_hWindowTheme == NULL)
				g_fDrawBrandingText = FALSE;
		}

		if (g_hButtonTheme != NULL)
			CloseThemeData(g_hButtonTheme);
		g_hButtonTheme = OpenThemeData(hWnd, THEME_BUTTON);

		// We need the button theme; disable Aero if we don't have it.
		if (g_hButtonTheme == NULL)
			AeroDisable(hWnd);

		return FALSE;

	case WM_ERASEBKGND:

		if (g_hbAero == NULL)
			break;

		FillRect((HDC)wParam, &g_rectWindow, GetSysColorBrush(COLOR_BTNFACE));
		FillRect((HDC)wParam, &g_rectAero, g_hbAero);
        
		if (g_fDrawBrandingText)
		{
			int cchLen = GetWindowTextLength(g_hBrandingText) + 1;
			PWCHAR pwszBrandingText = (PWCHAR)GlobalAlloc(GPTR, sizeof(WCHAR) * cchLen);
			if (pwszBrandingText)
			{
				GetWindowTextW(g_hBrandingText, pwszBrandingText, cchLen);
        
				if (g_fRTL)
					SetLayout((HDC)wParam, LAYOUT_RTL);

				HDC hDC = CreateCompatibleDC((HDC)wParam);
				if (SaveDC(hDC) != 0)
				{
					BITMAPINFO dib;
					dib.bmiHeader.biSize = sizeof(BITMAPINFO);
					dib.bmiHeader.biHeight = -g_margins.cyBottomHeight;
					dib.bmiHeader.biWidth = g_rectWindow.right;
					dib.bmiHeader.biPlanes = 1;
					dib.bmiHeader.biBitCount = 32;
					dib.bmiHeader.biCompression = BI_RGB;

					HBITMAP hBitmap = CreateDIBSection(hDC, &dib, DIB_RGB_COLORS, NULL, NULL, 0);
					if (hBitmap != NULL)
					{
						DTTOPTS dto;
						dto.dwSize = sizeof(DTTOPTS);
						dto.dwFlags = DTT_COMPOSITED;

						RECT r;
						r.left = g_rectBrandingText.left;
						r.top = 0;
						r.right = g_rectBrandingText.right;
						r.bottom = g_margins.cyBottomHeight;

						HBITMAP hBitmapOld = (HBITMAP)SelectObject(hDC, hBitmap);
						HFONT hFontOld = (HFONT)SendMessage(g_hBrandingText, WM_GETFONT, 0, NULL);
						if (hFontOld) hFontOld = (HFONT)SelectObject(hDC, hFontOld);

						DrawThemeTextEx(g_hWindowTheme, hDC, 0, 0, pwszBrandingText, -1, (g_fClassicUI ? DT_CENTER : 0) | DT_SINGLELINE | DT_VCENTER, &r, &dto);
						BitBlt((HDC)wParam, 0, g_rectAero.top, g_rectWindow.right, g_margins.cyBottomHeight, hDC, 0, 0, SRCCOPY | CAPTUREBLT);

						SelectObject(hDC, hBitmapOld);
						if (hFontOld) SelectObject(hDC, hFontOld);
						DeleteObject(hBitmap);
					}
        
					RestoreDC((HDC)wParam, -1);
					DeleteDC(hDC);
				}

				GlobalFree(pwszBrandingText);
			}
		}
    
		return TRUE;

	case WM_CTLCOLORBTN:

		if (g_hbAero != NULL && ((HWND)lParam == g_hBack || (HWND)lParam == g_hNext || (HWND)lParam == g_hCancel))
			return (LRESULT)g_hbAero;
	}

	return CallWindowProc(ParentDlgProcOld, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK ButtonWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (g_hbAero != NULL)
	{
		switch (uMsg)
		{
		case WM_SETTEXT:
    
			// Save the button text to our own buffer.
			if (g_fDrawButtonText)
			{
				SaveButtonText((PTCHAR)lParam, &GetButtonTextBuffer(hWnd));
				InvalidateRect(hWnd, NULL, TRUE);
				return FALSE;
			}
      
			InvalidateRect(hWnd, NULL, TRUE);
			break;

		case WM_ENABLE:
      
			InvalidateRect(hWnd, NULL, TRUE);
			return FALSE;

		case WM_ERASEBKGND:
    
			return TRUE;

		case WM_PAINT:
    
			PAINTSTRUCT ps;
			HDC hdcPaint = BeginPaint(hWnd, &ps);
			if (hdcPaint != NULL)
			{
				RECT r;
				GetClientRect(hWnd, &r);

				if (g_fRTL)
					SetLayout(hdcPaint, LAYOUT_RTL);

				HDC hdcBufferedPaint;
				HPAINTBUFFER hBufferedPaint = BeginBufferedPaint(hdcPaint, &r, BPBF_COMPOSITED, NULL, &hdcBufferedPaint);
				if (hBufferedPaint != NULL)
				{
					PatBlt(hdcBufferedPaint, 0, 0, r.right, r.bottom, BLACKNESS);

					// To fix text flicker we handle button text drawing ourselves.
					if (g_fDrawButtonText)
					{
						SendMessage(hWnd, WM_PRINTCLIENT, (WPARAM)hdcBufferedPaint, PRF_CLIENT);
        
						HFONT hFontOld = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, NULL);
						if (hFontOld) hFontOld = (HFONT)SelectObject(hdcBufferedPaint, hFontOld);
              
						DTTOPTS dto;
						dto.dwSize = sizeof(DTTOPTS);
						dto.dwFlags = DTT_COMPOSITED | DTT_GLOWSIZE;
						dto.iGlowSize = 12; // Button's text otherwise has no glow.
						DrawThemeTextEx(g_hButtonTheme, hdcBufferedPaint, BP_PUSHBUTTON, GetButtonState(hWnd), GetButtonTextBuffer(hWnd), -1, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_HIDEPREFIX, &r, &dto);

						if (hFontOld) SelectObject(hdcBufferedPaint, hFontOld);
					}
					// Let the button draw its text.
					else
					{
						SendMessage(hWnd, WM_PRINTCLIENT, (WPARAM)hdcBufferedPaint, PRF_CLIENT);
        
						RECT rContent;
						GetThemeBackgroundContentRect(g_hButtonTheme, NULL, BP_PUSHBUTTON, PBS_NORMAL, &r, &rContent);
						BufferedPaintSetAlpha(hBufferedPaint, &rContent, 255);
					}
        
					EndBufferedPaint(hBufferedPaint, TRUE);
				}

				EndPaint(hWnd, &ps);
			}

			return FALSE;
		}
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, PROP_AERO_WNDPROC), hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK BrandingTextWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SHOWWINDOW:
		g_fDrawBrandingText = (BOOL)wParam;
	}
  
	return CallWindowProc((WNDPROC)GetProp(hWnd, PROP_AERO_WNDPROC), hWnd, uMsg, wParam, lParam);
}

static BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam)
{
	if (GetWindowLong(hWnd, GWL_STYLE) & SS_ETCHEDHORZ)
	{
		*((HWND*)lParam) = hWnd;
		return FALSE;
	}

	return TRUE;
}

static BOOL GetMappedWindowRect(HWND hWnd, RECT* r)
{
	if (!IsWindow(hWnd) || !GetWindowRect(hWnd, r))
		return FALSE;
	MapWindowPoints(HWND_DESKTOP, g_hWndParent, (LPPOINT)r, 2);
	return TRUE;
}

NSISFUNC(Apply)
{
	DLL_INIT();

	// Plug-in already called?
	if (ParentDlgProcOld)
		return;

	DWORD dwMajorVersion = LOBYTE(LOWORD(GetVersion()));

	// Not Vista or above?
	if (dwMajorVersion < 6)
		return;
	
	g_fDrawBrandingText = TRUE;

	// Save common window handles.
	g_hBrandingText = GetDlgItem(hWndParent, 1028);
	g_hBack = GetDlgItem(hWndParent, 3);
	g_hCancel = GetDlgItem(hWndParent, 2);
	g_hNext = GetDlgItem(hWndParent, 1);

	// Windows 10 or above?
	if (dwMajorVersion >= 10)
	{
		// Should we draw the branding text?
		PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR) * string_size);
		if (pszParam)
		{
			while (popstring(pszParam) == 0)
			{
				if (lstrcmpi(pszParam, TEXT("/nobranding")) == 0)
				{
					g_fDrawBrandingText = FALSE;
				}
				else
				{
					pushstring(pszParam);
					break;
				}
			}
    
			GlobalFree(pszParam);
		}
		
		RECT r1, r2;
		
		HWND hWndCtl = GetDlgItem(g_hWndParent, 1044);
		if (IsWindow(hWndCtl))
		{
			if (!GetClientRect(hWndCtl, &r1) || !GetClientRect(g_hWndParent, &r2))
				goto cleanup;

			g_fClassicUI = FALSE;
		}
		else // Implement for non MUI?:
		{
			// Find the horizontal line static label.
			hWndCtl = GetDlgItem(g_hWndParent, -1);
			if (!IsWindow(hWndCtl))
			{
				hWndCtl = NULL;
				EnumChildWindows(g_hWndParent, EnumChildProc, (LPARAM)&hWndCtl);
				if (!IsWindow(hWndCtl))
					goto cleanup;
			}

			if (!GetMappedWindowRect(hWndCtl, &r1) || !GetClientRect(g_hWndParent, &r2))
				goto cleanup;
			
			g_fClassicUI = TRUE;
		}
		
		g_rectAero.left = g_rectWindow.left = 0;
		g_rectAero.top = r1.bottom; g_rectWindow.top = 0;
		g_rectAero.right = g_rectWindow.right = r2.right;
		g_rectAero.bottom = r2.bottom; g_rectWindow.bottom = r1.bottom;

		if (g_fClassicUI)
		{
			SetWindowPos(hWndCtl, NULL, -2, r1.top, r2.right + 4, r1.bottom - r1.top, SWP_NOZORDER);
			
			if (g_fDrawBrandingText && GetMappedWindowRect(g_hBrandingText, &r1))
			{
				SetWindowPos(g_hBrandingText, NULL, r1.left, r1.top + MODERN_UI_BUTTON_NUDGE_Y_WINDOWS10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
			else
			{
				SetWindowPos(g_hBrandingText, NULL, 0, r2.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
		}
		else
		{
			hWndCtl = GetDlgItem(g_hWndParent, 1035);
			if (GetMappedWindowRect(hWndCtl, &r1))
			{
				SetWindowPos(hWndCtl, NULL, -2, r1.top, r2.right + 4, r1.bottom - r1.top, SWP_NOZORDER);
			}

			hWndCtl = GetDlgItem(g_hWndParent, 1045);
			if (GetMappedWindowRect(hWndCtl, &r1))
			{
				SetWindowPos(hWndCtl, NULL, -2, r1.top, r2.right + 4, r1.bottom - r1.top, SWP_NOZORDER);
			}
    
			hWndCtl = GetDlgItem(g_hWndParent, 1256);
			if (IsWindow(hWndCtl))
			{
				SetWindowPos(hWndCtl, NULL, 0, r2.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
			
			if (g_fDrawBrandingText)
			{
				GetClientRect(g_hBrandingText, &r1);
				int pos = (g_rectAero.bottom - g_rectAero.top) / 2 - r1.bottom / 2;

				if (GetMappedWindowRect(g_hBack, &r2))
				{
					SetWindowPos(g_hBrandingText, NULL, pos + MODERN_UI_BUTTON_NUDGE_X_WINDOWS10, g_rectAero.top + pos + MODERN_UI_BUTTON_NUDGE_Y_WINDOWS10, r2.left - pos - MODERN_UI_BUTTON_NUDGE_X_WINDOWS10, r1.bottom, SWP_NOZORDER);
				}
				else
				{
					SetWindowPos(g_hBrandingText, NULL, pos + MODERN_UI_BUTTON_NUDGE_X_WINDOWS10, g_rectAero.top + pos + MODERN_UI_BUTTON_NUDGE_Y_WINDOWS10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
				}
			}
			else
			{
				SetWindowPos(g_hBrandingText, NULL, 0, r2.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
		}

		if (GetMappedWindowRect(g_hNext, &r1))
		{
			SetWindowPos(g_hNext, NULL, r1.left + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_X_WINDOWS10 : MODERN_UI_BUTTON_NUDGE_X_WINDOWS10), r1.top + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_Y_WINDOWS10 : MODERN_UI_BUTTON_NUDGE_Y_WINDOWS10), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}

		if (GetMappedWindowRect(g_hCancel, &r1))
		{
			if (g_fClassicUI)
				SetWindowPos(g_hCancel, NULL, r1.left - CLASSIC_UI_BUTTON_NUDGE_X_WINDOWS10, r1.top + CLASSIC_UI_BUTTON_NUDGE_Y_WINDOWS10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			else
				SetWindowPos(g_hCancel, NULL, r1.left + MODERN_UI_BUTTON_NUDGE_X_WINDOWS10, r1.top + MODERN_UI_BUTTON_NUDGE_Y_WINDOWS10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}

		if (GetMappedWindowRect(g_hBack, &r1))
		{
			SetWindowPos(g_hBack, NULL, r1.left + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_X_WINDOWS10 : MODERN_UI_BUTTON_NUDGE_X_WINDOWS10), r1.top + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_Y_WINDOWS10 : MODERN_UI_BUTTON_NUDGE_Y_WINDOWS10), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
		
		g_hbAero = GetSysColorBrush(COLOR_WINDOW);

		// Register callback so /NOUNLOAD does not have to be used.
		extra->RegisterPluginCallback((HMODULE)g_hInstance, PluginCallback);

		// We need to handle WM_ERASEBKGND to fill our aero rect and non aero rect.
		ParentDlgProcOld = (WNDPROC)SetWindowLongPtr(hWndParent, DWLP_DLGPROC, (LONG_PTR)Windows10ParentDlgProc);

		// Subclass the branding text so we know when it is shown and hidden.
		SetProp(g_hBrandingText, PROP_AERO_WNDPROC, (HANDLE)SetWindowLongPtr(g_hBrandingText, GWLP_WNDPROC, (LONG_PTR)BrandingTextWndProc));
		return;
	}

	HINSTANCE hDll = LoadLibrary(TEXT("Dwmapi.dll"));
	if (hDll == NULL)
		return;
  
	// These APIs are mandatory.
	DwmIsCompositionEnabled = (PDwmIsCompositionEnabled)GetProcAddress(hDll, "DwmIsCompositionEnabled");
	DwmExtendFrameIntoClientArea = (PDwmExtendFrameIntoClientArea)GetProcAddress(hDll, "DwmExtendFrameIntoClientArea");
	if (DwmIsCompositionEnabled == NULL || DwmExtendFrameIntoClientArea == NULL)
		return;

	// Check if Windows Aero is enabled.
	BOOL fAero;
	if (!SUCCEEDED(DwmIsCompositionEnabled(&fAero)))
		return;
  
	// We need some other functions from UxTheme.dll.
	hDll = LoadLibrary(TEXT("UxTheme.dll"));
	if (hDll == NULL)
		return;
  
	// These APIs are mandatory.
	OpenThemeData = (POpenThemeData)GetProcAddress(hDll, "OpenThemeData");
	CloseThemeData = (PCloseThemeData)GetProcAddress(hDll, "CloseThemeData");
	BufferedPaintInit = (PBufferedPaintInit)GetProcAddress(hDll, "BufferedPaintInit");
	BufferedPaintUnInit = (PBufferedPaintUnInit)GetProcAddress(hDll, "BufferedPaintUnInit");
	BeginBufferedPaint = (PBeginBufferedPaint)GetProcAddress(hDll, "BeginBufferedPaint");
	EndBufferedPaint = (PEndBufferedPaint)GetProcAddress(hDll, "EndBufferedPaint");
	BufferedPaintSetAlpha = (PBufferedPaintSetAlpha)GetProcAddress(hDll, "BufferedPaintSetAlpha");
	GetThemeBackgroundContentRect = (PGetThemeBackgroundContentRect)GetProcAddress(hDll, "GetThemeBackgroundContentRect");
	if (OpenThemeData == NULL ||
			CloseThemeData == NULL ||
			BufferedPaintInit == NULL ||
			BufferedPaintUnInit == NULL ||
			BeginBufferedPaint == NULL ||
			EndBufferedPaint == NULL ||
			BufferedPaintSetAlpha == NULL ||
			GetThemeBackgroundContentRect == NULL)
		return;

	// We always need the button theme.
	g_hButtonTheme = OpenThemeData(hWndParent, THEME_BUTTON);
	if (g_hButtonTheme == NULL)
		return;

	// Default values.
	BufferedPaintInit();
	g_fDrawButtonText = TRUE;
	g_fRTL = extra->exec_flags->rtl;

	// Should we draw the branding text?
	PTCHAR pszParam = (PTCHAR)GlobalAlloc(GPTR, sizeof(TCHAR) * string_size);
	if (pszParam)
	{
		while (popstring(pszParam) == 0)
		{
			if (lstrcmpi(pszParam, TEXT("/nobranding")) == 0)
			{
				g_fDrawBrandingText = FALSE;
			}
			else if (lstrcmpi(pszParam, TEXT("/btnold")) == 0)
			{
				g_fDrawButtonText = FALSE;
			}
			else
			{
				pushstring(pszParam);
				break;
			}
		}
    
		GlobalFree(pszParam);
	}

	// Branding text or button drawing is enabled.
	if (g_fDrawBrandingText || g_fDrawButtonText)
	{
		DrawThemeTextEx = (PDrawThemeTextEx)GetProcAddress(hDll, "DrawThemeTextEx");
		if (DrawThemeTextEx == NULL)
			goto cleanup;
	}

	// We like to know the position of the branding text.
	GetMappedWindowRect(g_hBrandingText, &g_rectBrandingText);

	// Branding text is enabled; load additional APIs.
	g_hWindowTheme = NULL;
	if (g_fDrawBrandingText)
	{
		g_fDrawBrandingText = FALSE;

		if (IsWindow(g_hBrandingText))
		{
			g_hWindowTheme = OpenThemeData(hWndParent, THEME_WINDOW);
			if (g_hWindowTheme != NULL)
				g_fDrawBrandingText = TRUE;
		}
	}
	
	RECT r1, r2;
	HWND hWndCtl = GetDlgItem(g_hWndParent, 1044);
	if (IsWindow(hWndCtl))
	{
		if (!GetClientRect(hWndCtl, &r1) || !GetClientRect(g_hWndParent, &r2))
			goto cleanup;
		
		hWndCtl = GetDlgItem(g_hWndParent, 1035);
		if (GetMappedWindowRect(hWndCtl, &g_rect1035) && fAero)
		{
			SetWindowPos(hWndCtl, NULL, 0, r2.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}

		hWndCtl = GetDlgItem(g_hWndParent, 1045);
		if (GetMappedWindowRect(hWndCtl, &g_rect1045) && fAero)
		{
			SetWindowPos(hWndCtl, NULL, 0, r2.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
    
		hWndCtl = GetDlgItem(g_hWndParent, 1256);
		if (GetMappedWindowRect(hWndCtl, &g_rect1256) && fAero)
		{
			SetWindowPos(hWndCtl, NULL, 0, r2.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}

		g_fClassicUI = FALSE;
	}
	else // Implement for non MUI?:
	{
		// Find the horizontal line static label.
		hWndCtl = GetDlgItem(g_hWndParent, -1);
		if (!IsWindow(hWndCtl))
		{
			hWndCtl = NULL;
			EnumChildWindows(g_hWndParent, EnumChildProc, (LPARAM)&hWndCtl);
			if (!IsWindow(hWndCtl))
				goto cleanup;
		}

		if (!GetMappedWindowRect(hWndCtl, &r1) || !GetClientRect(g_hWndParent, &r2))
			goto cleanup;
		
		if (fAero)
		{
			ShowWindow(hWndCtl, SW_HIDE);
		}

		g_fClassicUI = TRUE;
	}

	g_margins.cxLeftWidth = g_margins.cxRightWidth = g_margins.cyTopHeight = 0;
	g_margins.cyBottomHeight = r2.bottom - r1.bottom;
	g_rectAero.left = g_rectWindow.left = 0;
	g_rectAero.top = r1.bottom; g_rectWindow.top = 0;
	g_rectAero.right = g_rectWindow.right = r2.right;
	g_rectAero.bottom = r2.bottom; g_rectWindow.bottom = r1.bottom;
	
  if (fAero)
	{
		SetWindowPos(g_hBrandingText, NULL, g_fClassicUI ? g_rectBrandingText.left : 0, r2.bottom + 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}

	if (GetMappedWindowRect(g_hNext, &g_rectNext) && fAero)
	{
		SetWindowPos(g_hNext, NULL, g_rectNext.left + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_X : MODERN_UI_BUTTON_NUDGE_X), g_rectNext.top + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_Y : MODERN_UI_BUTTON_NUDGE_Y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}

	if (GetMappedWindowRect(g_hCancel, &g_rectCancel) && fAero)
	{
		if (g_fClassicUI)
			SetWindowPos(g_hCancel, NULL, g_rectCancel.left - CLASSIC_UI_BUTTON_NUDGE_X, g_rectCancel.top + CLASSIC_UI_BUTTON_NUDGE_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		else
			SetWindowPos(g_hCancel, NULL, g_rectCancel.left + MODERN_UI_BUTTON_NUDGE_X, g_rectCancel.top + MODERN_UI_BUTTON_NUDGE_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}

	if (GetMappedWindowRect(g_hBack, &g_rectBack) && fAero)
	{
		SetWindowPos(g_hBack, NULL, g_rectBack.left + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_X : MODERN_UI_BUTTON_NUDGE_X), g_rectBack.top + (g_fClassicUI ? CLASSIC_UI_BUTTON_NUDGE_Y : MODERN_UI_BUTTON_NUDGE_Y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}

	// Apply Windows Aero.
	if (fAero)
	{
		if (!SUCCEEDED(DwmExtendFrameIntoClientArea(hWndParent, &g_margins)))
			goto cleanup;
		g_hbAero = CreateSolidBrush(COLOR_AERO);
	}
	else
	{
		g_hbAero = NULL;
	}

	// Register callback so /NOUNLOAD does not have to be used.
	extra->RegisterPluginCallback((HMODULE)g_hInstance, PluginCallback);

	// Subclass the buttons.
	g_pwszBack = g_pwszNext = g_pwszCancel = NULL;
	SetProp(g_hBack, PROP_AERO_WNDPROC, (HANDLE)SetWindowLongPtr(g_hBack, GWLP_WNDPROC, (LONG_PTR)ButtonWndProc));
	SetProp(g_hCancel, PROP_AERO_WNDPROC, (HANDLE)SetWindowLongPtr(g_hCancel, GWLP_WNDPROC, (LONG_PTR)ButtonWndProc));
	SetProp(g_hNext, PROP_AERO_WNDPROC, (HANDLE)SetWindowLongPtr(g_hNext, GWLP_WNDPROC, (LONG_PTR)ButtonWndProc));

	// We need to handle WM_ERASEBKGND to fill our aero rect and non aero rect.
	ParentDlgProcOld = (WNDPROC)SetWindowLongPtr(hWndParent, DWLP_DLGPROC, (LONG_PTR)ParentDlgProc);

	// Subclass the branding text so we know when it is shown and hidden.
	SetProp(g_hBrandingText, PROP_AERO_WNDPROC, (HANDLE)SetWindowLongPtr(g_hBrandingText, GWLP_WNDPROC, (LONG_PTR)BrandingTextWndProc));

	return;
cleanup:

	if (pszParam != NULL)
		GlobalFree(pszParam);
  
	if (g_hWindowTheme != NULL)
		CloseThemeData(g_hWindowTheme);
  
	if (g_hButtonTheme != NULL)
		CloseThemeData(g_hButtonTheme);

	BufferedPaintUnInit();
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	g_hInstance = hInst;
	return TRUE;
}