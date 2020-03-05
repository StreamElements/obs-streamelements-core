#include "ScriptEngine.hpp"

#include <windows.h>
#include <scriptstdstring/scriptstdstring.h>

#include <stdio.h>

#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

#include <codecvt>
#include <algorithm>
#include <fstream>

static inline void path_transform(std::wstring &path)
{
	std::transform(path.begin(), path.end(), path.begin(), [](wchar_t ch) {
		if (ch == L'/')
			return L'\\';
		else
			return ch;
	});
}

static std::string quick_read_text_file(std::string path)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wPath = myconv.from_bytes(path);

	std::ifstream file;
	file.open(wPath, std::ifstream::in);

	std::string result;

	file.seekg(0, std::ios_base::end);
	std::streampos length = file.tellg();
	file.seekg(0, std::ios_base::beg);

	result.resize((size_t)length + 1);
	file.read((char*)result.data(), length);
	file.close();

	return result;
}

static bool shell_execute(std::string &path, std::string &args,
			  std::string &folder)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wPath = myconv.from_bytes(path);
	std::wstring wArgs = myconv.from_bytes(args);
	std::wstring wFolder = myconv.from_bytes(folder);

	path_transform(wPath);
	path_transform(wFolder);

	HINSTANCE retVal = ShellExecuteW(0, 0, wPath.c_str(), wArgs.c_str(),
					 wFolder.c_str(), SW_SHOW);

	return ((int)retVal > 32);
}

static bool wait_pid(uint64_t pid, uint64_t waitMilliseconds)
{
	HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);

	if (NULL == hProcess)
		return true;

	bool result = (WAIT_OBJECT_0 ==
		       WaitForSingleObject(hProcess, waitMilliseconds > 0
							     ? waitMilliseconds
							     : INFINITE));

	CloseHandle(hProcess);

	return result;
}

static bool filesystem_move(std::string &from, std::string &to)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wFrom = myconv.from_bytes(from);
	std::wstring wTo = myconv.from_bytes(to);
	std::wstring wTitle = L"Moving files...";

	path_transform(wFrom);
	path_transform(wTo);

	std::vector<wchar_t> vFrom;
	std::vector<wchar_t> vTo;

	vFrom.resize(wFrom.size());
	vTo.resize(wTo.size());

	std::copy(wFrom.begin(), wFrom.end(), vFrom.begin());
	std::copy(wTo.begin(), wTo.end(), vTo.begin());

	vFrom.push_back(0);
	vFrom.push_back(0);
	vTo.push_back(0);
	vTo.push_back(0);

	SHFILEOPSTRUCTW op;

	op.hwnd = 0;
	op.wFunc = FO_MOVE;
	op.pFrom = vFrom.data();
	op.pTo = vTo.data();
	op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR |
		    FOF_SIMPLEPROGRESS | /* FOF_NOERRORUI |*/ 0L;
	op.fAnyOperationsAborted = FALSE;
	op.hNameMappings = NULL;
	op.lpszProgressTitle = wTitle.c_str();

	SHFileOperation(&op);

	return !op.fAnyOperationsAborted;
}

static bool filesystem_copy(std::string &from, std::string &to)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wFrom = myconv.from_bytes(from);
	std::wstring wTo = myconv.from_bytes(to);
	std::wstring wTitle = L"Moving files...";

	path_transform(wFrom);
	path_transform(wTo);

	std::vector<wchar_t> vFrom;
	std::vector<wchar_t> vTo;

	vFrom.resize(wFrom.size());
	vTo.resize(wTo.size());

	std::copy(wFrom.begin(), wFrom.end(), vFrom.begin());
	std::copy(wTo.begin(), wTo.end(), vTo.begin());

	vFrom.push_back(0);
	vFrom.push_back(0);
	vTo.push_back(0);
	vTo.push_back(0);

	SHFILEOPSTRUCTW op;

	op.hwnd = 0;
	op.wFunc = FO_COPY;
	op.pFrom = vFrom.data();
	op.pTo = vTo.data();
	op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR |
		    FOF_SIMPLEPROGRESS | /* FOF_NOERRORUI |*/ 0L;
	op.fAnyOperationsAborted = FALSE;
	op.hNameMappings = NULL;
	op.lpszProgressTitle = wTitle.c_str();

	SHFileOperation(&op);

	return !op.fAnyOperationsAborted;
}

static bool ui_confirm(std::string& message, std::string& title)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wMsg = myconv.from_bytes(message);
	std::wstring wTitle = myconv.from_bytes(title);

	return IDYES ==
	       ::MessageBoxW(GetActiveWindow(), wMsg.c_str(), wTitle.c_str(),
			     MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 |
				     MB_SETFOREGROUND | MB_TOPMOST);
}

static void ui_alert(std::string &message, std::string &title)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wMsg = myconv.from_bytes(message);
	std::wstring wTitle = myconv.from_bytes(title);

	::MessageBoxW(GetActiveWindow(), wMsg.c_str(), wTitle.c_str(),
		      MB_OK | MB_ICONEXCLAMATION | MB_DEFBUTTON1 |
			      MB_SETFOREGROUND | MB_TOPMOST);
}

static void ui_error(std::string &message, std::string &title)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wMsg = myconv.from_bytes(message);
	std::wstring wTitle = myconv.from_bytes(title);

	::MessageBoxW(GetActiveWindow(), wMsg.c_str(), wTitle.c_str(),
		      MB_OK | MB_ICONERROR | MB_DEFBUTTON1 | MB_SETFOREGROUND |
			      MB_TOPMOST);
}

// Print the script string to the standard output stream
static void print(std::string &msg)
{
	printf("%s", msg.c_str());
}

bool filesystem_delete(std::string &from)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wFrom = myconv.from_bytes(from);
	std::wstring wTitle = L"Deleting files...";

	path_transform(wFrom);

	std::vector<wchar_t> vFrom;

	vFrom.resize(wFrom.size());

	std::copy(wFrom.begin(), wFrom.end(), vFrom.begin());

	vFrom.push_back(0);
	vFrom.push_back(0);

	SHFILEOPSTRUCTW op;

	op.hwnd = 0;
	op.wFunc = FO_DELETE;
	op.pFrom = vFrom.data();
	op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR |
		    FOF_SIMPLEPROGRESS | FOF_NOERRORUI | 0L;
	op.fAnyOperationsAborted = FALSE;
	op.hNameMappings = NULL;
	op.lpszProgressTitle = wTitle.c_str();

	SHFileOperation(&op);

	return !op.fAnyOperationsAborted;
}

// Implement a simple message callback function
static void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if (msg->type == asMSGTYPE_WARNING)
		type = "WARN";
	else if (msg->type == asMSGTYPE_INFORMATION)
		type = "INFO";
	printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col,
	       type, msg->message);
}

ScriptEngine::ScriptEngine()
{
	// Create the script engine
	m_engine = asCreateScriptEngine();

	// Set the message callback to receive information on errors in human readable form.
	m_engine->SetMessageCallback(asFUNCTION(MessageCallback), 0,
				     asCALL_CDECL);

	// AngelScript doesn't have a built-in string type, as there is no definite standard
	// string type for C++ applications. Every developer is free to register its own string type.
	// The SDK do however provide a standard add-on for registering a string type, so it's not
	// necessary to implement the registration yourself if you don't want to.
	RegisterStdString(m_engine);

	// Register the function that we want the scripts to call
	m_engine->RegisterGlobalFunction("void print(const string &in)",
					 asFUNCTION(print), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"bool filesystem_move(const string from, const string to)",
		asFUNCTION(filesystem_move), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"bool filesystem_copy(const string from, const string to)",
		asFUNCTION(filesystem_copy), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"bool filesystem_delete(const string from)",
		asFUNCTION(filesystem_delete), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"bool wait_pid(const uint64 pid, const uint64 waitMilliseconds)",
		asFUNCTION(wait_pid), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"bool shell_execute(const string path, const string args, const string folder)",
		asFUNCTION(shell_execute), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"bool ui_confirm(const string message, const string title)",
		asFUNCTION(ui_confirm), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"void ui_alert(const string message, const string title)",
		asFUNCTION(ui_alert), asCALL_CDECL);

	m_engine->RegisterGlobalFunction(
		"void ui_error(const string message, const string title)",
		asFUNCTION(ui_error), asCALL_CDECL);
}

ScriptEngine::~ScriptEngine()
{
	m_engine->ShutDownAndRelease();
}

bool ScriptEngine::ExecuteString(const char *script)
{
	m_lastErrorText.clear();

	CScriptBuilder builder;

	if (builder.StartNewModule(m_engine, "MainModule") < 0)
		return false;

	if (builder.AddSectionFromMemory("MainSection", script) < 0)
		return false;

	if (builder.BuildModule() < 0)
		return false;

	bool result = false;

	asIScriptModule *mod = m_engine->GetModule("MainModule");

	if (mod) {
		asIScriptFunction *func = mod->GetFunctionByDecl("void main()");

		if (func) {
			asIScriptContext *ctx = m_engine->CreateContext();

			if (ctx) {
				ctx->Prepare(func);

				int retVal = ctx->Execute();

				if (retVal == asEXECUTION_FINISHED) {
					result = true;
				} else if (retVal == asEXECUTION_EXCEPTION) {
					m_lastErrorText =
						ctx->GetExceptionString();
				}

				ctx->Release();
			}
		}

		mod->Discard();
	}

	return result;
}

bool ScriptEngine::ExecuteFile(const char *path)
{
	return ExecuteString(quick_read_text_file(path).c_str());
}
