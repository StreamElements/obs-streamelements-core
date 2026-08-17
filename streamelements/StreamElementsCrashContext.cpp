#include "StreamElementsCrashContext.hpp"

#include "cef-headers.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsSecretRedactor.hpp"
#include "StreamElementsUtils.hpp"
#include "deps/zip/zip.h"

#include <util/base.h>
#include <util/platform.h>
#include <obs.h>

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>

#ifdef WIN32
#include "deps/StackWalker/StackWalker.h"
#include <io.h>
#include <windows.h>
#include <winuser.h>
#include <QString>
#else
#include <unistd.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <sys/types.h>

// Windows spellings used throughout the collection below, so the shared code
// does not have to fork for the sake of a type name or an underscore.
typedef unsigned char BYTE;

// Functions rather than macros on purpose: the read loops below keep their
// byte count in a local called `read`, which a macro would rewrite into a call
// on an int.
static inline int _read(int fd, void *buffer, size_t count)
{
	return (int)::read(fd, buffer, count);
}

static inline int _close(int fd)
{
	return ::close(fd);
}
#endif

/* ================================================================= */

//
// Reserved at construction, released the moment collection begins.
//
// A process that died of exhaustion has no room for the configuration zip, the
// JSON or the stack text, so the report it needs most is the one it cannot
// produce. Handing this block back first buys that room. BugSplat reserved 1MB
// for the same purpose and the same size; matched here rather than guessed.
//
// Deliberately allocated with malloc rather than new: it is released from a
// crash path, and free() on a raw block is the smallest possible operation to
// perform there.
//
static const size_t GUARD_BUFFER_SIZE = 1024 * 1024;

static void *s_guardBuffer = nullptr;

void StreamElementsCrashContext::ReleaseGuardBuffer()
{
	void *buffer = s_guardBuffer;

	if (!buffer)
		return;

	s_guardBuffer = nullptr;

	free(buffer);
}

/* ================================================================= */

//
// itoa is an MSVC extension. snprintf is not, and is just as safe here.
//
static inline const char *IntToBuf(int value, char *buffer, size_t size)
{
	snprintf(buffer, size, "%d", value);

	return buffer;
}

/* ================================================================= */

//
// Walks the crashing thread's stack, and decides on the way whether the crash
// is ours to report.
//
// The module list starts as our own two modules and is replaced wholesale by
// the remote settings.json when that fetch succeeds -- which is why the fetch
// happens in the constructor, at plugin startup, and not on the crash path.
//
#ifdef WIN32

static class MyStackWalker : public StackWalker {
public:
	MyStackWalker(int options) : StackWalker(options)
	{
		output.reserve(1024 * 16);

		//////////////////////////////////////////////////////
		// Extract modules of interest from external JSON
		//////////////////////////////////////////////////////
		//
		// JSON format:
		//
		//     {
		//       "obs.live": {
		//         "features": {
		//           "diag": {
		//             "modules" : ["obs-browser"]
		//           }
		//         }
		//       }
		//     }
		//
		//////////////////////////////////////////////////////

		modulesOfInterest.push_back("obs-streamelements-core");
		modulesOfInterest.push_back("obs-streamelements");

		auto httpResponseReceivedCallback = [&](char *data,
							void *userdata,
							char *error_msg,
							int http_code) {
			if (!data)
				return;

			std::string json_string = data;

			CefRefPtr<CefValue> root =
				CefParseJSON(json_string.c_str(),
					     JSON_PARSER_ALLOW_TRAILING_COMMAS);

			if (!root.get() || root->GetType() != VTYPE_DICTIONARY)
				return;

			CefRefPtr<CefDictionaryValue> rootDict =
				root->GetDictionary();

			if (!rootDict->HasKey("obs.live") ||
			    rootDict->GetType("obs.live") != VTYPE_DICTIONARY)
				return;

			CefRefPtr<CefDictionaryValue> obsliveDict =
				rootDict->GetDictionary("obs.live");

			if (!obsliveDict->HasKey("features") ||
			    obsliveDict->GetType("features") !=
				    VTYPE_DICTIONARY)
				return;

			CefRefPtr<CefDictionaryValue> featDict =
				obsliveDict->GetDictionary("features");

			if (!featDict->HasKey("diag") ||
			    featDict->GetType("diag") != VTYPE_DICTIONARY)
				return;

			CefRefPtr<CefDictionaryValue> diagDict =
				featDict->GetDictionary("diag");

			if (!diagDict->HasKey("modules") ||
			    diagDict->GetType("modules") != VTYPE_LIST)
				return;

			CefRefPtr<CefListValue> modulesList =
				diagDict->GetList("modules");

			modulesOfInterest.clear();

			for (int index = 0; index < modulesList->GetSize();
			     ++index) {
				if (modulesList->GetType(index) != VTYPE_STRING)
					continue;

				std::string module =
					modulesList->GetString(index).ToString();

				if (IsTraceLogLevel()) {
					blog(LOG_INFO,
					     "StreamElementsCrashContext: added module of interest: %s",
					     module.c_str());
				}

				modulesOfInterest.push_back(module);
			}
		};

		time_t tv;
		time(&tv);
		time_t gmtv = mktime(gmtime(&tv));

		char url[128];

		sprintf(url,
			"https://obslive-external-assets.streamelements.com/settings.json?_nc=%lu",
			(unsigned long)(gmtv - (gmtv % (time_t)60)));

		http_client_headers_t request_headers;

		HttpGetString(url, request_headers,
			      httpResponseReceivedCallback, nullptr);
	}

protected:
	virtual void OnCallstackEntry(CallstackEntryType eType,
				      CallstackEntry &entry) override
	{
		StackWalker::OnCallstackEntry(eType, entry);

		if (!entry.offset)
			return;

#define LOCAL_SPACE "\t"
		output += entry.loadedImageName;
		output += LOCAL_SPACE;
		output += entry.undFullName;
		output += LOCAL_SPACE;
		output += entry.lineFileName;
		output += " (";
		char numbuf[32];
		output += ltoa(entry.lineNumber, numbuf, 10);
		output += ")";
		output += "\n";
#undef LOCAL_SPACE

		if (!hasMatchModuleOfInterest) {
			for (auto filter : modulesOfInterest) {
				if (strcasecmp(filter.c_str(),
					       entry.moduleName) == 0) {
					hasMatchModuleOfInterest = true;

					break;
				}
			}
		}
	}

public:
	bool hasMatchModuleOfInterest = false;
	std::vector<std::string> modulesOfInterest;
	std::string output;
};

#else

//
// macOS equivalent of the walker above.
//
// backtrace() rather than StackWalker: it is async-signal-safe, needs no symbol
// server, and dladdr gives the owning image per frame, which is all the
// module-of-interest test requires. The symbol names are less complete than a
// Windows walk with a PDB -- the minidump and Sentry's own symbolication cover
// that -- so this exists for the gate and for a human-readable stack.txt.
//
class MyStackWalker {
public:
	MyStackWalker()
	{
		output.reserve(1024 * 16);

		modulesOfInterest.push_back("obs-streamelements-core");
		modulesOfInterest.push_back("obs-streamelements");
	}

	void Walk()
	{
		void *frames[128];

		const int count = backtrace(frames, 128);

		char **symbols = backtrace_symbols(frames, count);

		for (int i = 0; i < count; ++i) {
			Dl_info info;

			const char *image = "";

			if (dladdr(frames[i], &info) && info.dli_fname)
				image = info.dli_fname;

			output += image;
			output += "\t";
			output += (symbols && symbols[i]) ? symbols[i] : "";
			output += "\n";

			if (hasMatchModuleOfInterest || !*image)
				continue;

			const std::string path = image;

			for (auto &filter : modulesOfInterest) {
				if (path.find(filter) != std::string::npos) {
					hasMatchModuleOfInterest = true;

					break;
				}
			}
		}

		if (symbols)
			free(symbols);
	}

public:
	bool hasMatchModuleOfInterest = false;
	std::vector<std::string> modulesOfInterest;
	std::string output;
};

#endif

/* ================================================================= */

struct StreamElementsCrashContext::Impl {
	MyStackWalker *stackWalker = nullptr;
};

/* ================================================================= */

static inline bool GetTemporaryFilePath(const wchar_t *basename,
					const wchar_t *ext,
					std::wstring &wtempBufPath)
{
#ifdef WIN32
	const size_t BUF_LEN = 2048;

	std::vector<wchar_t> pathBuffer;
	pathBuffer.reserve(BUF_LEN);

	if (!::GetTempPathW(BUF_LEN, pathBuffer.data())) {
		return false;
	}

	wtempBufPath = pathBuffer.data();

	if (0 == ::GetTempFileNameW(wtempBufPath.c_str(), L"selive-api-context",
				    0, pathBuffer.data())) {
		return false;
	}

	wtempBufPath = pathBuffer.data();
	wtempBufPath += ext;

	return true;
#else
	// mkstemp rather than tmpnam: it creates the file atomically, so two
	// crashing processes cannot land on the same name. The suffix is
	// appended afterwards because mkstemp owns the tail of the template.
	std::string path = "/tmp/" + wstring_to_utf8(basename) + "-XXXXXX";

	std::vector<char> buffer(path.begin(), path.end());
	buffer.push_back('\0');

	const int fd = mkstemp(buffer.data());

	if (fd < 0)
		return false;

	close(fd);

	wtempBufPath = utf8_to_wstring(std::string(buffer.data()) +
				       wstring_to_utf8(ext));

	return true;
#endif
}

/* ================================================================= */

StreamElementsCrashContext::StreamElementsCrashContext()
{
	// Reserved now, while allocation still works, so that Collect() has
	// something to hand back if the process dies of exhaustion.
	if (!s_guardBuffer)
		s_guardBuffer = malloc(GUARD_BUFFER_SIZE);

	m_impl = new Impl();

#ifdef WIN32
	m_impl->stackWalker = new MyStackWalker(
		StackWalker::RetrieveSymbol | StackWalker::RetrieveLine |
		StackWalker::RetrieveModuleInfo);
#else
	m_impl->stackWalker = new MyStackWalker();
#endif
}

StreamElementsCrashContext::~StreamElementsCrashContext()
{
	// The stack walker is deliberately leaked: an exception filter running
	// on another thread may still be inside WalkStack(), and freeing it
	// here would turn an unrelated crash into a use-after-free inside the
	// crash handler itself.
	delete m_impl;

	m_impl = nullptr;
}

void StreamElementsCrashContext::WalkStack(void *contextRecord)
{
	if (!m_impl || !m_impl->stackWalker)
		return;

#ifdef WIN32
	m_impl->stackWalker->ShowCallstack(::GetCurrentThread(),
					   (PCONTEXT)contextRecord);
#else
	// The context record is ignored: backtrace() walks the calling thread,
	// which on the crash path is the thread that faulted.
	UNUSED_PARAMETER(contextRecord);

	m_impl->stackWalker->Walk();
#endif
}

bool StreamElementsCrashContext::ShouldReport() const
{
	if (!m_impl || !m_impl->stackWalker)
		return false;

	return m_impl->stackWalker->hasMatchModuleOfInterest;
}

/* ================================================================= */

StreamElementsCrashContext::Result StreamElementsCrashContext::Collect()
{
	// First thing, before anything below asks the allocator for a byte.
	ReleaseGuardBuffer();

	Result result;

	const size_t BUF_LEN = 2048;

	std::wstring wtempBufPath;
	if (!GetTemporaryFilePath(L"selive-error-report-data", L".zip",
				  wtempBufPath))
		return result;

	std::string tempBufPath = wstring_to_utf8(wtempBufPath);

	char programDataPathBuf[BUF_LEN];
	int ret = os_get_config_path(programDataPathBuf, BUF_LEN, "obs-studio");

	if (ret <= 0) {
		return result;
	}

#ifdef WIN32
	std::wstring obsDataPath = QString(programDataPathBuf).toStdWString();
#else
	std::wstring obsDataPath = utf8_to_wstring(programDataPathBuf);
#endif

	zip_t *zip = zip_open(tempBufPath.c_str(), 9, 'w');

	if (!zip) {
		return result;
	}

	auto addBufferToZip = [&](BYTE *buf, size_t bufLen,
				  std::wstring zipPath) {
		zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

		zip_entry_write(zip, buf, bufLen);

		zip_entry_close(zip);
	};

	auto addLinesBufferToZip = [&](std::vector<std::string> &lines,
				       std::wstring zipPath) {
		zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

		for (auto line : lines) {
			zip_entry_write(zip, line.c_str(), line.size());
			zip_entry_write(zip, "\r\n", 2);
		}

		zip_entry_close(zip);
	};

	auto addCefValueToZip = [&](CefRefPtr<CefValue> &input,
				    std::wstring zipPath) {
		std::string buf = wstring_to_utf8(
			CefWriteJSON(input, JSON_WRITER_PRETTY_PRINT)
				.ToWString());

		zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

		zip_entry_write(zip, buf.c_str(), buf.size());

		zip_entry_close(zip);
	};

	auto addFileToZip = [&](std::wstring localPath, std::wstring zipPath) {
		// service.json carries the stream key in plaintext, so it is
		// buffered whole and sanitized rather than streamed straight
		// through. It is a few hundred bytes; everything else keeps the
		// chunked path.
		const bool redact =
			StreamElementsSecretRedactor::IsSensitivePath(zipPath);

#ifdef WIN32
		int fd = _wsopen(localPath.c_str(), _O_RDONLY | _O_BINARY,
				 _SH_DENYNO, 0 /*_S_IREAD | _S_IWRITE*/);
#else
		int fd = open(wstring_to_utf8(localPath).c_str(), O_RDONLY);
#endif

		if (-1 != fd) {
			size_t BUF_LEN = 32768;

			BYTE *buf = new BYTE[BUF_LEN];

			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			if (redact) {
				std::string content;

				int read = _read(fd, buf, BUF_LEN);
				while (read > 0) {
					content.append((const char *)buf,
						       (size_t)read);

					read = _read(fd, buf, BUF_LEN);
				}

				const std::string sanitized =
					StreamElementsSecretRedactor::Redact(
						content);

				zip_entry_write(zip, sanitized.c_str(),
						sanitized.size());
			} else {
				int read = _read(fd, buf, BUF_LEN);
				while (read > 0) {
					if (0 !=
					    zip_entry_write(zip, buf, read)) {
						break;
					}

					read = _read(fd, buf, BUF_LEN);
				}
			}

			zip_entry_close(zip);

			delete[] buf;

			_close(fd);
		} else {
			// Failed opening file for reading
			//
			// This is a crash handler: you can't really do anything
			// here to mitigate.
		}
	};

#ifdef WIN32
	auto addWindowCaptureToZip = [&](const HWND &hWnd, int nBitCount,
					 std::wstring zipPath) {
		//calculate the number of color indexes in the color table
		int nColorTableEntries = -1;
		switch (nBitCount) {
		case 1:
			nColorTableEntries = 2;
			break;
		case 4:
			nColorTableEntries = 16;
			break;
		case 8:
			nColorTableEntries = 256;
			break;
		case 16:
		case 24:
		case 32:
			nColorTableEntries = 0;
			break;
		default:
			nColorTableEntries = -1;
			break;
		}

		if (nColorTableEntries == -1) {
			// printf("bad bits-per-pixel argument\n");
			return false;
		}

		HDC hDC = GetDC(hWnd);
		HDC hMemDC = CreateCompatibleDC(hDC);

		int nWidth = 0;
		int nHeight = 0;

		if (hWnd != HWND_DESKTOP) {
			RECT rect;
			GetClientRect(hWnd, &rect);
			nWidth = rect.right - rect.left;
			nHeight = rect.bottom - rect.top;
		} else {
			nWidth = ::GetSystemMetrics(SM_CXSCREEN);
			nHeight = ::GetSystemMetrics(SM_CYSCREEN);
		}

		HBITMAP hBMP = CreateCompatibleBitmap(hDC, nWidth, nHeight);
		SelectObject(hMemDC, hBMP);
		BitBlt(hMemDC, 0, 0, nWidth, nHeight, hDC, 0, 0, SRCCOPY);

		int nStructLength = sizeof(BITMAPINFOHEADER) +
				    sizeof(RGBQUAD) * nColorTableEntries;
		LPBITMAPINFOHEADER lpBitmapInfoHeader =
			(LPBITMAPINFOHEADER) new char[nStructLength];
		::ZeroMemory(lpBitmapInfoHeader, nStructLength);

		lpBitmapInfoHeader->biSize = sizeof(BITMAPINFOHEADER);
		lpBitmapInfoHeader->biWidth = nWidth;
		lpBitmapInfoHeader->biHeight = nHeight;
		lpBitmapInfoHeader->biPlanes = 1;
		lpBitmapInfoHeader->biBitCount = nBitCount;
		lpBitmapInfoHeader->biCompression = BI_RGB;
		lpBitmapInfoHeader->biXPelsPerMeter = 0;
		lpBitmapInfoHeader->biYPelsPerMeter = 0;
		lpBitmapInfoHeader->biClrUsed = nColorTableEntries;
		lpBitmapInfoHeader->biClrImportant = nColorTableEntries;

		DWORD dwBytes = ((DWORD)nWidth * nBitCount) / 32;
		if (((DWORD)nWidth * nBitCount) % 32) {
			dwBytes++;
		}
		dwBytes *= 4;

		DWORD dwSizeImage = dwBytes * nHeight;
		lpBitmapInfoHeader->biSizeImage = dwSizeImage;

		LPBYTE lpDibBits = 0;
		HBITMAP hBitmap = ::CreateDIBSection(
			hMemDC, (LPBITMAPINFO)lpBitmapInfoHeader,
			DIB_RGB_COLORS, (void **)&lpDibBits, NULL, 0);
		SelectObject(hMemDC, hBitmap);
		BitBlt(hMemDC, 0, 0, nWidth, nHeight, hDC, 0, 0, SRCCOPY);
		ReleaseDC(hWnd, hDC);

		BITMAPFILEHEADER bmfh;
		bmfh.bfType = 0x4d42; // 'BM'
		int nHeaderSize = sizeof(BITMAPINFOHEADER) +
				  sizeof(RGBQUAD) * nColorTableEntries;
		bmfh.bfSize = 0;
		bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
		bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) +
				 sizeof(BITMAPINFOHEADER) +
				 sizeof(RGBQUAD) * nColorTableEntries;

		zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

		DWORD nColorTableSize = 0;
		if (nBitCount != 24) {
			nColorTableSize = (1ULL << nBitCount) * sizeof(RGBQUAD);
		} else {
			nColorTableSize = 0L;
		}

		zip_entry_write(zip, &bmfh, sizeof(BITMAPFILEHEADER));
		zip_entry_write(zip, lpBitmapInfoHeader, nHeaderSize);

		if (nBitCount < 16) {
			//int nBytesWritten = 0;
			RGBQUAD *rgbTable = new RGBQUAD[nColorTableEntries *
							sizeof(RGBQUAD)];
			//fill RGBQUAD table and write it in file
			for (int i = 0; i < nColorTableEntries; ++i) {
				rgbTable[i].rgbRed = rgbTable[i].rgbGreen =
					rgbTable[i].rgbBlue = i;
				rgbTable[i].rgbReserved = 0;

				zip_entry_write(zip, &rgbTable[i],
						sizeof(RGBQUAD));
			}

			delete[] rgbTable;
		}

		zip_entry_write(zip, lpDibBits, dwSizeImage);

		zip_entry_close(zip);

		::DeleteObject(hBMP);
		::DeleteObject(hBitmap);
		delete[] lpBitmapInfoHeader;

		return true;
	};
#endif

	std::string package_manifest = "generator=crash_handler\nversion=4\n";
	addBufferToZip((BYTE *)package_manifest.c_str(),
		       package_manifest.size(), L"manifest.ini");

	//
	// OBS's own text crash report -- always empty, and structurally so.
	//
	// libobs' crash handler writes its log and then exits the process
	// immediately: it never returns to its caller. So there is no point at
	// which we could capture what it wrote and still be running to put it
	// into this zip.
	//
	// That is precisely why our exception filter does all of its own work
	// first and only then chains downstream -- see CustomExceptionFilter in
	// StreamElementsBugSplatCrashHandler.cpp, where the call to
	// s_prevExceptionFilter carries the note "This will exit(0)".
	//
	// The entry is kept so the zip layout does not change. "crashes/" is
	// blacklisted below as well, but that would not help either: those are
	// earlier crashes' logs, and the current one does not exist yet at this
	// point.
	//
	static const std::string obsCrashLog;

	addBufferToZip((BYTE *)obsCrashLog.c_str(), obsCrashLog.size(),
		       L"obs-studio/crashes/crash.log");

	// Add window capture
	//
	// Windows only. The equivalent on macOS is CGWindowListCreateImage,
	// which since Catalina requires the Screen Recording entitlement --
	// and if it has not already been granted, the first call puts up a
	// system permission prompt. Doing that from a crashing process would
	// be both useless and hostile, so macOS reports go without.
#ifdef WIN32
	//
	// Guarded, because this runs while the process is dying and a crash
	// during shutdown is one of the cases worth reporting most. By then the
	// global state manager is gone, and dereferencing it here would fault
	// inside the crash handler -- losing the whole report rather than just
	// the screenshot.
	//
	if (StreamElementsGlobalStateManager::IsInstanceAvailable()) {
		auto mainWindow =
			StreamElementsGlobalStateManager::GetInstance()
				->mainWindow();

		if (mainWindow) {
			addWindowCaptureToZip((HWND)mainWindow->winId(), 24,
					      L"obs-main-window.bmp");
		}
	}
#endif

	// Hard ceilings on what the configuration archive may contain, applied on
	// top of the blacklist rather than instead of it.
	//
	// The blacklist only excludes what someone thought to name, and the cost
	// of missing one is not a slightly larger archive: Sentry rejects an
	// envelope over 40MB outright, so a single unlisted directory silently
	// costs every crash report. That is not hypothetical -- a mis-spelled
	// entry and a handful of Chromium component directories had the archive at
	// 62MB, and no report had ever arrived.
	//
	// Sized against the observed archive with the blacklist corrected (~5MB).
	// Anything approaching these limits is a new cache directory to blacklist,
	// not a budget to raise.
	//
	// Files under the auxiliary plugins folder are the one deliberate
	// exception. They are third-party executables -- Sentry has no symbols
	// for them, so their bytes are the only way to resolve a frame in
	// someone else's plugin -- and a plugin binary legitimately runs to tens
	// of megabytes, which the general per-file limit exists to reject. They
	// get their own, much higher ceiling instead of being exempt outright, so
	// one pathological file still cannot swallow the archive.
	//
	// The total is raised to match: shipping plugin binaries and then
	// truncating them at 24MB would be the worst of both. It stays well
	// inside Sentry's 40MB *compressed* envelope cap -- binaries compress
	// roughly 2-3x, so a full 40MB of them lands near 16MB, leaving room for
	// the ~6.5MB minidump.
	const uintmax_t maxFileBytes = 4ull * 1024 * 1024;
	const uintmax_t maxPluginFileBytes = 32ull * 1024 * 1024;
	const uintmax_t maxTotalBytes = 40ull * 1024 * 1024;

	struct CandidateFile {
		std::wstring localPath;
		std::wstring zipPath;
		uintmax_t size;

		// Under the auxiliary plugins folder, so subject to
		// maxPluginFileBytes and taken last -- see the sort below.
		bool isPlugin;
	};

	std::vector<CandidateFile> candidates;

	// Collect files
	//
	// Compared against the lowercased, forward-slashed relative path, so every
	// entry here must be lowercase or it silently matches nothing.
	std::vector<std::wstring> blacklist = {
		// The whole CEF profile, not the two dozen individual entries this
		// replaces. It is almost entirely Chromium's own state -- caches,
		// component-updater payloads, leveldb stores -- which is large,
		// reproducible from a fresh profile, and evidence of nothing. The
		// enumerated form was also unmaintainable in the specific way that
		// matters here: Chromium adds directories on demand, so the list
		// was permanently one release behind, and each miss was invisible
		// because the archive was still produced and still reported sent.
		// component_crx_cache, WidevineCdm and OnDeviceHeadSuggestModel
		// alone were ~42MB of a 62MB archive.
		L"plugin_config/obs-browser/",
		// Both spellings, because both directories exist on a machine
		// that has been through the rename: the ~17MB updater sits in
		// obs-streamelements, and a second ~16MB copy in
		// obs-streamelements-core. Only the first was ever listed, so the
		// newer copy went into every report.
		L"plugin_config/obs-streamelements/obs-streamelements-update.exe",
		L"plugin_config/obs-streamelements-core/obs-streamelements-update.exe",
		// The Sentry SDK's own database, which lives here rather than
		// beside the binary so a non-elevated user can write it. It holds
		// the minidumps of previous crashes, so without this every report
		// carries the ones before it. No trailing slash: the same prefix
		// covers the timestamped copies left by a manual reset.
		L"plugin_config/obs-streamelements-core/sentry-db",
		// Our own binaries, and only ours.
		//
		// CI uploads obs-streamelements-core's symbols *and* its code
		// file to Sentry, so Sentry resolves our frames on its own;
		// shipping the bytes a second time buys nothing and costs
		// megabytes. Third-party plugins are the opposite case -- Sentry
		// holds no symbols for them, so the binary in this archive is
		// the only way to resolve their frames by hand, which is why
		// they are deliberately kept and exempted from the per-file
		// limit below.
		//
		// This was previously true only by accident: our binary happens
		// to exceed the per-file limit, so it was dropped as if it were
		// a cache file. A stripped build coming in under the limit would
		// silently have started shipping it.
		//
		// Both layouts of the auxiliary plugins folder. The Windows
		// installer currently writes into the OBS install directory,
		// which is outside this tree entirely, so that entry is only
		// insurance against the layout changing.
		L"plugins/obs-streamelements-core.plugin/contents/macos/",
		L"plugins/obs-streamelements-core/bin/",
		L"updates/",
		L"profiler_data/",
		L"obslive_restored_files/",
		L"crashes/"};

	// Collect all files
	for (auto &i : std::filesystem::recursive_directory_iterator(
		     programDataPathBuf)) {
		if (!std::filesystem::is_directory(i.path())) {
			std::wstring local_path = i.path().wstring();
			std::wstring zip_path =
				local_path.substr(obsDataPath.size() + 1);

			std::wstring zip_path_lcase = zip_path;
			std::transform(zip_path_lcase.begin(),
				       zip_path_lcase.end(),
				       zip_path_lcase.begin(), ::towlower);
			std::transform(zip_path_lcase.begin(),
				       zip_path_lcase.end(),
				       zip_path_lcase.begin(), [](wchar_t ch) {
					       return ch == L'\\' ? L'/' : ch;
				       });

			bool accept = true;
			for (auto item : blacklist) {
				if (zip_path_lcase.size() >= item.size()) {
					if (zip_path_lcase.substr(
						    0, item.size()) == item) {
						accept = false;

						break;
					}
				}
			}

			if (accept) {
				std::error_code ec;
				const uintmax_t size = i.file_size(ec);

				// The auxiliary plugins folder, which is inside
				// the configuration tree on both platforms:
				// ~/Library/Application Support/obs-studio/
				// plugins on macOS, %APPDATA%\obs-studio\plugins
				// on Windows. Ours is already excluded by the
				// blacklist, so whatever is left here is a
				// third-party plugin.
				const std::wstring pluginsPrefix = L"plugins/";
				const bool isPlugin =
					zip_path_lcase.size() >=
						pluginsPrefix.size() &&
					zip_path_lcase.substr(
						0, pluginsPrefix.size()) ==
						pluginsPrefix;

				candidates.push_back(
					{local_path, L"obs-studio\\" + zip_path,
					 ec ? 0 : size, isPlugin});
			}
		}
	}

	// Everything else before the plugins, and smallest first within each, so
	// the budget buys the largest number of files and a single huge one can
	// never crowd out the small text files that carry almost all of the
	// diagnostic value.
	//
	// Plugin binaries are the reason for the two tiers rather than one sort
	// by size. They are wanted, but they are also the only things here large
	// enough to exhaust the budget, so they take what is left after the
	// configuration tree -- a few hundred KB -- rather than competing with
	// it. Sorting purely by size would have let a handful of plugins push
	// out the text files instead.
	std::sort(candidates.begin(), candidates.end(),
		  [](const CandidateFile &a, const CandidateFile &b) {
			  if (a.isPlugin != b.isPlugin)
				  return !a.isPlugin;

			  return a.size < b.size;
		  });

	std::vector<std::string> omitted;
	uintmax_t totalBytes = 0;

	for (auto &candidate : candidates) {
		const uintmax_t fileLimit = candidate.isPlugin
						    ? maxPluginFileBytes
						    : maxFileBytes;

		const bool tooBig = candidate.size > fileLimit;
		const bool overBudget = totalBytes + candidate.size >
					maxTotalBytes;

		if (tooBig || overBudget) {
			omitted.push_back(
				wstring_to_utf8(candidate.zipPath) + " (" +
				std::to_string(candidate.size) + " bytes, " +
				(tooBig ? "over per-file limit"
					: "archive budget exhausted") +
				")");

			continue;
		}

		totalBytes += candidate.size;

		addFileToZip(candidate.localPath, candidate.zipPath);
	}

	// Always written, even when nothing was dropped. A truncated archive that
	// does not say it was truncated reads as a complete picture of the
	// machine, and someone will eventually conclude a file was missing when it
	// was only omitted.
	{
		std::vector<std::string> manifest;

		manifest.push_back(
			"Files collected: " +
			std::to_string(candidates.size() - omitted.size()) +
			" (" + std::to_string(totalBytes) + " bytes)");
		manifest.push_back("Files omitted: " +
				   std::to_string(omitted.size()));
		manifest.push_back("Per-file limit: " +
				   std::to_string(maxFileBytes) + " bytes");
		manifest.push_back("Per-file limit (plugins): " +
				   std::to_string(maxPluginFileBytes) +
				   " bytes");
		manifest.push_back("Archive budget: " +
				   std::to_string(maxTotalBytes) + " bytes");
		manifest.push_back("");

		for (auto &line : omitted) {
			manifest.push_back(line);
		}

		addLinesBufferToZip(manifest, L"omitted-files.txt");
	}

	{
		CefRefPtr<CefValue> basicInfo = CefValue::Create();
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		basicInfo->SetDictionary(d);

		d->SetString("obsVersion", obs_get_version_string());
		d->SetString("cefVersion", GetCefVersionString());
		d->SetString("cefApiHash", GetCefPlatformApiHash());

#ifdef WIN32
		d->SetString("platform", "windows");
#elif defined(__APPLE__)
		d->SetString("platform", "macos");
#elif defined(__linux__)
		d->SetString("platform", "linux");
#endif

		if (sizeof(void *) == 8) {
			d->SetString("platformArch", "64bit");
		} else {
			d->SetString("platformArch", "32bit");
		}

		d->SetString("streamelementsPluginVersion",
			     GetStreamElementsPluginVersionString());

		d->SetString("machineUniqueId", GetComputerSystemUniqueId());

		addCefValueToZip(basicInfo, L"system\\basic.json");
	}

	{
		CefRefPtr<CefValue> sysHardwareInfo = CefValue::Create();

		SerializeSystemHardwareProperties(sysHardwareInfo);

		addCefValueToZip(sysHardwareInfo, L"system\\hardware.json");
	}

	{
		CefRefPtr<CefValue> sysMemoryInfo = CefValue::Create();

		SerializeSystemMemoryUsage(sysMemoryInfo);

		addCefValueToZip(sysMemoryInfo, L"system\\memory.json");
	}

	// Same guard as the window capture above: gone during shutdown, and a
	// null dereference here would cost the entire report.
	auto performanceTracker =
		StreamElementsGlobalStateManager::IsInstanceAvailable()
			? StreamElementsGlobalStateManager::GetInstance()
				  ->GetPerformanceHistoryTracker()
			: nullptr;

	if (performanceTracker) {
		// Histogram CPU & memory usage (past hour, 1 minute intervals)

		auto cpuUsageHistory =
			performanceTracker->getCpuUsageSnapshot();

		auto memoryUsageHistory =
			performanceTracker->getMemoryUsageSnapshot();

		char lineBuf[512];

		{
			std::vector<std::string> lines;

			lines.push_back("totalSeconds,busySeconds,idleSeconds");
			for (auto item : cpuUsageHistory) {
				sprintf(lineBuf, "%1.2Lf,%1.2Lf,%1.2Lf",
					item.totalSeconds, item.busySeconds,
					item.idleSeconds);

				lines.push_back(lineBuf);
			}

			addLinesBufferToZip(lines,
					    L"system\\usage_history_cpu.csv");
		}

		{
			std::vector<std::string> lines;

			lines.push_back("totalSeconds,memoryUsedPercentage");

			size_t index = 0;
			for (auto item : memoryUsageHistory) {
				if (index < cpuUsageHistory.size()) {
					auto totalSec = cpuUsageHistory[index]
								.totalSeconds;

					sprintf(lineBuf, "%1.2Lf,%d", totalSec,
						item.dwMemoryLoad // % Used
					);
				} else {
					sprintf(lineBuf, "%1.2Lf,%d", 0.0L,
						item.dwMemoryLoad // % Used
					);
				}

				lines.push_back(lineBuf);

				++index;
			}

			addLinesBufferToZip(
				lines, L"system\\usage_history_memory.csv");
		}
	}

	TryGetAsyncCallContextStack([&](const StreamElementsAsyncCallContextStack_t
						*asyncCallContextStack) {
		if (!asyncCallContextStack->size())
			return;

		std::vector<std::string> lines;

		lines.push_back("Asynchronous call context:");
		lines.push_back("");

		for (auto item : *asyncCallContextStack) {
			char buf[32];
			lines.push_back(item->file.c_str() +
					std::string(" (line: ") +
					std::string(IntToBuf(item->line, buf,
							     sizeof(buf))) +
					")");
		}

		addLinesBufferToZip(lines, L"async_context.txt");

		// Fill in async call details notes

		auto asyncContextList = CefListValue::Create();

		std::string notes = "ASYNC";

		notes += " ---- Asynchronous call context:";

		for (auto item : *asyncCallContextStack) {
			char buf[32];
			notes += std::string(" ---- ") + item->file +
				 std::string(" (line: ") +
				 std::string(IntToBuf(item->line, buf,
						      sizeof(buf))) +
				 ")\r\n";

			auto d = CefDictionaryValue::Create();

			d->SetString("file", item->file);
			d->SetInt("line", item->line);
			d->SetInt("thread", item->thread);
			d->SetBool("running", item->running);

			asyncContextList->SetDictionary(
				asyncContextList->GetSize(), d);
		}

		result.notes = notes;

		// Retrieve temporary path
		std::wstring wtempBufPath;
		if (!GetTemporaryFilePath(L"async-context", L".json",
					  wtempBufPath))
			return;

		auto asyncContextListValue = CefValue::Create();

		asyncContextListValue->SetList(asyncContextList);

		auto json = CefWriteJSON(asyncContextListValue,
					 JSON_WRITER_PRETTY_PRINT)
				    .ToString();

		os_quick_write_utf8_file(wstring_to_utf8(wtempBufPath).c_str(),
					 json.c_str(), json.size(), false);

		result.attachments.push_back({wstring_to_utf8(wtempBufPath)});
	});

	zip_close(zip);

	result.attachments.push_back({tempBufPath});

	////////////////////////////////////////////////////////////////////
	// Process ApiContext
	////////////////////////////////////////////////////////////////////

	result.attributes.push_back({"product", "SE.Live"});

	TryGetApiContext([&](StreamElementsApiContext_t *apiContext) {
		auto apiContextList = CefListValue::Create();

		{
			char buf[64];
			sprintf(buf, "%d", int(apiContext->size()));
			result.attributes.push_back({"selive.api.calls", buf});
		}

		int apiContextIndex = 0;
		for (auto api : *apiContext) {
			char buf[64];
			sprintf(buf, "selive.api.%d", apiContextIndex);
			std::string attrPrefix = buf;

			result.attributes.push_back(
				{attrPrefix + ".method",
				 api.get()->method.ToString()});

			///////////////////////////////////////////////////////////////////////////
			//
			// BugSplat generates invalid XML report with long string atrribute values
			//
			// We'll disable the following section until it is fixed in a future
			// version of their SDK.
			//
			///////////////////////////////////////////////////////////////////////////

			/*
			auto argsValue = CefValue::Create();
			argsValue->SetList(api.get()->args);

			auto ba = QUrl::toPercentEncoding(
				CefWriteJSON(argsValue, JSON_WRITER_DEFAULT)
					.ToString()
					.c_str(),
				"", "[](){}\n\r\t");

			result.attributes.push_back(
				{attrPrefix + ".args", ba.constData()});
			*/

			auto apiContextListItem = CefDictionaryValue::Create();

			apiContextListItem->SetString("invoke",
						      api.get()->method);
			apiContextListItem->SetList("invokeArgs",
						    api.get()->args);
			apiContextListItem->SetInt("thread", api.get()->thread);

			apiContextList->SetDictionary(apiContextList->GetSize(),
						      apiContextListItem);

			++apiContextIndex;
		}

		// Retrieve temporary path
		std::wstring wtempBufPath;
		if (!GetTemporaryFilePath(L"api-context", L".json",
					  wtempBufPath))
			return;

		auto apiContextListValue = CefValue::Create();

		apiContextListValue->SetList(apiContextList);

		auto json = CefWriteJSON(apiContextListValue,
					 JSON_WRITER_PRETTY_PRINT)
				    .ToString();

		os_quick_write_utf8_file(wstring_to_utf8(wtempBufPath).c_str(),
					 json.c_str(), json.size(), false);

		result.attachments.push_back({wstring_to_utf8(wtempBufPath)});
	});

	if (m_impl && m_impl->stackWalker &&
	    m_impl->stackWalker->output.size()) {
		// Retrieve temporary path
		std::wstring wtempBufPath;
		if (!GetTemporaryFilePath(L"stack", L".txt", wtempBufPath))
			return result;

		os_quick_write_utf8_file(wstring_to_utf8(wtempBufPath).c_str(),
					 m_impl->stackWalker->output.c_str(),
					 m_impl->stackWalker->output.size(),
					 false);

		result.attachments.push_back({wstring_to_utf8(wtempBufPath)});
	}

	return result;
}
