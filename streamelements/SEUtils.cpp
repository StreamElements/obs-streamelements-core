#include <string.h>

#include "SELocalFilesystemHttpServer.hpp"
#include "SEUtils.hpp"
#include "SEConfig.hpp"
#include "SEGlobalStateManager.hpp"
#include "SERemoteIconLoader.hpp"
#include "SEPleaseWaitWindow.hpp"
#include "Version.hpp"
#include "wide-string.hpp"
#include "deps/utf8.h"

#define GLOBAL_ENV_CONFIG_FILE_NAME "obs-studio/streamelements-env.ini"

#include <cstdint>
#include <atomic>
#include <chrono>
#include <vector>
#include <regex>
#include <unordered_map>
#include <filesystem>
#include <regex>

#include <curl/curl.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <obs.hpp>

#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QApplication>
#include <QProcess>
#include <QLatin1String>
#include <QStringList>

#include "deps/picosha2/picosha2.h"

#ifndef WIN32
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

#ifndef WIN32
#define sprintf_s sprintf

#ifndef WCHAR
typedef wchar_t WCHAR;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef HKEY
typedef void *HKEY;
#endif

#include <sys/syscall.h>
#endif

/* ========================================================= */


/* ========================================================= */

static config_t *obs_fe_user_config()
{
	auto config = SEConfig::GetInstance();

	if (config) {
		return config->GetObsUserConfig();
	}

	return nullptr;
}

/* ========================================================= */


/* ========================================================= */


/* ========================================================= */

bool IsTraceLogLevel()
{
	static bool has_result = false;
	static bool result = false;

	if (!has_result) {
		std::string search = "--setrace";

		QStringList args = QCoreApplication::instance()->arguments();

		for (int i = 0; i < args.size() && !has_result; ++i) {
			std::string arg = args.at(i).toStdString();

			if (arg.substr(0, search.size()) == search) {
				result = true;
				break;
			}
		}
	}

	has_result = true;

	return result;
}

/* ========================================================= */

// convert wstring to UTF-8 string
static std::vector<std::string> tokenizeString(const std::string &str,
					       const std::string &delimiters)
{
	std::vector<std::string> tokens;
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos ||
	       std::string::npos !=
		       lastPos) { // Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
	return tokens;
}

/* ========================================================= */


/* ========================================================= */


std::future<void> __QtDelayTask_Impl(std::function<void()> task, int delayMs,
				     const char *file, const int line)
{
	auto promise = std::make_shared<std::promise<void>>();

	auto item = AsyncCallContextPush(file, line, false);

	QTimer::singleShot(std::chrono::milliseconds(delayMs), qApp, [=]() {
		// Same reasoning as __QtPostTask_Impl: cleanup must survive a
		// throwing task, or the context entry leaks into every later
		// crash report.
		auto finish = [=]() {
			AsyncCallContextRemove(item);

			promise->set_value();
		};

		// Same gate as __QtPostTask_Impl, and for the same reason
		// (CORE-862). A timer fires through the same event dispatcher,
		// so a Win32 modal loop delivers it just as readily.
		if (IsCrashReportingInProgress()) {
			finish();

			return;
		}

		item->running = true;

		try {
			task();
		} catch (...) {
			finish();

			throw;
		}

		finish();
	});

	return promise->get_future();
}

std::future<void> __QtPostTask_Impl(std::function<void()> task,
				    std::string file, int line)
{
	std::shared_ptr<std::promise<void>> promise =
		std::make_shared<std::promise<void>>();

	auto item = AsyncCallContextPush(file, line, false);

	auto executor = [=]() {
		// The entry has to come off the stack, and the waiter has to be
		// released, even if the task throws. Otherwise the entry stays
		// for the life of the process and appears in every later crash
		// report, and any QtExecSync blocked on this never learns it
		// finished.
		auto finish = [=]() {
			AsyncCallContextRemove(item);

			promise->set_value();
		};

		//
		// Nothing queued before a crash may run after it (CORE-862).
		//
		// The consent prompt is a native Win32 modal dialog, and every
		// Win32 modal loop calls DispatchMessage on all messages --
		// including the private one Qt's event dispatcher posts to its
		// own hidden window to drain the posted-event queue. So merely
		// putting the prompt up runs whatever was queued here, inside
		// the crash handler, on the crashing thread.
		//
		// When one of those tasks calls QDialog::exec(), its nested
		// event loop sits on top of the prompt's loop and the crash
		// path cannot continue until that unrelated dialog is answered
		// -- which the user has no reason to connect to the crash
		// prompt, and may never see behind a topmost window. Observed:
		// the prompt was answered and hidden while DialogBox2 was still
		// on the stack, unable to return.
		//
		// finish() still runs. The point is not to strand anyone: a
		// QtExecSync caller is blocked in result.wait() on another
		// thread, and dropping the task without releasing it would wedge
		// that thread too.
		//
		// This is the same single-choke-point shape as
		// SEDrainEventQueue() (CORE-786), and it covers only OUR queued
		// work -- OBS's own posted calls and other plug-ins' can still
		// open a dialog inside the prompt.
		//
		if (IsCrashReportingInProgress()) {
			finish();

			return;
		}

		// Before the task, not after.
		//
		// This flag is what tells a crash report which queued call was
		// executing at the moment of the fault -- the crash handler
		// writes it into async-context.json. Setting it afterwards meant
		// the one task that actually was running reported running=false,
		// which is precisely backwards, and on the macro used at most of
		// the call sites in this codebase. QtDelayTask below has always
		// had it the right way round.
		//
		// Below the gate above, so a task that was dropped is not
		// reported as having been running.
		item->running = true;

		try {
			task();
		} catch (...) {
			finish();

			throw;
		}

		finish();
	};

	QMetaObject::invokeMethod(qApp, executor, Qt::QueuedConnection);

	return promise->get_future();
}

static std::atomic<bool> s_shuttingDown(false);

bool IsShuttingDown()
{
	return s_shuttingDown.load();
}

void SetShuttingDown()
{
	s_shuttingDown.store(true);
}

std::future<void> __QtExecSync_Impl(std::function<void()> task,
				    std::string file, int line)
{
	if (QThread::currentThread() == qApp->thread()) {
		// Same gate as the queued path (CORE-862), so the contract is
		// the same whichever thread asks: once crash reporting has
		// begun, a marshalled task does not run. This branch bypasses
		// the queue entirely, so it needs its own check -- and the
		// crashing thread is usually this one.
		if (!IsCrashReportingInProgress())
			task();

		std::promise<void> promise;
		promise.set_value();
		return promise.get_future();
	} else {
		// Same contract as the crash gate above, for the same reason:
		// once the Qt thread has left its event loop the task will never
		// run, so waiting for it is waiting forever.
		//
		// This is not hypothetical. The updater's shutdown blocks the Qt
		// thread until an in-flight update check finishes, and that check
		// finishes by calling QtExecSync -- each waiting on the other, a
		// deadlock caught in a live dump with both halves visible
		// (CORE-1131).
		if (IsShuttingDown()) {
			std::promise<void> promise;
			promise.set_value();
			return promise.get_future();
		}

		//
		// A slot shared with the posted task, so that abandoning the
		// wait below is safe.
		//
		// QtExecSync callers legitimately capture by reference -- the
		// call is synchronous by contract -- so a task left queued after
		// this frame unwinds would run against dangling references.
		// Claiming the slot under its mutex leaves only two cases: the
		// task has not started, and now never will, or it has already
		// finished. There is no third.
		//
		struct Slot {
			std::mutex mutex;
			bool abandoned = false;
		};

		auto slot = std::make_shared<Slot>();

		std::future<void> result = __QtPostTask_Impl(
			[slot, task]() {
				std::lock_guard<std::mutex> lock(slot->mutex);

				if (!slot->abandoned)
					task();
			},
			file, line);

		//
		// Wait in slices rather than in one shot.
		//
		// The gate above only covers a caller that arrives after
		// shutdown has begun. One already parked here when it begins
		// would go on waiting for a Qt thread that has left its event
		// loop -- and that is the common case, because the deadlock
		// exists precisely when the call was already in flight. The
		// entry check alone moved CORE-1193 five seconds later instead
		// of removing it: the updater's wait timed out, shutdown
		// advanced to ~NetworkDialog, and that joined the same worker
		// still parked right here.
		//
		while (result.wait_for(std::chrono::milliseconds(50)) !=
		       std::future_status::ready) {
			if (!IsShuttingDown())
				continue;

			std::lock_guard<std::mutex> lock(slot->mutex);

			slot->abandoned = true;

			std::promise<void> promise;
			promise.set_value();
			return promise.get_future();
		}

		return result;
	}
}

/* ========================================================= */

void SEDrainEventQueue()
{
	// Not while Initialize() is on the stack -- pumping there delivers the
	// update thread's queued close() and nests OBS's EXIT dispatch inside
	// its FINISHED_LOADING dispatch -- and not once a close has been caught,
	// when every further dispatch feeds a world being torn down (CORE-786).
	if (!SEIsEventPumpAllowed())
		return;

	QApplication::sendPostedEvents();
}

void SEDeleteDockWidgetWhenSafe(QPointer<QDockWidget> dock, const char *id,
				bool useDeleteLater)
{
	if (!dock)
		return;

	if (!SEIsUiTeardownSafe()) {
		blog(LOG_WARNING,
		     "[obs-streamelements-core]: leaking dock widget '%s': OBS was asked to close before initialization completed, deleting it now would corrupt the widget tree",
		     id ? id : "(unnamed)");

		// Detach from the main window so OBS's own teardown does not
		// walk into it later.
		dock->setParent(nullptr);

		return;
	}

	//
	// Out of the paint tree before it is destroyed.
	//
	// QMainWindow::removeDockWidget() hides a dock but leaves it parented,
	// so Qt can still walk into it from a repaint while something below is
	// destroying it -- and destroying our browser widget spins a nested
	// event loop inside obs-browser's closeBrowser(), which is exactly when
	// such a repaint runs. A paint that reaches a half-destroyed widget
	// calls a virtual that is pure in the base it has been demoted to, and
	// the process aborts in _purecall (CORE-1922, SELIVE-8G; CORE-777 is
	// the same mechanism from the event-queue side).
	//
	// Cheap and unconditional: a dock on its way out has nothing to paint.
	//
	dock->hide();
	dock->setParent(nullptr);

	if (useDeleteLater) {
		// The destructor runs later, from the event loop, so the scope
		// marker belongs to whatever destroys it then -- our widget's
		// own destructor carries one.
		dock->deleteLater();
	} else {
		SEWidgetTeardownScope marker("dock", id);

		delete dock.data();
	}
}

/* ========================================================= */

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area)
{
	switch (area) {
	case Qt::LeftDockWidgetArea:
		return "left";
	case Qt::RightDockWidgetArea:
		return "right";
	case Qt::TopDockWidgetArea:
		return "top";
	case Qt::BottomDockWidgetArea:
		return "bottom";
	case Qt::NoDockWidgetArea:
	default:
		return "floating";
	}
}

std::string GetCommandLineOptionValue(const std::string key)
{
	QStringList args = QCoreApplication::instance()->arguments();

	std::string search = "--" + key + "=";

	for (int i = 0; i < args.size(); ++i) {
		std::string arg = args.at(i).toStdString();

		if (arg.substr(0, search.size()) == search) {
			return arg.substr(search.size());
		}
	}

	return std::string();
}

std::string LoadResourceString(std::string path)
{
	std::string result = "";

	QFile file(QString(path.c_str()));

	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);

		result = stream.readAll().toStdString();
	}

	return result;
}

/* ========================================================= */

#ifdef WIN32
static uint64_t FromFileTime(const FILETIME &ft)
{
	ULARGE_INTEGER uli = {0};
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart;
}
#endif

void SerializeSystemTimes(CefRefPtr<CefValue> &output)
{
	SYNC_ACCESS();

	output->SetNull();

#ifdef WIN32
	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;

	if (::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		static bool hasSavedStartingValues = false;
		static uint64_t savedIdleTime;
		static uint64_t savedKernelTime;
		static uint64_t savedUserTime;

		if (!hasSavedStartingValues) {
			savedIdleTime = FromFileTime(idleTime);
			savedKernelTime = FromFileTime(kernelTime);
			savedUserTime = FromFileTime(userTime);

			hasSavedStartingValues = true;
		}

		const uint64_t SECOND_PART = 10000000L;
		const uint64_t MS_PART = SECOND_PART / 1000L;

		uint64_t idleInt = FromFileTime(idleTime) - savedIdleTime;
		uint64_t kernelInt = FromFileTime(kernelTime) - savedKernelTime;
		uint64_t userInt = FromFileTime(userTime) - savedUserTime;

		uint64_t idleMs = idleInt / MS_PART;
		uint64_t kernelMs = kernelInt / MS_PART;
		uint64_t userMs = userInt / MS_PART;

		uint64_t idleSec = idleMs / (uint64_t)1000;
		uint64_t kernelSec = kernelMs / (uint64_t)1000;
		uint64_t userSec = userMs / (uint64_t)1000;

		uint64_t idleMod = idleMs % (uint64_t)1000;
		uint64_t kernelMod = kernelMs % (uint64_t)1000;
		uint64_t userMod = userMs % (uint64_t)1000;

		double idleRat = idleSec + ((double)idleMod / 1000.0);
		double kernelRat = kernelSec + ((double)kernelMod / 1000.0);
		double userRat = userSec + ((double)userMod / 1000.0);

		// https://msdn.microsoft.com/en-us/84f674e7-536b-4ae0-b523-6a17cb0a1c17
		// lpKernelTime [out, optional]
		// A pointer to a FILETIME structure that receives the amount of time that
		// the system has spent executing in Kernel mode (including all threads in
		// all processes, on all processors)
		//
		// >>> This time value also includes the amount of time the system has been idle.
		//

		d->SetDouble("idleSeconds", idleRat);
		d->SetDouble("kernelSeconds", kernelRat - idleRat);
		d->SetDouble("userSeconds", userRat);
		d->SetDouble("totalSeconds", kernelRat + userRat);
		d->SetDouble("busySeconds", kernelRat + userRat - idleRat);
	}
#else
	mach_port_t mach_port = mach_host_self();
	host_cpu_load_info_data_t cpu_load_info;

	mach_msg_type_number_t cpu_load_info_count = HOST_CPU_LOAD_INFO_COUNT;
	if (host_statistics((host_t)mach_port, HOST_CPU_LOAD_INFO,
			    (host_info_t)&cpu_load_info,
			    &cpu_load_info_count) == KERN_SUCCESS) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		d->SetDouble("idleSeconds",
			     (double)(cpu_load_info.cpu_ticks[CPU_STATE_IDLE]) /
				     (double)CLOCKS_PER_SEC);
		d->SetDouble(
			"kernelSeconds",
			(double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM]) /
				(double)CLOCKS_PER_SEC);
		d->SetDouble("userSeconds",
			     (double)(cpu_load_info.cpu_ticks[CPU_STATE_USER] +
				      cpu_load_info.cpu_ticks[CPU_STATE_NICE]) /
				     (double)CLOCKS_PER_SEC);
		d->SetDouble(
			"totalSeconds",
			(double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] +
				 cpu_load_info.cpu_ticks[CPU_STATE_USER] +
				 cpu_load_info.cpu_ticks[CPU_STATE_IDLE] +
				 cpu_load_info.cpu_ticks[CPU_STATE_NICE]) /
				(double)CLOCKS_PER_SEC);
		d->SetDouble(
			"busySeconds",
			(double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] +
				 cpu_load_info.cpu_ticks[CPU_STATE_USER] +
				 cpu_load_info.cpu_ticks[CPU_STATE_NICE]) /
				(double)CLOCKS_PER_SEC);
	}
#endif
}

void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output)
{
	output->SetNull();

	const uint64_t DIV = 1048576;

#ifdef _WIN32
	MEMORYSTATUSEX mem;

	mem.dwLength = sizeof(mem);

	if (GlobalMemoryStatusEx(&mem)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		d->SetString("units", "MB");
		d->SetInt("memoryUsedPercentage", mem.dwMemoryLoad);
		d->SetInt("totalPhysicalMemory", mem.ullTotalPhys / DIV);
		d->SetInt("freePhysicalMemory", mem.ullAvailPhys / DIV);
		d->SetInt("totalVirtualMemory", mem.ullTotalVirtual / DIV);
		d->SetInt("freeVirtualMemory", mem.ullAvailVirtual / DIV);
		d->SetInt("freeExtendedVirtualMemory",
			  mem.ullAvailExtendedVirtual / DIV);
		d->SetInt("totalPageFileSize", mem.ullTotalPageFile / DIV);
		d->SetInt("freePageFileSize", mem.ullAvailPageFile / DIV);
	}
#else
	mach_port_t mach_port = mach_host_self();
	vm_statistics_data_t vm_stats;

	mach_msg_type_number_t vm_info_count = HOST_VM_INFO_COUNT;
	vm_size_t page_size;
	if (host_statistics((host_t)mach_port, HOST_VM_INFO,
			    (host_info_t)&vm_stats,
			    &vm_info_count) == KERN_SUCCESS &&
	    host_page_size(mach_port, &page_size) == KERN_SUCCESS) {
		int64_t free_memory =
			(int64_t)vm_stats.free_count * (int64_t)page_size;
		int64_t used_memory = ((int64_t)vm_stats.active_count +
				       (int64_t)vm_stats.inactive_count +
				       (int64_t)vm_stats.wire_count) *
				      (int64_t)page_size;
		int64_t total_memory = free_memory + used_memory;

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		d->SetString("units", "MB");
		d->SetInt("memoryUsedPercentage",
			  used_memory * 100L / total_memory);
		d->SetInt("totalVirtualMemory", total_memory / DIV);
		d->SetInt("freeVirtualMemory", free_memory / DIV);
		d->SetInt("freeExtendedVirtualMemory", 0);
		d->SetInt("totalPageFileSize", 0);
		d->SetInt("freePageFileSize", 0);

		{
			int mib[2] = {CTL_HW, HW_MEMSIZE};
			int64_t physical_memory;
			size_t length = sizeof(physical_memory);
			sysctl(mib, 2, &physical_memory, &length, NULL, 0);

			d->SetInt("totalPhysicalMemory", physical_memory / DIV);
			d->SetInt("freePhysicalMemory",
				  free_memory / DIV); // inaccurate
		}
	}
#endif
}

static CefString getRegStr(HKEY parent, const WCHAR *subkey, const WCHAR *key)
{
	CefString result;

#ifdef WIN32
	DWORD dataSize = 0;

	if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key, RRF_RT_ANY,
					    NULL, NULL, &dataSize)) {
		WCHAR *buffer = new WCHAR[dataSize];

		if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key,
						    RRF_RT_ANY, NULL, buffer,
						    &dataSize)) {
			result = buffer;
		}

		delete[] buffer;
	}
#endif

	return result;
};

static DWORD getRegDWORD(HKEY parent, const WCHAR *subkey, const WCHAR *key)
{
	DWORD result = 0;

#ifdef WIN32
	DWORD dataSize = sizeof(DWORD);

	::RegGetValueW(parent, subkey, key, RRF_RT_DWORD, NULL, &result,
		       &dataSize);
#endif

	return result;
}

#ifdef _WIN32
void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output)
{
	output->SetNull();

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
	output->SetDictionary(d);

	d->SetString("platform", "windows");

	SYSTEM_INFO info;

	::GetNativeSystemInfo(&info);

	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		d->SetString("cpuArch", "x86");
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		d->SetString("cpuArch", "IA64");
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
		d->SetString("cpuArch", "x64");
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		d->SetString("cpuArch", "ARM");
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		d->SetString("cpuArch", "ARM64");
		break;

	default:
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
		d->SetString("cpuArch", "Unknown");
		break;
	}

	d->SetInt("cpuCount", info.dwNumberOfProcessors);
	d->SetInt("cpuLevel", info.wProcessorLevel);

	{
		CefRefPtr<CefListValue> cpuList = CefListValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS ==
		    ::RegOpenKeyA(
			    HKEY_LOCAL_MACHINE,
			    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor",
			    &hRoot)) {
			WCHAR cpuKeyBuffer[2048];

			for (DWORD index = 0;
			     ERROR_SUCCESS ==
			     ::RegEnumKeyW(hRoot, index, cpuKeyBuffer,
					   sizeof(cpuKeyBuffer));
			     ++index) {
				CefRefPtr<CefDictionaryValue> p =
					CefDictionaryValue::Create();

				p->SetString("name",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"ProcessorNameString"));
				p->SetString("vendor",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"VendorIdentifier"));
				p->SetInt("speedMHz",
					  getRegDWORD(hRoot, cpuKeyBuffer,
						      L"~MHz"));
				p->SetString("identifier",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"Identifier"));

				cpuList->SetDictionary(cpuList->GetSize(), p);
			}

			::RegCloseKey(hRoot);
		}

		d->SetList("cpuHardware", cpuList);
	}

	{
		CefRefPtr<CefDictionaryValue> bios =
			CefDictionaryValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS ==
		    ::RegOpenKeyW(HKEY_LOCAL_MACHINE,
				  L"HARDWARE\\DESCRIPTION\\System", &hRoot)) {
			HKEY hBios;
			if (ERROR_SUCCESS ==
			    ::RegOpenKeyW(hRoot, L"BIOS", &hBios)) {
				WCHAR subKeyBuffer[2048];
				DWORD bufSize = sizeof(subKeyBuffer) /
						sizeof(subKeyBuffer[0]);

				DWORD valueIndex = 0;
				DWORD valueType = 0;

				LSTATUS callStatus = ::RegEnumValueW(
					hBios, valueIndex, subKeyBuffer,
					&bufSize, NULL, &valueType, NULL, NULL);
				while (ERROR_NO_MORE_ITEMS != callStatus) {
					switch (valueType) {
					case REG_DWORD_BIG_ENDIAN:
					case REG_DWORD_LITTLE_ENDIAN:
						bios->SetInt(
							subKeyBuffer,
							getRegDWORD(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;

					case REG_QWORD:
						bios->SetInt(
							subKeyBuffer,
							getRegDWORD(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;

					case REG_SZ:
					case REG_EXPAND_SZ:
					case REG_MULTI_SZ:
						bios->SetString(
							subKeyBuffer,
							getRegStr(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;
					}

					++valueIndex;

					bufSize = sizeof(subKeyBuffer) /
						  sizeof(subKeyBuffer[0]);
					callStatus = ::RegEnumValueW(
						hBios, valueIndex, subKeyBuffer,
						&bufSize, NULL, &valueType,
						NULL, NULL);
				}

				::RegCloseKey(hBios);
			}

			::RegCloseKey(hRoot);
		}

		d->SetDictionary("bios", bios);
	}

	d->SetString("os", "Windows");
}
#endif

/* ========================================================= */







std::string SerializeAppStyleSheet()
{
	std::string result = qApp->styleSheet().toStdString();

	if (result.compare(0, 8, "file:///") == 0) {
		QUrl url(result.c_str());

		if (url.isLocalFile()) {
			result = result.substr(8);

			QFile file(result.c_str());

			if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QTextStream in(&file);

				result = in.readAll().toStdString();
			}
		}
	}

	return result;
}

std::string GetAppStyleSheetSelectorContent(std::string selector)
{
	std::string result;

	std::string css = SerializeAppStyleSheet();

	std::replace(css.begin(), css.end(), '\n', ' ');

	css = std::regex_replace(css, std::regex("/\\*.*?\\*/"), "");

	std::regex selector_regex("[^\\s]*" + selector + "[\\s]*\\{(.*?)\\}");
	std::smatch selector_match;

	if (std::regex_search(css, selector_match, selector_regex)) {
		result = std::string(selector_match[1].first,
				     selector_match[1].second);
	}

	return result;
}

std::string GetCurrentThemeName()
{
	std::string result = "Default";

	config_t *globalConfig =
		obs_fe_user_config(); // does not increase refcount

	if (globalConfig) {
		const char *themeName = config_get_string(
			globalConfig, "General", "CurrentTheme");
		if (!themeName) {
			/* Use deprecated "Theme" value if available */
			themeName = config_get_string(globalConfig, "General",
						      "Theme");
			if (!themeName) {
				themeName = "Default";
			}

			result = themeName;
		}
	}

	std::string appStyle = qApp->styleSheet().toStdString();

	if (appStyle.substr(0, 7) == "file://") {
		QUrl url(appStyle.c_str());

		if (url.isLocalFile()) {
			result = url.fileName().split('.')[0].toStdString();
		}
	}

	return result;
}

std::string GetCefVersionString()
{
	char buf[64] = "unknown";

#ifdef _WIN32
	void *libcef = os_dlopen("libcef");
	if (libcef) {
		typedef int (*cef_version_info_func_ptr_t)(int entry);

		cef_version_info_func_ptr_t cef_version_info_func_ptr =
			(cef_version_info_func_ptr_t)os_dlsym(
				libcef, "cef_version_info");

		if (cef_version_info_func_ptr) {
			sprintf(buf, "cef.%d.%d.chrome.%d.%d.%d.%d",
				cef_version_info_func_ptr(0),
				cef_version_info_func_ptr(1),
				cef_version_info_func_ptr(2),
				cef_version_info_func_ptr(3),
				cef_version_info_func_ptr(4),
				cef_version_info_func_ptr(5));
		}

		os_dlclose(libcef);
	}
#endif

	return std::string(buf);
}

std::string GetCefPlatformApiHash()
{
	char buf[128] = "unknown";

#ifdef _WIN32
	void *libcef = os_dlopen("libcef");
	if (libcef) {
		typedef const char *(*cef_api_hash_func_ptr_t)(int entry);

		cef_api_hash_func_ptr_t cef_api_hash_func_ptr =
			(cef_api_hash_func_ptr_t)os_dlsym(libcef,
							  "cef_api_hash");

		if (cef_api_hash_func_ptr) {
			sprintf(buf, "%s", cef_api_hash_func_ptr(0));

			return std::string(buf);
		}

		os_dlclose(libcef);
	}
#endif

	return std::string(buf);
}

std::string GetCefUniversalApiHash()
{
	char buf[128] = "unknown";

#ifdef _WIN32
	void *libcef = os_dlopen("libcef");
	if (libcef) {
		typedef const char *(*cef_api_hash_func_ptr_t)(int entry);

		cef_api_hash_func_ptr_t cef_api_hash_func_ptr =
			(cef_api_hash_func_ptr_t)os_dlsym(libcef,
							  "cef_api_hash");

		if (cef_api_hash_func_ptr) {
			sprintf(buf, "%s", cef_api_hash_func_ptr(1));

			return std::string(buf);
		}

		os_dlclose(libcef);
	}
#endif

	return std::string(buf);
}

std::string GetSEPluginVersionString()
{
	char version_buf[64];
	sprintf(version_buf, "%d.%d.%d.%d",
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 1000000000000L) /
		      10000000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 10000000000L) /
		      100000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 100000000L) / 1000000L),
		(int)(STREAMELEMENTS_PLUGIN_VERSION % 1000000L));

	return version_buf;
}

std::string GetSEApiVersionString()
{
	char version_buf[64];

	sprintf(version_buf, "%d.%d", HOST_API_VERSION_MAJOR,
		HOST_API_VERSION_MINOR);

	return version_buf;
}

/* ========================================================= */

#ifdef WIN32
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")
void SetGlobalCURLOptions(CURL *curl, const char *url)
{
	// TODO: TBD: MacOS: http://mirror.informatimago.com/next/developer.apple.com/qa/qa2001/qa1234.html
	std::string proxy =
		GetCommandLineOptionValue("streamelements-http-proxy");

	if (!proxy.size()) {
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config;

		if (WinHttpGetIEProxyConfigForCurrentUser(&config)) {
			// http=127.0.0.1:8888;https=127.0.0.1:8888
			if (config.lpszProxy) {
				proxy = wstring_to_utf8(config.lpszProxy);

				std::map<std::string, std::string> schemes;
				for (auto kvstr : tokenizeString(proxy, ";")) {
					std::vector<std::string> kv =
						tokenizeString(kvstr, "=");

					if (kv.size() == 2) {
						std::transform(kv[0].begin(),
							       kv[0].end(),
							       kv[0].begin(),
							       tolower);
						schemes[kv[0]] = kv[1];
					}
				}

				std::string scheme =
					tokenizeString(url, ":")[0];
				std::transform(scheme.begin(), scheme.end(),
					       scheme.begin(), tolower);

				if (schemes.count(scheme)) {
					proxy = schemes[scheme];
				} else if (schemes.count("http")) {
					proxy = schemes["http"];
				} else {
					proxy = "";
				}
			}

			if (config.lpszProxy) {
				GlobalFree((HGLOBAL)config.lpszProxy);
			}

			if (config.lpszProxyBypass) {
				GlobalFree((HGLOBAL)config.lpszProxyBypass);
			}

			if (config.lpszAutoConfigUrl) {
				GlobalFree((HGLOBAL)config.lpszAutoConfigUrl);
			}
		}
	}

	if (proxy.size()) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
	}
}
#endif












/* ========================================================= */




#ifdef WIN32
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#endif





/* ========================================================= */

#ifndef WIN32
#include <uuid/uuid.h>
#endif

std::string CreateGloballyUniqueIdString()
{
	std::string result;
#ifdef WIN32
	const int GUID_STRING_LENGTH = 39;

	GUID guid;
	CoCreateGuid(&guid);

	OLECHAR guidStr[GUID_STRING_LENGTH];
	StringFromGUID2(guid, guidStr, GUID_STRING_LENGTH);

	guidStr[GUID_STRING_LENGTH - 2] = 0;
	result = wstring_to_utf8(guidStr + 1);
#else
	uuid_t uuid;

	uuid_generate_time(uuid);

	char buf[128];
	uuid_unparse(uuid, buf);

	result = buf;
#endif

	return result;
}

#ifdef WIN32
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
std::string CreateCryptoSecureRandomNumberString()
{
	std::string result = "0";

	BCRYPT_ALG_HANDLE hAlgo;

	if (0 == BCryptOpenAlgorithmProvider(&hAlgo, BCRYPT_RNG_ALGORITHM, NULL,
					     0)) {
		uint64_t buffer;

		if (0 == BCryptGenRandom(hAlgo, (PUCHAR)&buffer, sizeof(buffer),
					 0)) {
			char buf[sizeof(buffer) * 2 + 1];
			sprintf_s(buf, sizeof(buf), "%llX", buffer);

			result = buf;

			std::cout << buffer << std::endl;
		}

		BCryptCloseAlgorithmProvider(hAlgo, 0);
	}

	return result;
}
#endif

#ifdef WIN32
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Advapi32.lib")
// MacOS version implemented in SEUtils.mm
std::string GetComputerSystemUniqueId()
{
	const char *REG_VALUE_NAME = "MachineUniqueIdentifier";

	std::string result =
		ReadEnvironmentConfigString(REG_VALUE_NAME, nullptr);
	std::string prevResult = result;

	if (result.size()) {
		// Discard invalid values
		if (result ==
			    "WUID/03000200-0400-0500-0006-000700080009" || // Known duplicate
		    result ==
			    "WUID/00000000-0000-0000-0000-000000000000" || // Null value
		    result ==
			    "WUID/FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF" || // Invalid value
		    result ==
			    "WUID/00412F4E-0000-0000-0000-0000FFFFFFFF") { // Set by russian MS Office crack
			result = "";
		}
	}

	// ...and discard anything that is not even shaped like an id.
	//
	// Every id this function produces is "<4-letter tag>/<payload>" --
	// WUID, SWID or SEID here, ALSU on macOS. The list above only rejects
	// four exact strings, so a value that is merely malformed used to be
	// accepted and, because it is written straight back to the registry,
	// became that machine's permanent identity.
	//
	// Not hypothetical: a machine was found reporting a user id of literally
	// "S" -- one character, REG_SZ, the first letter of an SWID/SEID value.
	// Every crash from it was anonymous and none could be correlated with
	// any other. Rejecting the value here regenerates a good one on the next
	// call rather than requiring the registry to be cleaned by hand.
	if (result.size() && (result.size() < 16 || result.find('/') != 4)) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: discarding malformed machine unique id '%s'; a new one will be generated",
		     result.c_str());

		result = "";
	}

	if (!result.size()) {
		// Get unique ID from WMI

		HRESULT hr = CoInitialize(NULL);

		// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/initializing-com-for-a-wmi-application
		if (SUCCEEDED(hr)) {
			bool uinitializeCom = hr == S_OK;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/setting-the-default-process-security-level-using-c-
			CoInitializeSecurity(
				NULL, // security descriptor
				-1,   // use this simple setting
				NULL, // use this simple setting
				NULL, // reserved
				RPC_C_AUTHN_LEVEL_DEFAULT, // authentication level
				RPC_C_IMP_LEVEL_IMPERSONATE, // impersonation level
				NULL,      // use this simple setting
				EOAC_NONE, // no special capabilities
				NULL);     // reserved

			IWbemLocator *pLocator;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
			hr = CoCreateInstance(CLSID_WbemLocator, 0,
					      CLSCTX_INPROC_SERVER,
					      IID_IWbemLocator,
					      (LPVOID *)&pLocator);

			if (SUCCEEDED(hr)) {
				IWbemServices *pSvc = 0;

				// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
				hr = pLocator->ConnectServer(
					BSTR(L"root\\cimv2"), //namespace
					NULL,                 // User name
					NULL,                 // User password
					0,                    // Locale
					NULL,                 // Security flags
					0,                    // Authority
					0,                    // Context object
					&pSvc); // IWbemServices proxy

				if (SUCCEEDED(hr)) {
					hr = CoSetProxyBlanket(
						pSvc, RPC_C_AUTHN_WINNT,
						RPC_C_AUTHZ_NONE, NULL,
						RPC_C_AUTHN_LEVEL_CALL,
						RPC_C_IMP_LEVEL_IMPERSONATE,
						NULL, EOAC_NONE);

					if (SUCCEEDED(hr)) {
						IEnumWbemClassObject
							*pEnumerator = NULL;

						hr = pSvc->ExecQuery(
							(BSTR)L"WQL",
							(BSTR)L"select * from Win32_ComputerSystemProduct",
							WBEM_FLAG_FORWARD_ONLY,
							NULL, &pEnumerator);

						if (SUCCEEDED(hr)) {
							IWbemClassObject *pObj =
								NULL;

							ULONG resultCount;
							hr = pEnumerator->Next(
								WBEM_INFINITE,
								1, &pObj,
								&resultCount);

							if (SUCCEEDED(hr)) {
								VARIANT value;

								hr = pObj->Get(
									L"UUID",
									0,
									&value,
									NULL,
									NULL);

								if (SUCCEEDED(
									    hr)) {
									if (value.vt !=
									    VT_NULL) {
										result =
											std::string(
												"SWID/") +
											clean_guid_string(wstring_to_utf8(
												std::wstring(
													value.bstrVal)));
										result +=
											"-";
										result += clean_guid_string(
											CreateGloballyUniqueIdString());
										result +=
											"-";
										result +=
											CreateCryptoSecureRandomNumberString();
									}
									VariantClear(
										&value);
								}
							}

							pEnumerator->Release();
						}
					}

					pSvc->Release();
				}

				pLocator->Release();
			}

			if (uinitializeCom) {
				CoUninitialize();
			}
		}
	}

	if (!result.size()) {
		// Failed retrieving UUID, generate our own
		result = std::string("SEID/") +
			 clean_guid_string(CreateGloballyUniqueIdString());
		result += "-";
		result += CreateCryptoSecureRandomNumberString();
	}

	if (result.size() && result != prevResult) {
		// Save for future use
		WriteEnvironmentConfigString(REG_VALUE_NAME, result.c_str(),
					     nullptr);
	}

	return result;
}
#endif


std::string CreateSessionSignedAbsolutePathURL(std::wstring path)
{
	// static, because constructing a std::wregex compiles the pattern and
	// this is called once per asset. getAllRazerWyvrnEvents signs ~23,700
	// URLs in one call; at two fresh regexes each that was ~47,000 pattern
	// compilations and the dominant cost of the whole request.
	static const std::wregex kHash(L"#");
	static const std::wregex kAmp(L"&");

	path = std::regex_replace(path, kHash, L"%23");
	path = std::regex_replace(path, kAmp, L"%26");

	QUrl parts(SEGlobalStateManager::GetInstance()
			   ->GetLocalFilesystemHttpServer()
			   ->GetBaseUrl()
			   .c_str());

	parts.setPath(
		QString::fromStdWString((std::wstring(L"/") + path).c_str()));

	std::string message = parts.path().toStdString();

	message = message.erase(0, 1);

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return parts.url().toStdString() + std::string("?digest=") +
	       CreateSessionMessageSignature(message);
}

bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path)
{
	QUrl parts(url.c_str());

	path = parts.path().toStdString();

	path = path.erase(0, 1);

	if (!parts.hasQuery())
		return false;

	QUrlQuery query(parts.query());

	if (!query.hasQueryItem("digest"))
		return false;

	std::string signature = query.queryItemValue("digest").toStdString();

	std::string message = path;

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return VerifySessionMessageSignature(message, signature);
}

/* ========================================================= */

bool IsAlwaysOnTop(QWidget *window)
{
	if (!window)
		return false;

#ifdef WIN32
	DWORD exStyle = GetWindowLong((HWND)window->winId(), GWL_EXSTYLE);
	return (exStyle & WS_EX_TOPMOST) != 0;
#else
	return (window->windowFlags() & Qt::WindowStaysOnTopHint) != 0;
#endif
}

void SetAlwaysOnTop(QWidget *window, bool enable)
{
	if (!window)
		return;

#ifdef WIN32
	HWND hwnd = (HWND)window->winId();
	SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
		     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
	Qt::WindowFlags flags = window->windowFlags();

	if (enable)
		flags |= Qt::WindowStaysOnTopHint;
	else
		flags &= ~Qt::WindowStaysOnTopHint;

	window->setWindowFlags(flags);
	window->show();
#endif
}

double GetObsGlobalFramesPerSecond()
{
	config_t *basicConfig =
		obs_frontend_get_profile_config(); // does not increase refcount

	switch (config_get_uint(basicConfig, "Video", "FPSType")) {
	case 0: // Common
		return (double)atoi(
			config_get_string(basicConfig, "Video", "FPSCommon"));
		break;

	case 1: // Integer
		return (double)config_get_uint(basicConfig, "Video", "FPSInt");
		break;

	case 2: // Fractional
		return (double)config_get_uint(basicConfig, "Video", "FPSNum") /
		       (double)config_get_uint(basicConfig, "Video", "FPSDen");
		break;
	}

	return 0;
}

void AdviseHostUserInterfaceStateChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {
		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			os_atomic_set_bool(&isBusy, false);

			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostUserInterfaceStateChanged",
					      "null");
		});
	}
}

void AdviseHostHotkeyBindingsChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {

		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostHotkeyBindingsChanged",
					      "null");

			os_atomic_set_bool(&isBusy, false);
		});
	}
}

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId)
{
	std::regex url_regex(
		"^https://streamelements.com/overlay/([^/]+)/([^/]+)$");
	std::smatch match;

	if (std::regex_match(url, match, url_regex)) {
		overlayId = match[1].str();
		accountId = match[2].str();

		return true;
	}

	return false;
}

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId)
{
	return std::string("https://streamelements.com/overlay/") + overlayId +
	       std::string("/editor");
}







class QRemoteIconMenu : public QMenu {
public:
	QRemoteIconMenu(const char *iconUrl, QPixmap *defaultPixmap = nullptr)
		: QMenu(),
		  loader(SERemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		QObject::connect(this, &QObject::destroyed,
				 [this]() { loader->Cancel(); });
	}

private:
	CefRefPtr<SERemoteIconLoader> loader;
};

class QRemoteIconAction : public QAction {
public:
	QRemoteIconAction(const char *iconUrl, QPixmap *defaultPixmap = nullptr)
		: QAction(),
		  loader(SERemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		QObject::connect(this, &QObject::destroyed,
				 [this]() { loader->Cancel(); });
	}

private:
	CefRefPtr<SERemoteIconLoader> loader;
};

class QRemoteIconPushButton : public QPushButton {
public:
	QRemoteIconPushButton(const char *iconUrl,
			      QPixmap *defaultPixmap = nullptr)
		: QPushButton(),
		  loader(SERemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		setMouseTracking(true);

		setStyleSheet("background: none; padding: 0;");

		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

		CefRefPtr<SERemoteIconLoader> loaderCopy = loader;

		QObject::connect(this, &QObject::destroyed,
				 [loaderCopy]() { loaderCopy->Cancel(); });
	}

	virtual QSize minimumSizeHint() const override { return sizeHint(); }
	virtual QSize sizeHint() const override
	{
		if (text().size()) {
			QSize textSize = fontMetrics().size(
				Qt::TextShowMnemonic, text());

			QStyleOptionButton opt;
			opt.initFrom(this);
			opt.rect.setSize(textSize);

			QSize size = style()->sizeFromContents(
				QStyle::CT_PushButton, &opt, textSize, this);

			return size;
		} else {
			return QSize(16, 16);
		}
	}

private:
	CefRefPtr<SERemoteIconLoader> loader;
};

bool DeserializeDocksMenu(QMenu &menu)
{
	auto widgetManager = SEGlobalStateManager::GetInstance()
				     ->GetWidgetManager();

	if (!widgetManager)
		return true;

	widgetManager->EnterCriticalSection();

	std::vector<std::string> widgetIds;

	widgetManager->GetDockBrowserWidgetIdentifiers(widgetIds);

	std::vector<SEBrowserWidgetManager::DockBrowserWidgetInfo *>
		widgets;

	if (widgetManager) {
		for (auto id : widgetIds) {
			auto info = widgetManager->GetDockBrowserWidgetInfo(
				id.c_str());

			if (info) {
				widgets.push_back(info);
			}
		}
	}

	std::sort(
		widgets.begin(), widgets.end(),
		[](SEBrowserWidgetManager::DockBrowserWidgetInfo *a,
		   SEBrowserWidgetManager::DockBrowserWidgetInfo
			   *b) { return a->m_title < b->m_title; });

	for (auto widget : widgets) {
		// widget->m_visible
		QAction *widget_action =
			new QAction(QString(widget->m_title.c_str()));
		menu.addAction(widget_action);

		std::string id = widget->m_id;
		bool isVisible = widget->m_visible;

		widget_action->setCheckable(true);
		widget_action->setChecked(isVisible);

		QObject::connect(widget_action, &QAction::triggered, [id, isVisible, widget_action] {
			auto widgetManager =
				SEGlobalStateManager::GetInstance()
					->GetWidgetManager();

			if (!widgetManager)
				return;

			widgetManager->EnterCriticalSection();

			QDockWidget *dock =
				widgetManager->GetDockWidget(id.c_str());

			if (dock) {
				if (isVisible) {
					// Hide
					SEGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackEvent(
							"se_live_dock_hide_click",
							json11::Json::object{
								{"type",
								 "button_click"},
								{"placement",
								 "menu"}},
							json11::Json::array{json11::Json::array{
								"dock_widget_title",
								dock->windowTitle()
									.toStdString()}});
				} else {
					// Show
					SEGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackEvent(
							"se_live_dock_show_click",
							json11::Json::object{
								{"type",
								 "button_click"},
								{"placement",
								 "menu"}},
							json11::Json::array{json11::Json::array{
								"dock_widget_title",
								dock->windowTitle()
									.toStdString()}});
				}

				dock->setVisible(!isVisible);

				SEGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
			}

			widgetManager->LeaveCriticalSection();
		});
	}

	for (auto widget : widgets) {
		delete widget;
	}

	widgetManager->LeaveCriticalSection();

	return true;
}

bool DeserializeMenu(CefRefPtr<CefValue> input, QMenu &menu,
		     std::function<void()> defaultAction,
		     std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_LIST)
		return false;

	auto getIconUrl =
		[](CefRefPtr<CefDictionaryValue> parent) -> std::string {
		if (!parent->HasKey("icon") ||
		    parent->GetType("icon") != VTYPE_DICTIONARY)
			return "";

		CefRefPtr<CefDictionaryValue> d = parent->GetDictionary("icon");

		if (!d->HasKey("url") || d->GetType("url") != VTYPE_STRING)
			return "";

		std::string url = d->GetString("url").ToString();

		return url;
	};

	CefRefPtr<CefListValue> list = input->GetList();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_DICTIONARY)
			return false;

		CefRefPtr<CefDictionaryValue> d = list->GetDictionary(index);

		if (!d->HasKey("type") || d->GetType("type") != VTYPE_STRING)
			return false;

		std::string type = d->GetString("type");

		if (type == "separator") {
			menu.addSeparator();
		} else if (type == "command") {
			bool enabled = true;

			if (d->HasKey("enabled") &&
			    d->GetType("enabled") == VTYPE_BOOL) {
				enabled = d->GetBool("enabled");
			}

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				return false;

			if (enabled) {
				if (!d->HasKey("invoke") ||
				    d->GetType("invoke") != VTYPE_STRING)
					return false;
			}

			std::string title = d->GetString("title");

			std::string iconUrl = getIconUrl(d);

			QAction *auxAction = new QRemoteIconAction(
				iconUrl.size() ? iconUrl.c_str() : nullptr);

			auxAction->setText(title.c_str());
			auxAction->setEnabled(enabled);

			menu.addAction(auxAction);

			CefRefPtr<CefValue> action = CefValue::Create();
			action->SetDictionary(d->Copy(false));

			auxAction->connect(
				auxAction, &QAction::triggered,
				[action, defaultAction, defaultContextMenu]() {
					DeserializeAndInvokeAction(
						action, defaultAction,
						defaultContextMenu);

					return true;
				});
		} else if (type == "container") {
			bool enabled = true;

			if (d->HasKey("enabled") &&
			    d->GetType("enabled") == VTYPE_BOOL) {
				enabled = d->GetBool("enabled");
			}

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				return false;

			std::string iconUrl = getIconUrl(d);

			if (d->HasKey("items") &&
			    d->GetType("items") == VTYPE_LIST) {
				QMenu *submenu = new QRemoteIconMenu(
					iconUrl.size() ? iconUrl.c_str()
						       : nullptr);

				submenu->setTitle(d->GetString("title")
							  .ToString()
							  .c_str());

				submenu->setEnabled(enabled);

				menu.addMenu(submenu);

				if (!DeserializeMenu(d->GetValue("items"),
						     *submenu))
					return false;
			} else if (d->HasKey("itemsSource") &&
				   d->GetType("itemsSource") == VTYPE_STRING) {
				std::string itemsSource =
					d->GetString("itemsSource");

				if (itemsSource == ":dockingWidgets") {
					QMenu *submenu = new QRemoteIconMenu(
						iconUrl.size() ? iconUrl.c_str()
							       : nullptr);

					submenu->setTitle(d->GetString("title")
								  .ToString()
								  .c_str());

					submenu->setEnabled(enabled);

					menu.addMenu(submenu);

					if (!DeserializeDocksMenu(*submenu))
						return false;
				} else {
					return false;
				}
			} else {
				return false;
			}
		} else {
			return false;
		}
	}

	return true;
}

QWidget *
DeserializeAuxiliaryControlWidget(CefRefPtr<CefValue> input,
				  std::function<void()> defaultAction,
				  std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("type") || d->GetType("type") != VTYPE_STRING)
		return nullptr;

	std::string type = d->GetString("type");

	std::string iconUrl = "";

	if (d->HasKey("icon") && d->GetType("icon") == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> m_icon = d->GetDictionary("icon");

		if (m_icon->HasKey("url") &&
		    m_icon->GetType("url") == VTYPE_STRING) {
			iconUrl = m_icon->GetString("url");
		}
	}

	std::string styleSheet =
		"QToolTip { color: palette(text); background-color: palette(dark); } ";

	if (d->HasKey("color") && d->GetType("color") == VTYPE_STRING) {
		styleSheet += "QPushButton { color: ";
		styleSheet += d->GetString("color").ToString() + ";";
		styleSheet += " } ";
	}

	styleSheet +=
		"QPushButton { background-color: palette(button); padding: 1; padding-left: 1em; padding-right: 1em; } ";
	styleSheet +=
		"QPushButton:hover { background-color: palette(midlight); color: palette(dark); } ";
	styleSheet +=
		"QPushButton:pressed { background-color: palette(shadow); color: palette(bright-text); } ";

	if (type == "container") {
		if (!d->HasKey("items") || d->GetType("items") != VTYPE_LIST)
			return nullptr;

		CefRefPtr<CefValue> items = d->GetValue("items")->Copy();

		QRemoteIconPushButton *control =
			new QRemoteIconPushButton(iconUrl.c_str());

		//control->setContentsMargins(0, 0, 0, 0);
		//control->setMinimumSize(16, 16);
		control->setSizePolicy(QSizePolicy::Minimum,
				       QSizePolicy::Minimum);

		auto HandleClick = [items, control](bool /*checked*/) {
			QMenu menu;

			if (!DeserializeMenu(items, menu))
				return;

			// menu.exec(control->mapToGlobal(QPoint(0, 0)));
			menu.exec(QCursor::pos());
		};

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			control->setText(
				d->GetString("title").ToString().c_str());
		}

		if (d->HasKey("tooltip") &&
		    d->GetType("tooltip") == VTYPE_STRING) {
			std::string tooltip =
				d->GetString("tooltip").ToString();

			control->setToolTip(tooltip.c_str());
		}

		control->setStyleSheet(styleSheet.c_str());

		QObject::connect(control, &QPushButton::clicked, HandleClick);

		return control;
	} else if (type == "command") {
		if (!d->HasKey("invoke") ||
		    d->GetType("invoke") != VTYPE_STRING)
			return nullptr;

		CefRefPtr<CefValue> action = input->Copy();

		auto HandleClick = [action, defaultAction,
				    defaultContextMenu](bool /*checked*/) {
			DeserializeAndInvokeAction(action, defaultAction,
						   defaultContextMenu);
		};

		QRemoteIconPushButton *control =
			new QRemoteIconPushButton(iconUrl.c_str());

		//control->setContentsMargins(0, 0, 0, 0);
		//control->setMinimumSize(16, 16);
		//control->setSizePolicy(QSizePolicy::Minimum,
		//		       QSizePolicy::Minimum);

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			control->setText(
				d->GetString("title").ToString().c_str());

			control->setWindowIconText(
				d->GetString("title").ToString().c_str());
		}

		if (d->HasKey("tooltip") &&
		    d->GetType("tooltip") == VTYPE_STRING) {
			std::string tooltip =
				d->GetString("tooltip").ToString();

			control->setToolTip(tooltip.c_str());
		}

		control->setStyleSheet(styleSheet.c_str());

		QObject::connect(control, &QPushButton::clicked, HandleClick);

		return control;
	} else if (type == "separator") {
		QWidget *control = new QWidget();
		control->setContentsMargins(0, 0, 0, 0);
		control->setStyleSheet("background: none");
		control->setFixedSize(2, 2);

		return control;
	}

	return nullptr;
}

QWidget *DeserializeRemoteIconWidget(CefRefPtr<CefValue> input,
				     QPixmap *defaultPixmap)
{
	std::string iconUrl = "";

	if (input.get() && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (d->HasKey("url") && d->GetType("url") == VTYPE_STRING) {
			iconUrl = d->GetString("url").ToString();
		}
	}

	return new QRemoteIconPushButton(iconUrl.c_str(), defaultPixmap);
}

bool DeserializeAndInvokeAction(CefRefPtr<CefValue> input,
				std::function<void()> defaultAction,
				std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("invoke") || d->GetType("invoke") != VTYPE_STRING)
		return false;

	std::string invoke = d->GetString("invoke");

	CefRefPtr<CefListValue> invokeArgs = CefListValue::Create();

	if (d->HasKey("invokeArgs") && d->GetType("invokeArgs") == VTYPE_LIST)
		invokeArgs = d->GetList("invokeArgs")->Copy();

	if (invoke == ":defaultAction") {
		defaultAction();

		return true;
	} else if (invoke == ":defaultContextMenu") {
		defaultContextMenu();

		return true;
	} else if (invoke == ":none") {
		// No action
		return true;
	} else {
		if (!SEGlobalStateManager::IsInstanceAvailable())
			return false;

		return SEApiMessageHandler::InvokeHandler::GetInstance(
			       SEGlobalStateManager::GetInstance()
				       ->GetApiServices())
			->InvokeApiCallAsync(invoke, invokeArgs,
					     [](CefRefPtr<CefValue>) {});
	}
}






class TimedObsApiTransactionHandle {
private:
	static std::map<std::string, TimedObsApiTransactionHandle *> s_map;
	static std::recursive_mutex s_mutex;

public:
	static std::string Create(int timeoutMilliseconds = 60000)
	{
		std::lock_guard<std::recursive_mutex> guard(s_mutex);

		std::string id = CreateGloballyUniqueIdString();

		s_map[id] = new TimedObsApiTransactionHandle(
			id, timeoutMilliseconds, [id]() { Destroy(id); });

		if (s_map.size() == 1) {
			SEPleaseWaitWindow::GetInstance()->Show();

			obs_frontend_defer_save_begin();
		}

		return id;
	}

	static void Destroy(std::string id)
	{
		std::lock_guard<std::recursive_mutex> guard(s_mutex);

		if (!s_map.count(id))
			return;

		delete s_map[id];

		s_map.erase(id);

		if (s_map.size() == 0) {
			obs_frontend_defer_save_end();

			SEPleaseWaitWindow::GetInstance()->Hide();
		}
	}

private:
	TimedObsApiTransactionHandle(std::string id, int timeoutMilliseconds,
				     std::function<void()> onTimer)
		: m_id(id), m_onTimer(onTimer)
	{
		m_timer = new QTimer();
		m_timer->moveToThread(qApp->thread());
		m_timer->setInterval(timeoutMilliseconds);
		m_timer->setSingleShot(true);
		QObject::connect(m_timer, &QTimer::timeout,
				 [this]() { m_onTimer(); });
		QMetaObject::invokeMethod(m_timer, "start",
					  Qt::QueuedConnection,
					  Q_ARG(int, timeoutMilliseconds));
	}

	~TimedObsApiTransactionHandle()
	{
		QMetaObject::invokeMethod(m_timer, "stop", Qt::QueuedConnection,
					  Q_ARG(int, 0));

		m_timer->deleteLater();
	}

private:
	std::string m_id;
	std::function<void()> m_onTimer;
	QTimer *m_timer;
};

std::map<std::string, TimedObsApiTransactionHandle *>
	TimedObsApiTransactionHandle::s_map;
std::recursive_mutex TimedObsApiTransactionHandle::s_mutex;

std::string CreateTimedObsApiTransaction(int timeoutMilliseconds)
{
	return TimedObsApiTransactionHandle::Create(timeoutMilliseconds);
}

void CompleteTimedObsApiTransaction(std::string id)
{
	TimedObsApiTransactionHandle::Destroy(id);
}

/* ========================================================= */

static bool GetBool(CefRefPtr<CefDictionaryValue> input, std::string key,
		    bool defaultValue = false)
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_BOOL)
		return defaultValue;

	return input->GetBool(key);
}

static int GetInt(CefRefPtr<CefDictionaryValue> input, std::string key,
		  int defaultValue = 0)
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_INT)
		return defaultValue;

	return input->GetInt(key);
}

static std::string GetString(CefRefPtr<CefDictionaryValue> input,
			     std::string key, std::string defaultValue = "")
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_STRING)
		return defaultValue;

	return input->GetString(key).ToString();
}

static std::wstring GetWString(CefRefPtr<CefDictionaryValue> input,
			       std::string key, std::wstring defaultValue = L"")
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_STRING)
		return defaultValue;

	return input->GetString(key).ToWString();
}

void RemoveObsDirtyShutdownMarker()
{
	char *safeModeMarkerPathPtr =
		os_get_config_path_ptr("obs-studio/safe_mode");
	if (safeModeMarkerPathPtr) {
		// We'll remove this flag so the next time OBS starts, it won't alert that it was terminated abnormally
		os_unlink(safeModeMarkerPathPtr);
	}
	bfree(safeModeMarkerPathPtr);
}

#ifdef WIN32
void RestartCurrentApplication()
{
	bool success = false;

	QProcess proc;
	if (proc.startDetached(
		    QCoreApplication::instance()->applicationFilePath(),
		    QCoreApplication::instance()->arguments())) {
		success = true;

		/* Exit OBS */

		/* This is not the nicest way to terminate our own process,
			* yet, given that we are not looking for a clean shutdown
			* but will rather overwrite settings files, this is
			* acceptable.
			*
			* It is also likely to overcome any shutdown issues OBS
			* might have, and which appear from time to time. We definitely
			* do NOT want those attributed to Cloud Restore.
			*/

		::TerminateProcess(GetCurrentProcess(), 0);
		//::exit(0);
		//QApplication::quit();
		//auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());

		//mainWindow->close();
	}
}
#endif


void DispatchClientMessage(std::string target, CefRefPtr<CefProcessMessage> msg)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetClientMessage("system", target, msg);
}

void DispatchJSEventGlobal(std::string event, std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", event, eventArgsJson);
}

void DispatchJSEventContainer(std::string target, std::string event,
			      std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetJSEvent("system", target, event,
					 eventArgsJson);
}

















void DeserializeRevealFileInGraphicalShell(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output)
{
	output->SetBool(false);

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	const QString pathIn = input->GetString().ToString().c_str();

	const QFileInfo fileInfo(pathIn);

	if (!fileInfo.exists())
		return;

#ifdef WIN32
	QString explorer = QStandardPaths::findExecutable(
		QString::fromLatin1("explorer.exe"));

	if (explorer.isEmpty()) {
		return;
	}

	QStringList param;
	if (!fileInfo.isDir())
		param += QLatin1String("/select,");
	param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());

	output->SetBool(QProcess::startDetached(explorer, param));
#endif

#ifdef APPLE
	QStringList scriptArgs;
	scriptArgs
		<< QLatin1String("-e")
		<< QString::fromLatin1(
			   "tell application \"Finder\" to reveal POSIX file \"%1\"")
			   .arg(fileInfo.canonicalFilePath());
	QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
	scriptArgs.clear();
	scriptArgs << QLatin1String("-e")
		   << QLatin1String("tell application \"Finder\" to activate");
	output->SetBool(QProcess::execute(QLatin1String("/usr/bin/osascript"),
					  scriptArgs) == 0);
#endif
}
