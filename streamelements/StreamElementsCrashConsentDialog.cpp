#include "StreamElementsCrashConsentDialog.hpp"

#include "StreamElementsUtils.hpp"

#include <vector>

#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>

// Linked here rather than in CMakeLists: both are needed only by the dark
// theming below, and this file is the only Windows-only consumer of either.
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

/* ================================================================= */

//
// Strings are hardcoded English rather than routed through obs_module_text().
//
// This matches every other user-visible string in the crash handler -- the
// "please wait" window and BugSplat's own prompts are hardcoded too -- and
// keeps the dying process from reaching into the module's locale lookup. If
// these are ever localised, the rest of the crash handler should move with
// them rather than this one dialog diverging.
//
static const wchar_t *const DIALOG_TITLE = L"SE.Live";

static const wchar_t *const PROMPT_TEXT =
	L"OBS Studio has stopped working, and SE.Live can send a report to help us fix it. "
	L"What were you doing just before this happened?";

static const wchar_t *const CONTACT_TEXT =
	L"All three are optional. They let us follow up about this crash, and are remembered for next time.";

//
// What the report actually contains.
//
// The archive is the whole OBS configuration tree plus a picture of the OBS
// main window, which can show a camera preview, a browser source or anything
// else on screen at the moment of the crash. That is personal information by
// any reasonable reading, and the user is entitled to know before consenting.
//
// The stream-key claim is load-bearing and true: service.json is redacted in
// place before it enters the archive. If that redaction is ever removed or
// bypassed, this sentence has to go with it.
//
static const wchar_t *const PRIVACY_TEXT =
	L"The report includes your OBS Studio and SE.Live configuration and an image of the OBS window. "
	L"These may contain personal information, and are used only to diagnose this crash. Stream keys are removed.";

/* ================================================================= */

#define IDC_DESCRIPTION 1001
#define IDC_NAME 1002
#define IDC_EMAIL 1003
#define IDC_ERRORICON 1004
#define IDC_DISCORD 1005

// System window class atoms, as used inside a dialog template.
#define ATOM_BUTTON 0x0080
#define ATOM_EDIT 0x0081
#define ATOM_STATIC 0x0082

//
// Builds a DLGTEMPLATE in memory.
//
// The layout is a packed, DWORD-aligned byte stream rather than a struct: the
// header and every item are followed by variable-length name/title arrays, so
// there is nothing to declare as a type.
//
class DialogTemplateBuilder {
public:
	void AlignDword()
	{
		while (m_bytes.size() % sizeof(DWORD))
			m_bytes.push_back(0);
	}

	void AddWord(WORD value)
	{
		const BYTE *p = (const BYTE *)&value;

		m_bytes.insert(m_bytes.end(), p, p + sizeof(value));
	}

	void AddDword(DWORD value)
	{
		const BYTE *p = (const BYTE *)&value;

		m_bytes.insert(m_bytes.end(), p, p + sizeof(value));
	}

	void AddShort(short value) { AddWord((WORD)value); }

	// A sz_Or_Ord holding a string, null terminator included.
	void AddString(const wchar_t *text)
	{
		for (const wchar_t *p = text; *p; ++p)
			AddWord((WORD)*p);

		AddWord(0);
	}

	// A sz_Or_Ord holding a class atom.
	void AddOrdinal(WORD atom)
	{
		AddWord(0xFFFF);
		AddWord(atom);
	}

	const DLGTEMPLATE *Get() const
	{
		return (const DLGTEMPLATE *)m_bytes.data();
	}

private:
	std::vector<BYTE> m_bytes;
};

static void AddControl(DialogTemplateBuilder &builder, DWORD style, short x,
		       short y, short cx, short cy, WORD id, WORD classAtom,
		       const wchar_t *text)
{
	builder.AlignDword();

	builder.AddDword(style);
	builder.AddDword(0); // extended style
	builder.AddShort(x);
	builder.AddShort(y);
	builder.AddShort(cx);
	builder.AddShort(cy);
	builder.AddWord(id);

	builder.AddOrdinal(classAtom);
	builder.AddString(text);

	builder.AddWord(0); // no creation data
}

/* ================================================================= */

struct DialogState {
	std::wstring name;
	std::wstring email;
	std::wstring discord;

	std::wstring description;
	bool consented = false;
};

static std::wstring GetControlText(HWND dialog, int id)
{
	HWND control = ::GetDlgItem(dialog, id);

	if (!control)
		return L"";

	const int length = ::GetWindowTextLengthW(control);

	if (length <= 0)
		return L"";

	std::vector<wchar_t> buffer((size_t)length + 1, 0);

	::GetWindowTextW(control, buffer.data(), length + 1);

	return buffer.data();
}

//
// Dark theme, unconditionally -- not "follow the system".
//
// This dialog appears in front of a dying OBS, whose own UI is dark in every
// theme it ships. A light system dialog in front of that does not read as part
// of the same application, which matters here more than usual: the user is
// being asked to send us their configuration and a picture of their screen, and
// anything that looks like it came from somewhere else is a reason to say no.
//
static const COLORREF kDarkBackground = RGB(32, 32, 32);
static const COLORREF kDarkControl = RGB(45, 45, 45);
static const COLORREF kDarkText = RGB(255, 255, 255);
static const COLORREF kDarkButtonFace = RGB(86, 132, 253);
static const COLORREF kFieldBorder = RGB(80, 80, 80);
static const COLORREF kDarkButtonHover = RGB(64, 104, 214);
static const COLORREF kDarkButtonPressed = RGB(52, 86, 180);
static const COLORREF kOutlineHover = RGB(52, 52, 52);
static const COLORREF kOutlinePressed = RGB(64, 64, 64);
static const COLORREF kDarkButtonEdge = RGB(90, 90, 90);
static const COLORREF kDarkButtonFocusEdge = RGB(0, 120, 212);
static const COLORREF kAcceptGlyph = RGB(64, 200, 96);
static const COLORREF kDeclineGlyph = RGB(232, 92, 92);

//
// One padding value and one corner radius, used by everything drawn here so the
// buttons and the field outlines stay visually of a piece.
//
static const int kPadding = 8;
static const int kCornerRadius = 4;   // field outlines
static const int kButtonRadius = 16; // buttons
static const int kGlyphBox = 16;     // circle / cross, both buttons

// Inside a button: 24px from the left and right edges to the glyph and the
// caption, 8px above and below. kPadding stays the gap between the glyph and
// the caption, and between the two buttons.
static const int kButtonPaddingH = 24;
static const int kButtonPaddingV = 8;

//
// Which button the pointer is over, or null.
//
// A BS_OWNERDRAW button is not repainted by the system when the pointer enters
// or leaves it -- the theme engine that would normally do the hot-tracking is
// exactly what we opted out of -- so the two buttons are subclassed to track it
// themselves. One static is enough: the pointer can only be over one of them.
//
static HWND s_hotButton = nullptr;

static LRESULT CALLBACK ConsentButtonProc(HWND button, UINT message,
					  WPARAM wParam, LPARAM lParam,
					  UINT_PTR id, DWORD_PTR data)
{
	UNREFERENCED_PARAMETER(id);
	UNREFERENCED_PARAMETER(data);

	switch (message) {
	case WM_MOUSEMOVE:
		if (s_hotButton != button) {
			HWND previous = s_hotButton;

			s_hotButton = button;

			if (previous)
				::InvalidateRect(previous, NULL, FALSE);

			::InvalidateRect(button, NULL, FALSE);

			// Without this there is no WM_MOUSELEAVE, and the button
			// stays lit after the pointer has gone.
			TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, button,
						 0};
			::TrackMouseEvent(&track);
		}
		break;

	case WM_MOUSELEAVE:
		if (s_hotButton == button) {
			s_hotButton = nullptr;

			::InvalidateRect(button, NULL, FALSE);
		}
		break;

	case WM_NCDESTROY:
		::RemoveWindowSubclass(button, ConsentButtonProc, 1);

		if (s_hotButton == button)
			s_hotButton = nullptr;

		break;
	}

	return ::DefSubclassProc(button, message, wParam, lParam);
}

//
// Arial, bold, and a couple of points up from the dialog body font.
//
// Sized from the DC rather than as a fixed pixel height so it follows the
// display DPI -- a hardcoded height is legible on the machine it was written on
// and small everywhere else. Cached and leaked for the same reason as the
// brushes below.
//
static HFONT GetButtonFont(HDC hdc)
{
	static HFONT font = nullptr;

	if (!font) {
		LOGFONTW lf = {};

		lf.lfHeight = -::MulDiv(10, ::GetDeviceCaps(hdc, LOGPIXELSY), 72);
		lf.lfWeight = FW_BOLD;
		lf.lfCharSet = DEFAULT_CHARSET;
		lf.lfQuality = CLEARTYPE_QUALITY;

		::wcscpy_s(lf.lfFaceName, L"Arial");

		font = ::CreateFontIndirectW(&lf);
	}

	return font;
}

//
// Deliberately leaked, both of them. They live as long as the dialog, the
// process is terminating behind it, and a brush destroyed while the dialog is
// still painting is a far worse outcome than one that is never freed.
//
static HBRUSH GetDarkBackgroundBrush()
{
	static HBRUSH brush = ::CreateSolidBrush(kDarkBackground);

	return brush;
}

static HBRUSH GetDarkControlBrush()
{
	static HBRUSH brush = ::CreateSolidBrush(kDarkControl);

	return brush;
}

//
// The title bar, which no amount of WM_CTLCOLOR* reaches -- it is drawn by the
// desktop compositor, not by us.
//
static void ApplyDarkTitleBar(HWND window)
{
	// 20 on Windows 10 2004 and later, 19 on the builds before it. Trying
	// the current one and falling back is cheaper and more honest than
	// sniffing the build number, and both simply fail on older systems.
	const DWORD kUseImmersiveDarkMode = 20;
	const DWORD kUseImmersiveDarkModeBefore20H1 = 19;

	BOOL enabled = TRUE;

	if (FAILED(::DwmSetWindowAttribute(window, kUseImmersiveDarkMode,
					   &enabled, sizeof(enabled)))) {
		::DwmSetWindowAttribute(window, kUseImmersiveDarkModeBefore20H1,
					&enabled, sizeof(enabled));
	}
}

//
// Edit borders and button faces are drawn by the theme engine, which ignores
// the colours we hand back below. "DarkMode_Explorer" is undocumented but has
// been what File Explorer itself uses since Windows 10 1809. If it fails the
// dialog is still readable -- the handlers below have already painted the
// backgrounds and the text.
//
static BOOL CALLBACK ApplyDarkThemeToChild(HWND child, LPARAM)
{
	::SetWindowTheme(child, L"DarkMode_Explorer", nullptr);

	return TRUE;
}

//
// Paints an owner-drawn push button, since the theme engine will not do it in
// our colours.
//
// The two are deliberately not equal weight. "Send report" is the outcome the
// dialog is asking for and is drawn as a filled blue button; "Don't send" is an
// outline on the dialog face. Both keep the same size, font and text colour --
// the declining button is secondary, not discouraged or hidden, and a user who
// wants it has to be able to find it immediately.
//
// The glyphs are drawn with lines rather than set as font characters. A tick
// and a cross are two and four segments respectively, and drawing them avoids
// depending on a font lookup and its fallback chain inside a process that has
// already crashed.
//
//
// The width a button needs to hold its glyph and caption at 8px padding.
//
// Measured rather than guessed: the caption is set in bold uppercase Arial at a
// size derived from the display DPI, so the only honest way to size the button
// is to ask the DC how wide the string actually is. A fixed width was clipping
// "SEND REPORT" mid-word.
//
static SIZE MeasureConsentButton(HDC hdc, const wchar_t *caption)
{
	HGDIOBJ previousFont = ::SelectObject(hdc, GetButtonFont(hdc));

	SIZE text = {0, 0};
	::GetTextExtentPoint32W(hdc, caption, (int)wcslen(caption), &text);

	::SelectObject(hdc, previousFont);

	SIZE result;

	// padding | glyph | padding | text | padding
	result.cx = kButtonPaddingH + kGlyphBox + kPadding + text.cx +
		    kButtonPaddingH;
	result.cy = text.cy + kButtonPaddingV * 2;

	return result;
}

static void DrawConsentButton(const DRAWITEMSTRUCT *item)
{
	const bool primary = item->CtlID == IDOK;
	const bool pressed = (item->itemState & ODS_SELECTED) != 0;
	const bool focused = (item->itemState & (ODS_FOCUS | ODS_DEFAULT)) != 0;
	const bool hot = item->hwndItem == s_hotButton;

	RECT rc = item->rcItem;

	// The dialog face first. RoundRect leaves the four corners outside its
	// path untouched, and without this they keep whatever was there before,
	// which reads as chipped rather than rounded.
	::FillRect(item->hDC, &rc, GetDarkBackgroundBrush());

	// Hover moves the filled button darker and the outline button lighter.
	// Both end up closer to the other in tone, which is what makes the
	// pointer feel like it is resting on something.
	COLORREF faceColor;

	if (primary) {
		if (pressed)
			faceColor = kDarkButtonPressed;
		else if (hot)
			faceColor = kDarkButtonHover;
		else
			faceColor = kDarkButtonFace;
	} else {
		if (pressed)
			faceColor = kOutlinePressed;
		else if (hot)
			faceColor = kOutlineHover;
		else
			faceColor = kDarkBackground;
	}

	HBRUSH face = ::CreateSolidBrush(faceColor);

	// Brighter edge when the button is the keyboard target, so the focused
	// control stays obvious without the theme's focus ring. The filled button
	// draws its edge in its own colour, so it reads as one solid shape.
	HPEN edge = ::CreatePen(PS_SOLID, 1,
				focused ? kDarkButtonFocusEdge
					: (primary ? faceColor : kDarkButtonEdge));

	HGDIOBJ previousBrush = ::SelectObject(item->hDC, face);
	HGDIOBJ previousPen = ::SelectObject(item->hDC, edge);

	::RoundRect(item->hDC, rc.left, rc.top, rc.right, rc.bottom,
		    kButtonRadius * 2, kButtonRadius * 2);

	::SelectObject(item->hDC, previousBrush);
	::SelectObject(item->hDC, previousPen);
	::DeleteObject(face);
	::DeleteObject(edge);

	const int glyphLeft = rc.left + kButtonPaddingH;
	const int glyphTop = (rc.top + rc.bottom) / 2 - kGlyphBox / 2;

	HPEN pen = ::CreatePen(PS_SOLID, 2,
			       primary ? kAcceptGlyph : kDeclineGlyph);
	previousPen = ::SelectObject(item->hDC, pen);
	previousBrush = ::SelectObject(item->hDC, ::GetStockObject(NULL_BRUSH));

	if (primary) {
		// A tick inside a ring, so the accepting action reads as a
		// deliberate mark rather than a stray stroke on the blue face.
		::Ellipse(item->hDC, glyphLeft, glyphTop, glyphLeft + kGlyphBox,
			  glyphTop + kGlyphBox);

		const int inset = 4;

		POINT tick[3] = {{glyphLeft + inset,
				  glyphTop + kGlyphBox / 2},
				 {glyphLeft + kGlyphBox / 2 - 1,
				  glyphTop + kGlyphBox - inset},
				 {glyphLeft + kGlyphBox - inset,
				  glyphTop + inset + 1}};

		::Polyline(item->hDC, tick, 3);
	} else {
		const int inset = 2;

		::MoveToEx(item->hDC, glyphLeft + inset, glyphTop + inset, NULL);
		::LineTo(item->hDC, glyphLeft + kGlyphBox - inset,
			 glyphTop + kGlyphBox - inset);

		::MoveToEx(item->hDC, glyphLeft + kGlyphBox - inset,
			   glyphTop + inset, NULL);
		::LineTo(item->hDC, glyphLeft + inset,
			 glyphTop + kGlyphBox - inset);
	}

	::SelectObject(item->hDC, previousPen);
	::SelectObject(item->hDC, previousBrush);
	::DeleteObject(pen);

	// Uppercased at draw time rather than in the template, so the control's
	// own text -- what the accessibility tree and any automation read --
	// stays the sentence-case string it was created with.
	wchar_t caption[64] = {0};
	::GetWindowTextW(item->hwndItem, caption, ARRAYSIZE(caption));
	::CharUpperBuffW(caption, (DWORD)wcslen(caption));

	RECT caption_rc = rc;
	caption_rc.left = glyphLeft + kGlyphBox + kPadding;
	caption_rc.right -= kButtonPaddingH;

	HGDIOBJ previousFont =
		::SelectObject(item->hDC, GetButtonFont(item->hDC));

	::SetBkMode(item->hDC, TRANSPARENT);
	::SetTextColor(item->hDC, kDarkText);
	::DrawTextW(item->hDC, caption, -1, &caption_rc,
		    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	::SelectObject(item->hDC, previousFont);
}

//
// Sizes both buttons to their own content and right-aligns the pair.
//
// The template can only carry fixed sizes, and the caption width is not known
// until the font has been created against a real DC. So the template's numbers
// are a placeholder and this replaces them at WM_INITDIALOG, keeping the right
// edge flush with the fields above and kPadding between the two.
//
static void LayoutConsentButtons(HWND dialog)
{
	HWND accept = ::GetDlgItem(dialog, IDOK);
	HWND decline = ::GetDlgItem(dialog, IDCANCEL);
	HWND field = ::GetDlgItem(dialog, IDC_DESCRIPTION);

	if (!accept || !decline || !field)
		return;

	HDC hdc = ::GetDC(dialog);

	if (!hdc)
		return;

	wchar_t acceptText[64] = {0};
	wchar_t declineText[64] = {0};

	::GetWindowTextW(accept, acceptText, ARRAYSIZE(acceptText));
	::GetWindowTextW(decline, declineText, ARRAYSIZE(declineText));

	::CharUpperBuffW(acceptText, (DWORD)wcslen(acceptText));
	::CharUpperBuffW(declineText, (DWORD)wcslen(declineText));

	const SIZE acceptSize = MeasureConsentButton(hdc, acceptText);
	const SIZE declineSize = MeasureConsentButton(hdc, declineText);

	::ReleaseDC(dialog, hdc);

	const int height = acceptSize.cy > declineSize.cy ? acceptSize.cy
							  : declineSize.cy;

	// Right edge and vertical position come from the template, so the layout
	// above this row still decides where the row sits.
	RECT fieldRect;
	::GetWindowRect(field, &fieldRect);
	::MapWindowPoints(NULL, dialog, (LPPOINT)&fieldRect, 2);

	RECT acceptRect;
	::GetWindowRect(accept, &acceptRect);
	::MapWindowPoints(NULL, dialog, (LPPOINT)&acceptRect, 2);

	const int top = acceptRect.top;
	const int right = fieldRect.right;

	const int declineLeft = right - declineSize.cx;
	const int acceptLeft = declineLeft - kPadding - acceptSize.cx;

	::SetWindowPos(accept, NULL, acceptLeft, top, acceptSize.cx, height,
		       SWP_NOZORDER | SWP_NOACTIVATE);
	::SetWindowPos(decline, NULL, declineLeft, top, declineSize.cx, height,
		       SWP_NOZORDER | SWP_NOACTIVATE);

	::SetWindowSubclass(accept, ConsentButtonProc, 1, 0);
	::SetWindowSubclass(decline, ConsentButtonProc, 1, 0);
}

//
// A flat, subtle outline around each entry field.
//
// The fields carry no WS_BORDER of their own: the border a themed Edit draws is
// the system's, in the system's colour, and against a dark dialog it reads as a
// bright groove. Drawing it here instead keeps it one grey hairline at the same
// 4px radius as the buttons.
//
static void DrawFieldOutline(HWND dialog, HDC hdc, int controlId)
{
	HWND control = ::GetDlgItem(dialog, controlId);

	if (!control)
		return;

	RECT rc;
	::GetWindowRect(control, &rc);
	::MapWindowPoints(NULL, dialog, (LPPOINT)&rc, 2);
	::InflateRect(&rc, 2, 2);

	HPEN pen = ::CreatePen(PS_SOLID, 1, kFieldBorder);
	HGDIOBJ previousPen = ::SelectObject(hdc, pen);
	HGDIOBJ previousBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));

	::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom,
		    kCornerRadius * 2, kCornerRadius * 2);

	::SelectObject(hdc, previousPen);
	::SelectObject(hdc, previousBrush);
	::DeleteObject(pen);
}

static INT_PTR CALLBACK ConsentDialogProc(HWND dialog, UINT message,
					  WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG: {
		auto *state = (DialogState *)lParam;

		::SetWindowLongPtrW(dialog, GWLP_USERDATA, (LONG_PTR)state);

		ApplyDarkTitleBar(dialog);
		::EnumChildWindows(dialog, ApplyDarkThemeToChild, 0);
		LayoutConsentButtons(dialog);

		if (state) {
			::SetDlgItemTextW(dialog, IDC_NAME,
					  state->name.c_str());
			::SetDlgItemTextW(dialog, IDC_EMAIL,
					  state->email.c_str());
			::SetDlgItemTextW(dialog, IDC_DISCORD,
					  state->discord.c_str());
		}

		// The system error icon, in the body next to the message and
		// again in the title bar. Loaded rather than named in the
		// template so it follows the OS rather than shipping our own
		// bitmap.
		// LoadIcon rather than LoadIconW: IDI_ERROR is itself a
		// TCHAR-generic macro, so the two have to match or the build
		// breaks wherever UNICODE is not defined.
		HICON icon = ::LoadIcon(NULL, IDI_ERROR);

		if (icon) {
			::SendDlgItemMessageW(dialog, IDC_ERRORICON,
					      STM_SETICON, (WPARAM)icon, 0);

			::SendMessageW(dialog, WM_SETICON, ICON_SMALL,
				       (LPARAM)icon);
			::SendMessageW(dialog, WM_SETICON, ICON_BIG,
				       (LPARAM)icon);
		}

		// Always on top.
		//
		// WS_EX_TOPMOST is already in the template, but it is set again
		// here because it is load-bearing rather than cosmetic: this
		// window has no owner (see Prompt), so nothing else will raise
		// it, and OBS is in the middle of dying behind it. A prompt the
		// user never sees is a crash report never sent.
		::SetWindowPos(dialog, HWND_TOPMOST, 0, 0, 0, 0,
			       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

		::SetForegroundWindow(dialog);

		::SetFocus(::GetDlgItem(dialog, IDC_DESCRIPTION));

		return FALSE; // focus was set explicitly
	}

	// The dialog face, and the static text sitting on it.
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSTATIC:
		::SetTextColor((HDC)wParam, kDarkText);
		::SetBkColor((HDC)wParam, kDarkBackground);

		return (INT_PTR)GetDarkBackgroundBrush();

	// The four entry fields, a shade lighter so they still read as fields.
	case WM_CTLCOLOREDIT:
		::SetTextColor((HDC)wParam, kDarkText);
		::SetBkColor((HDC)wParam, kDarkControl);

		return (INT_PTR)GetDarkControlBrush();

	case WM_PAINT: {
		// The background has already been erased through
		// WM_CTLCOLORDLG; this only adds the field outlines, which sit
		// outside each control's rect and so are not painted over when
		// the controls themselves repaint.
		PAINTSTRUCT ps;
		HDC hdc = ::BeginPaint(dialog, &ps);

		if (hdc) {
			DrawFieldOutline(dialog, hdc, IDC_DESCRIPTION);
			DrawFieldOutline(dialog, hdc, IDC_NAME);
			DrawFieldOutline(dialog, hdc, IDC_EMAIL);
			DrawFieldOutline(dialog, hdc, IDC_DISCORD);
		}

		::EndPaint(dialog, &ps);

		return TRUE;
	}

	case WM_DRAWITEM: {
		const DRAWITEMSTRUCT *item = (const DRAWITEMSTRUCT *)lParam;

		if (item->CtlType != ODT_BUTTON)
			break;

		DrawConsentButton(item);

		return TRUE;
	}

	case WM_COMMAND: {
		const int id = LOWORD(wParam);

		if (id != IDOK && id != IDCANCEL)
			break;

		auto *state = (DialogState *)::GetWindowLongPtrW(dialog,
								 GWLP_USERDATA);

		if (state && id == IDOK) {
			state->description =
				GetControlText(dialog, IDC_DESCRIPTION);
			state->name = GetControlText(dialog, IDC_NAME);
			state->email = GetControlText(dialog, IDC_EMAIL);
			state->discord = GetControlText(dialog, IDC_DISCORD);
			state->consented = true;
		}

		::EndDialog(dialog, id);

		return TRUE;
	}
	}

	return FALSE;
}

/* ================================================================= */

// Separate from Prompt() so that the template can be built -- and therefore
// checked -- without showing a modal dialog. A hand-assembled DLGTEMPLATE that
// is subtly malformed fails by returning -1 from DialogBoxIndirectParamW, which
// this handler reads as "the user declined": every crash report would be
// suppressed, silently. That is worth being able to test.
static void BuildConsentDialogTemplate(DialogTemplateBuilder &builder)
{
	const DWORD dialogStyle = DS_MODALFRAME | DS_CENTER | DS_SETFONT |
				  WS_POPUP | WS_CAPTION | WS_SYSMENU;

	builder.AddDword(dialogStyle);
	builder.AddDword(WS_EX_TOPMOST);
	builder.AddWord(13); // control count
	builder.AddShort(0);
	builder.AddShort(0);
	builder.AddShort(320);
	builder.AddShort(232);

	builder.AddWord(0); // no menu
	builder.AddWord(0); // default dialog class
	builder.AddString(DIALOG_TITLE);

	// Arial rather than MS Shell Dlg, so every string in the dialog -- the
	// message, the labels, the field contents -- is the same face. The
	// buttons take a bold, slightly larger Arial of their own.
	builder.AddWord(8); // point size
	builder.AddString(L"Arial");

	const DWORD visibleChild = WS_CHILD | WS_VISIBLE;
	// No WS_BORDER: DrawFieldOutline draws a flat grey hairline instead of
	// the system's themed border, which against a dark dialog is too bright
	// and still carries a groove.
	const DWORD editStyle = visibleChild | WS_TABSTOP | ES_AUTOHSCROLL;

	// The icon sizes itself to the system icon; cx/cy here are only a hint.
	// The message clears it and both share a right edge with everything
	// below (7 + 306 == 36 + 277).
	AddControl(builder, visibleChild | SS_ICON, 7, 9, 21, 20, IDC_ERRORICON,
		   ATOM_STATIC, L"");

	AddControl(builder, visibleChild, 36, 7, 277, 30, (WORD)-1, ATOM_STATIC,
		   PROMPT_TEXT);

	AddControl(builder,
		   editStyle | ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL, 7, 37,
		   306, 58, IDC_DESCRIPTION, ATOM_EDIT, L"");

	// Labels are 44 wide rather than 40 so "Discord:" is not clipped; the
	// edits start past them at 54 and share a right edge with everything
	// else at 313.
	AddControl(builder, visibleChild, 7, 102, 44, 10, (WORD)-1, ATOM_STATIC,
		   L"Name:");
	AddControl(builder, editStyle, 54, 100, 259, 13, IDC_NAME, ATOM_EDIT,
		   L"");

	AddControl(builder, visibleChild, 7, 120, 44, 10, (WORD)-1, ATOM_STATIC,
		   L"Email:");
	AddControl(builder, editStyle, 54, 118, 259, 13, IDC_EMAIL, ATOM_EDIT,
		   L"");

	AddControl(builder, visibleChild, 7, 138, 44, 10, (WORD)-1, ATOM_STATIC,
		   L"Discord:");
	AddControl(builder, editStyle, 54, 136, 259, 13, IDC_DISCORD, ATOM_EDIT,
		   L"");

	AddControl(builder, visibleChild, 7, 155, 306, 10, (WORD)-1,
		   ATOM_STATIC, CONTACT_TEXT);

	AddControl(builder, visibleChild, 7, 170, 306, 30, (WORD)-1,
		   ATOM_STATIC, PRIVACY_TEXT);

	// BS_OWNERDRAW on both, so DrawConsentButton below paints them.
	//
	// A themed push button ignores WM_CTLCOLORBTN entirely -- the theme
	// engine draws the face -- so on a dark dialog the buttons stay white
	// unless we take them over. The documented alternative is the
	// undocumented uxtheme ordinals (AllowDarkModeForWindow and friends),
	// which is not a trade worth making on the crash path.
	//
	// BS_DEFPUSHBUTTON is kept: it no longer changes the drawing, but it
	// still tells the dialog manager which control Enter activates.
	// Placeholder geometry. LayoutConsentButtons measures the captions in
	// the real button font at WM_INITDIALOG and resizes both to fit their
	// own glyph and text, then right-aligns the pair -- only the vertical
	// position below survives from here.
	AddControl(builder,
		   visibleChild | WS_TABSTOP | BS_DEFPUSHBUTTON | BS_OWNERDRAW,
		   113, 204, 96, 20, IDOK, ATOM_BUTTON, L"Send report");

	AddControl(builder,
		   visibleChild | WS_TABSTOP | BS_PUSHBUTTON | BS_OWNERDRAW, 217,
		   204, 96, 20, IDCANCEL, ATOM_BUTTON, L"Don't send");
}

/* ================================================================= */

StreamElementsCrashConsentDialog::Result
StreamElementsCrashConsentDialog::Prompt(const std::string &name,
					 const std::string &email,
					 const std::string &discord)
{
	Result result;

	DialogState state;
	state.name = utf8_to_wstring(name);
	state.email = utf8_to_wstring(email);
	state.discord = utf8_to_wstring(discord);

	DialogTemplateBuilder builder;

	BuildConsentDialogTemplate(builder);

	//
	// No owner window, deliberately.
	//
	// Owning this to the OBS main window would attach our input queue to the
	// thread that owns it. On a crash path that thread is quite possibly the
	// one that just faulted, or is blocked waiting on it, and attaching to a
	// wedged input queue hangs the prompt -- turning a crash into a hang. The
	// cost is that the dialog is not modal to OBS, which is why it is
	// WS_EX_TOPMOST and calls SetForegroundWindow above.
	//
	const INT_PTR outcome = ::DialogBoxIndirectParamW(
		::GetModuleHandleW(NULL), builder.Get(), NULL,
		ConsentDialogProc, (LPARAM)&state);

	// IDOK and IDCANCEL both mean the dialog appeared and was answered; -1
	// means Windows refused the template and it never did.
	result.prompted = (outcome == IDOK || outcome == IDCANCEL);

	if (outcome != IDOK) {
		// Declined, dismissed, or the dialog could not be created. All
		// three mean do not send.
		return result;
	}

	result.consented = state.consented;
	result.description = wstring_to_utf8(state.description);
	result.name = wstring_to_utf8(state.name);
	result.email = wstring_to_utf8(state.email);
	result.discord = wstring_to_utf8(state.discord);

	return result;
}
