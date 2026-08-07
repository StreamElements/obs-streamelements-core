/*
  Aero NSIS plug-in by Stuart Welch <afrowuk@afrowsoft.co.uk>
*/

#ifndef __AERO_H__
#define __AERO_H__

typedef struct _MARGINS
{
  int cxLeftWidth;
  int cxRightWidth;
  int cyTopHeight;
  int cyBottomHeight;
} MARGINS, *PMARGINS;

typedef int (WINAPI *DTT_CALLBACK_PROC)
(
  HDC hdc,
  LPWSTR pszText,
  int cchText,
  LPRECT prc,
  UINT dwFlags,
  LPARAM lParam
);

typedef struct _DTTOPTS
{
  DWORD             dwSize;
  DWORD             dwFlags;
  COLORREF          crText;
  COLORREF          crBorder;
  COLORREF          crShadow;
  int               iTextShadowType;
  POINT             ptShadowOffset;
  int               iBorderSize;
  int               iFontPropId;
  int               iColorPropId;
  int               iStateId;
  BOOL              fApplyOverlay;
  int               iGlowSize;
  DTT_CALLBACK_PROC pfnDrawTextCallback;
  LPARAM            lParam;
} DTTOPTS, *PDTTOPTS;

typedef enum _BP_BUFFERFORMAT
{
  BPBF_COMPATIBLEBITMAP,
  BPBF_DIB,
  BPBF_TOPDOWNDIB,
  BPBF_TOPDOWNMONODIB
} BP_BUFFERFORMAT;

#define BPBF_COMPOSITED BPBF_TOPDOWNDIB

typedef struct _BP_PAINTPARAMS
{
  DWORD                cbSize;
  DWORD                dwFlags;
  const RECT*          prcExclude;
  const BLENDFUNCTION* pBlendFunction;
} BP_PAINTPARAMS, *PBP_PAINTPARAMS;

typedef HANDLE HTHEME;
typedef HANDLE HPAINTBUFFER;

typedef HRESULT (WINAPI* PDwmIsCompositionEnabled)(BOOL* pfEnabled);
typedef HRESULT (WINAPI* PDwmExtendFrameIntoClientArea)(HWND hWnd, const MARGINS* pMargins);
typedef HRESULT (WINAPI* PDrawThemeTextEx)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwFlags, LPRECT pRect, const DTTOPTS* pOptions);
typedef HTHEME (WINAPI* POpenThemeData)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (WINAPI* PCloseThemeData)(HTHEME hTheme);
typedef HRESULT (WINAPI* PBufferedPaintInit)(VOID);
typedef HRESULT (WINAPI* PBufferedPaintUnInit)(VOID);
typedef HPAINTBUFFER (WINAPI* PBeginBufferedPaint)(HDC hdcTarget, const RECT* prcTarget, BP_BUFFERFORMAT dwFormat, BP_PAINTPARAMS* pPaintParams, HDC* phdc);
typedef HRESULT (WINAPI* PEndBufferedPaint)(HPAINTBUFFER hBufferedPaint, BOOL fUpdateTarget);
typedef HRESULT (WINAPI* PBufferedPaintSetAlpha)(HPAINTBUFFER hBufferedPaint, const RECT* prc, BYTE alpha);
typedef HRESULT (WINAPI* PGetThemeBackgroundContentRect)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pBoundingRect, LPRECT pContentRect);

PDwmIsCompositionEnabled       DwmIsCompositionEnabled = NULL;
PDwmExtendFrameIntoClientArea  DwmExtendFrameIntoClientArea = NULL;
PDrawThemeTextEx               DrawThemeTextEx = NULL;
POpenThemeData                 OpenThemeData = NULL;
PCloseThemeData                CloseThemeData = NULL;
PBufferedPaintInit             BufferedPaintInit = NULL;
PBufferedPaintUnInit           BufferedPaintUnInit = NULL;
PBeginBufferedPaint            BeginBufferedPaint = NULL;
PEndBufferedPaint              EndBufferedPaint = NULL;
PBufferedPaintSetAlpha         BufferedPaintSetAlpha = NULL;
PGetThemeBackgroundContentRect GetThemeBackgroundContentRect = NULL;

#define NSISFUNC(name) extern "C" void __declspec(dllexport) name(HWND hWndParent, int string_size, TCHAR* variables, stack_t** stacktop, extra_parameters* extra)
#define DLL_INIT() { EXDLL_INIT(); g_hWndParent = hWndParent; }
#define COLOR_AERO RGB(0, 0, 0)
#define MODERN_UI_BUTTON_NUDGE_X 4
#define MODERN_UI_BUTTON_NUDGE_Y 2
#define MODERN_UI_BUTTON_NUDGE_X_WINDOWS10 0
#define MODERN_UI_BUTTON_NUDGE_Y_WINDOWS10 0
#define CLASSIC_UI_BUTTON_NUDGE_X 6
#define CLASSIC_UI_BUTTON_NUDGE_Y 5
#define CLASSIC_UI_BUTTON_NUDGE_X_WINDOWS10 2
#define CLASSIC_UI_BUTTON_NUDGE_Y_WINDOWS10 1

#define DTT_GLOWSIZE   (1UL << 11)
#define DTT_COMPOSITED (1UL << 13)
#define BP_PUSHBUTTON 1

#define PBS_NORMAL      0x00000001
#define PBS_HOT         0x00000002
#define PBS_PRESSED     0x00000003
#define PBS_DISABLED    0x00000004
#define PBS_DEFAULTED   0x00000005

#define PROP_AERO_WNDPROC TEXT("_StuAeroWndProc_")
#define THEME_WINDOW L"CompositedWindow::Window"
#define THEME_BUTTON L"Button"

#endif