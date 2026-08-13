#include "StreamElementsCrashConsentDialog.hpp"

#include "StreamElementsUtils.hpp"

#include <vector>

#include <windows.h>

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

static INT_PTR CALLBACK ConsentDialogProc(HWND dialog, UINT message,
					  WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG: {
		auto *state = (DialogState *)lParam;

		::SetWindowLongPtrW(dialog, GWLP_USERDATA, (LONG_PTR)state);

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

	builder.AddWord(8); // point size
	builder.AddString(L"MS Shell Dlg");

	const DWORD visibleChild = WS_CHILD | WS_VISIBLE;
	const DWORD editStyle = visibleChild | WS_TABSTOP | WS_BORDER |
				ES_AUTOHSCROLL;

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

	AddControl(builder, visibleChild | WS_TABSTOP | BS_DEFPUSHBUTTON, 185,
		   208, 60, 16, IDOK, ATOM_BUTTON, L"Send report");

	AddControl(builder, visibleChild | WS_TABSTOP | BS_PUSHBUTTON, 253, 208,
		   60, 16, IDCANCEL, ATOM_BUTTON, L"Don't send");
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
