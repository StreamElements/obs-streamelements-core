/*
 * MacOS X Crash Handler.
 *
 * The following code was used as reference when writing this module:
 * https://github.com/danzimm/mach_fun/blob/master/exceptions.c
 *
 * The best description of this code can be obtained here:
 * https://stackoverflow.com/questions/2824105/handling-mach-exceptions-in-64bit-os-x-application
 *
 * To handle mach exceptions, you have to register a mach port for the exceptions you are interested in.
 * You then wait for a message to arrive on the port in another thread. When a message arrives,
 * you call exc_server() whose implementation is provided by System.library. exec_server() takes the
 * message that arrived and calls one of three handlers that you must provide. catch_exception_raise(),
 * catch_exception_raise_state(), or catch_exception_raise_state_identity() depending on the
 * arguments you passed to task_set_exception_ports(). This is how it is done for 32 bit apps.
 *
 * For 64 bit apps, the 32 bit method still works but the data passed to you in your handler may be
 * truncated to 32 bits. To get 64 bit data passed to your handlers requires a little extra work that
 * is not very straight forward and as far as I can tell not very well documented. I stumbled onto the
 * solution by looking at the sources for GDB.
 *
 * Instead of calling exc_server() when a message arrives at the port, you have to call mach_exc_server()
 * instead. The handlers also have to have different names as well catch_mach_exception_raise(),
 * catch_mach_exception_raise_state(), and catch_mach_exception_raise_state_identity(). The parameters
 * for the handlers are the same as their 32 bit counterparts. The problem is that mach_exc_server() is
 * not provided for you the way exc_server() is. To get the implementation for mach_exc_server() requires
 * the use of the MIG (Mach Interface Generator) utility. MIG takes an interface definition file and
 * generates a set of source files that include a server function that dispatches mach messages to handlers
 * you provide. The 10.5 and 10.6 SDKs include a MIG definition file <mach_exc.defs> for the exception
 * messages and will generate the mach_exc_server() function. You then include the generated source files
 * in your project and then you are good to go.
 *
 * The nice thing is that if you are targeting 10.6+ (and maybe 10.5) you can use the same exception
 * handling for both 32 and 64 bit. Just OR the exception behavior with MACH_EXCEPTION_CODES when you set
 * your exception ports. The exception codes will come through as 64 bit values but you can truncate them
 * to 32 bits in your 32 bit build.
 *
 * I took the mach_exc.defs file and copied it to my source directory, opened a terminal and used the
 * command mig -v mach_exc.defs. This generated mach_exc.h, mach_excServer.c, and mach_excUser.c. I then
 * included those files in my project, added the correct declaration for the server function in my source
 * file and implemented my handlers. I then built my app and was good to go.
 */

#include "StreamElementsCrashHandler.hpp"
#include "StreamElementsGlobalStateManager.hpp"

//#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <pthread.h>

#include <mutex>

//#include "mach_exc.h"

#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

#import <BugSplatMac/BugSplat.h>
#import <BugSplatMac/BugSplatDelegate.h>
//@import BugSplatMac;
//#import <BugSplatMac/BugSplat.h>

#include "cef-headers.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsConfig.hpp"
#include <util/base.h>
#include <util/platform.h>
#include <util/config-file.h>
#include <obs-frontend-api.h>
#include <time.h>
#include <cstdio>
#include <string>
#include <ios>
#include <fstream>
#include "deps/zip/zip.h"
#include <iostream>
#include <filesystem>
#include <stdio.h>
#include <fcntl.h>
#include <condition_variable>
#include <QUrl>

/* ================================================================= */

static config_t *obs_fe_user_config()
{
	auto config = StreamElementsConfig::GetInstance();

	if (config) {
		return config->GetObsUserConfig();
	}

	return nullptr;
}

/* ================================================================= */

unsigned int s_maxRemainingLogFilesCount = 7;

/* ================================================================= */

static std::string GenerateTimeDateFilename(const char *extension,
					    bool noSpace = false)
{
	time_t now = time(0);
	char file[256] = {};
	struct tm *cur_time;

	cur_time = localtime(&now);
	snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d.%s",
		 cur_time->tm_year + 1900, cur_time->tm_mon + 1,
		 cur_time->tm_mday, noSpace ? '_' : ' ', cur_time->tm_hour,
		 cur_time->tm_min, cur_time->tm_sec, extension);

	return std::string(file);
}

static void delete_oldest_file(bool has_prefix, const char *location)
{
	UNUSED_PARAMETER(has_prefix);

	char *basePathPtr = os_get_config_path_ptr(location);
	std::string logDir(basePathPtr);
	bfree(basePathPtr);

	std::string oldestLog;
	time_t oldest_ts = (time_t)-1;
	struct os_dirent *entry;

	os_dir_t *dir = os_opendir(logDir.c_str());
	if (dir) {
		unsigned int count = 0;

		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			std::string filePath =
				logDir + "/" + std::string(entry->d_name);
			struct stat st;
			if (0 == os_stat(filePath.c_str(), &st)) {
				time_t ts = st.st_ctime;

				if (ts) {
					if (ts < oldest_ts) {
						oldestLog = filePath;
						oldest_ts = ts;
					}

					count++;
				}
			}
		}

		os_closedir(dir);

		if (count > s_maxRemainingLogFilesCount) {
			os_unlink(oldestLog.c_str());
		}
	}
}

static void write_file_content(std::string &path, const char *content)
{
	std::fstream file;

#ifdef _WIN32
	std::wstring wpath = utf8_to_wstring(path);

	file.open(wpath, std::ios_base::in | std::ios_base::out |
				 std::ios_base::trunc | std::ios_base::binary);
#else
	file.open(path, std::ios_base::in | std::ios_base::out |
				std::ios_base::trunc | std::ios_base::binary);
#endif
	file << content;

	file.close();
}

/* ================================================================= */

static std::string GetTemporaryFolderPath() {
	char* folder_path = obs_module_file("crashes/current");
	std::string result = folder_path;
	bfree(folder_path);
	
	os_mkdirs(result.c_str());
	
	return result;
}

static bool GetTemporaryFilePath(const char *basename,
					const char *ext,
					std::string &tempBufPath)
{
	tempBufPath = GetTemporaryFolderPath() + basename + std::string(ext);
	
	return true;
}

static inline void AddObsConfigurationFiles()
{
	const size_t BUF_LEN = 2048;

	std::string tempBufPath;
	if (!GetTemporaryFilePath("selive-error-report-data", ".zip",
				  tempBufPath))
		return;

	char programDataPathBuf[BUF_LEN];
	int ret = os_get_config_path(programDataPathBuf, BUF_LEN, "obs-studio");

	if (ret <= 0) {
		return;
	}

	std::wstring obsDataPath = QString(programDataPathBuf).toStdWString();

	zip_t *zip = zip_open(tempBufPath.c_str(), 9, 'w');

	if (!zip) {
		return;
	}

	auto addBufferToZip = [&](const char *buf, size_t bufLen,
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

	auto addFileToZip = [&](std::string localPath, std::wstring zipPath) {
		int fd = open(localPath.c_str(), O_RDONLY,
				 0, 0 /*_S_IREAD | _S_IWRITE*/);

		if (-1 != fd) {
			size_t BUF_LEN = 32768;

			char *buf = new char[BUF_LEN];

			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			int read_count = read(fd, buf, BUF_LEN);
			while (read_count > 0) {
				if (0 != zip_entry_write(zip, buf, read_count)) {
					break;
				}

				read_count = read(fd, buf, BUF_LEN);
			}

			zip_entry_close(zip);

			delete[] buf;

			close(fd);
		} else {
			// Failed opening file for reading
			//
			// This is a crash handler: you can't really do anything
			// here to mitigate.
		}
	};

	std::string package_manifest = "generator=crash_handler\nversion=4\n";
	addBufferToZip(package_manifest.c_str(),
		       package_manifest.size(), L"manifest.ini");

	std::map<std::string, std::wstring> local_to_zip_files_map;

	// Collect files
	std::vector<std::string> blacklist = {
		"plugin_config/obs-streamelements/obs-streamelements-update.exe",
		"plugin_config/obs-browser/cache/",
		"plugin_config/obs-browser/blob_storage/",
		"plugin_config/obs-browser/code cache/",
		"plugin_config/obs-browser/gpucache/",
		"plugin_config/obs-browser/visited links/",
		"plugin_config/obs-browser/transportsecurity/",
		"plugin_config/obs-browser/videodecodestats/",
		"plugin_config/obs-browser/session storage/",
		"plugin_config/obs-browser/service worker/",
		"plugin_config/obs-browser/pepper data/",
		"plugin_config/obs-browser/indexeddb/",
		"plugin_config/obs-browser/file system/",
		"plugin_config/obs-browser/databases/",
		"plugin_config/obs-browser/obs-streamelements-core.ini.bak",
		"plugin_config/obs-browser/cef.",
		"plugin_config/obs-browser/obs_profile_cookies/",
		"plugin_config/obs-streamelements-core/cache/",
		"plugin_config/obs-streamelements-core/blob_storage/",
		"plugin_config/obs-streamelements-core/code cache/",
		"plugin_config/obs-streamelements-core/gpucache/",
		"plugin_config/obs-streamelements-core/visited links/",
		"plugin_config/obs-streamelements-core/transportsecurity/",
		"plugin_config/obs-streamelements-core/videodecodestats/",
		"plugin_config/obs-streamelements-core/session storage/",
		"plugin_config/obs-streamelements-core/service worker/",
		"plugin_config/obs-streamelements-core/pepper data/",
		"plugin_config/obs-streamelements-core/indexeddb/",
		"plugin_config/obs-streamelements-core/file system/",
		"plugin_config/obs-streamelements-core/databases/",
		"plugin_config/obs-streamelements-core/obs-streamelements-core.ini.bak",
		"plugin_config/obs-streamelements-core/cef.",
		"plugin_config/obs-streamelements-core/obs_profile_cookies/",
		"plugin_config/obs-streamelements-core/crashes/",
		"updates/",
		"profiler_data/",
		"obslive_restored_files/",
		"plugin_config/obs-browser/streamelements_restored_files/",
		"plugin_config/obs-streamelements-core/streamelements_restored_files/",
		"crashes/"};

	// Collect all files
	for (auto &i :
	     std::filesystem::recursive_directory_iterator(
		     programDataPathBuf)) {
		if (!std::filesystem::is_directory(i.path())) {
			std::string local_path = i.path().string();
			std::wstring zip_path =
				utf8_to_wstring(local_path).substr(obsDataPath.size() + 1);

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
						    0, item.size()) == utf8_to_wstring(item)) {
						accept = false;

						break;
					}
				}
			}

			if (accept) {
				local_to_zip_files_map[local_path] =
					L"obs-studio/" + zip_path;
			}
		}
	}

	for (auto item : local_to_zip_files_map) {
		addFileToZip(item.first, item.second);
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

		if (sizeof(void*) == 8) {
			d->SetString("platformArch", "64bit");
		} else {
			d->SetString("platformArch", "32bit");
		}

		d->SetString("streamelementsPluginVersion",
			     GetStreamElementsPluginVersionString());

		d->SetString("machineUniqueId", GetComputerSystemUniqueId());

		addCefValueToZip(basicInfo, L"system\\basic.json");
	}

	GetAsyncCallContextStack([&](const StreamElementsAsyncCallContextStack_t
					     *asyncCallContextStack) {
		if (!asyncCallContextStack->size())
			return;

		std::vector<std::string> lines;

		lines.push_back("Asynchronous call context:");
		lines.push_back("");

		for (auto item : *asyncCallContextStack) {
			char buf[32];
			sprintf(buf, "%d", item->line);
			
			lines.push_back(
				item->file.c_str() + std::string(" (line: ") +
				std::string(buf) + ")");
		}

		addLinesBufferToZip(lines, L"async_context.txt");

		// Fill in async call details notes

		auto asyncContextList = CefListValue::Create();

		for (auto item : *asyncCallContextStack) {
			auto d = CefDictionaryValue::Create();

			d->SetString("file", item->file);
			d->SetInt("line", item->line);
			d->SetInt("thread", item->thread);
			d->SetBool("running", item->running);

			asyncContextList->SetDictionary(
				asyncContextList->GetSize(), d);
		}
				// Retrieve temporary path
		std::string wtempBufPath;
		if (!GetTemporaryFilePath("async-context", ".json",
					  wtempBufPath))
			return;

		auto asyncContextListValue = CefValue::Create();

		asyncContextListValue->SetList(asyncContextList);

		auto json = CefWriteJSON(asyncContextListValue,
					 JSON_WRITER_PRETTY_PRINT)
				    .ToString();

		os_quick_write_utf8_file(wtempBufPath.c_str(),
					 json.c_str(), json.size(), false);
	});

	zip_close(zip);

	////////////////////////////////////////////////////////////////////
	// Process ApiContext
	////////////////////////////////////////////////////////////////////

	GetApiContext([](StreamElementsApiContext_t *apiContext) {
		auto apiContextList = CefListValue::Create();

		int apiContextIndex = 0;
		for (auto api : *apiContext) {
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
		std::string wtempBufPath;
		if (!GetTemporaryFilePath("api-context", ".json",
					  wtempBufPath))
			return;

		auto apiContextListValue = CefValue::Create();

		apiContextListValue->SetList(apiContextList);

		auto json = CefWriteJSON(apiContextListValue,
					 JSON_WRITER_PRETTY_PRINT)
				    .ToString();

		os_quick_write_utf8_file(wtempBufPath.c_str(),
					 json.c_str(), json.size(), false);
	});
}

/* ================================================================= */


static bool AmIBeingDebugged(void)
    // Returns true if the current process is being debugged (either
    // running under the debugger or has a debugger attached post facto).
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.

    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.

    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

static bool initialized = false;

// Called by one of the crash handlers below to report a crash event to the analytics service
static void TrackCrash(const char* caller_reference)
{
    if (!initialized) return;

    blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: TrackCrash: %s", caller_reference);
	
    AddObsConfigurationFiles();

    // This will also report the platform and a bunch of other props
    StreamElementsGlobalStateManager::GetInstance()
        ->GetAnalyticsEventsManager()
        ->trackSynchronousEvent("OBS Studio Crashed", json11::Json::object{
            { "platformCallerReference", caller_reference }
        });
    
    initialized = false;
}

extern "C" {
    // Implementation generated by mig from mach_exc.defs
    extern boolean_t mach_exc_server(mach_msg_header_t *, mach_msg_header_t *);

    // Exception handler, triggered by mach_exc_server
    kern_return_t catch_mach_exception_raise(mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t type, exception_data_t code, mach_msg_type_number_t code_count) {

        TrackCrash("catch_mach_exception_raise");

        return KERN_INVALID_ADDRESS;
    }

    // Exception handler, triggered by mach_exc_server
    kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port, exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_count, int *flavor, thread_state_t in_state, mach_msg_type_number_t in_state_count, thread_state_t out_state, mach_msg_type_number_t *out_state_count) {

        TrackCrash("catch_mach_exception_raise_state");

        return KERN_INVALID_ADDRESS;
    }

    // Exception handler, triggered by mach_exc_server
    kern_return_t catch_mach_exception_raise_state_identity(mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_count, int *flavor, thread_state_t in_state, mach_msg_type_number_t in_state_count, thread_state_t out_state, mach_msg_type_number_t *out_state_count) {
        TrackCrash("catch_mach_exception_raise_state_identity");

        return KERN_INVALID_ADDRESS;
    }

    // Exception message pump thread
    static void *server_thread(void *arg) {
        mach_port_t exc_port = *(mach_port_t *)arg;
        kern_return_t kr;

        // Upon exception, the handler will set initialized = false
        //
        // When this happens, the OS will retry executing the failed instruction, that in
        // turn will trigger another exception which will terminate the program and trigger
        // Apple Crash Report as described here:
        // https://developer.apple.com/forums/thread/113742
        //
        // The Crash Report is not something we have access to when handling the exception
        // so in order to get it to developers, we'll package most recent crash reports when
        // user reports an issue.
        //
        // The Apple Crash Report is important since this is the only way to ensure we are
        // getting complete crash information, including source-line-level stack traces.
        // See the link above for an explanation WHY.
        //
        while(initialized) {
            if ((kr = mach_msg_server_once(mach_exc_server, 4096, exc_port, 0)) != KERN_SUCCESS) {
                blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: mach_msg_server_once: error %#x\n", kr);
                break;
            }
        }
        return (NULL);
    }

    // Initialize exception handler
    static void exn_init() {
        kern_return_t kr = 0;
        static mach_port_t exc_port;
        mach_port_t task = mach_task_self();
        pthread_t exception_thread;
        int err;
      
        mach_msg_type_number_t maskCount = 1;
        exception_mask_t mask;
        exception_handler_t handler;
        exception_behavior_t behavior;
        thread_state_flavor_t flavor;

        // Obtain Mach port which will receive exception messages
        if ((kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &exc_port)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: mach_port_allocate: %#x\n", kr);
            return;
        }

        // Add send right to the exeption Mach port
        if ((kr = mach_port_insert_right(task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: mach_port_allocate: %#x\n", kr);
            return;
        }

        // Get current Mach exception port settings
        if ((kr = task_get_exception_ports(task, EXC_MASK_ALL, &mask, &maskCount, &handler, &behavior, &flavor)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: task_get_exception_ports: %#x\n", kr);
            return;
        }

        // Start exception message pump thread
        if ((err = pthread_create(&exception_thread, NULL, server_thread, &exc_port)) != 0) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: pthread_create server_thread: %s\n", strerror(err));
            return;
        }
      
        pthread_detach(exception_thread);

        // Set exception port settings to receive all exceptions with exception codes
        if ((kr = task_set_exception_ports(task, EXC_MASK_ALL, exc_port, EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, flavor)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: task_set_exception_ports: %#x\n", kr);
            return;
        }

        // We're done
        blog(LOG_INFO, "obs-streamelements-core: StreamElements: Crash Handler: Initialized");
    }
}

@interface MyBugSplatDelegate : NSObject <BugSplatDelegate>
@end

@implementation MyBugSplatDelegate

- (instancetype) init
{
	self = [super init];
	
	return self;
}

- (void)bugSplatWillSendCrashReport:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillSendCrashReport called");
}

- (void)bugSplatWillSendCrashReportsAlways:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillSendCrashReportsAlways called");
}

- (void)bugSplatDidFinishSendingCrashReport:(BugSplat *)bugSplat {
    NSLog(@"bugSplatDidFinishSendingCrashReport called");
}

- (void)bugSplatWillCancelSendingCrashReport:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillCancelSendingCrashReport called");
}

- (void)bugSplatWillShowSubmitCrashReportAlert:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillShowSubmitCrashReportAlert called");
}

- (void)bugSplat:(BugSplat *)bugSplat didFailWithError:(NSError *)error {
    NSLog(@"bugSplat:didFailWithError: %@", [error localizedDescription]);
}

- (NSString *)applicationKeyForBugSplat:(BugSplat *)bugSplat signal:(NSString *)signal exceptionName:(NSString *)exceptionName exceptionReason:(NSString *)exceptionReason API_AVAILABLE(macosx(10.13)) {
    NSLog(@"applicationKeyForBugSplat called");

    auto appKey = GetStreamElementsPluginVersionString();
	
    id appKeyNSString = [NSString stringWithCString:appKey.c_str()
					   encoding:[NSString defaultCStringEncoding]];

    return [NSString stringWithFormat:@"SE.Live (Mac) %@", appKeyNSString];
}

- (NSArray<BugSplatAttachment *> *)attachmentsForBugSplat:(BugSplat *)bugSplat API_AVAILABLE(macosx(10.13)) {
	NSLog(@"attachmentsForBugSplat called");

	[[BugSplat shared] setValue:@"SE.Live" forAttribute:@"product"];
	
	id res = [[NSArray<BugSplatAttachment *> alloc] init];
	
	return res;
}

@end

StreamElementsCrashHandler::StreamElementsCrashHandler()
{
    static std::mutex mutex;

    std::lock_guard<std::mutex> guard(mutex);

    if (!initialized) {
        initialized = true;
        
	if (!AmIBeingDebugged())
		exn_init();

	    [[BugSplat shared] setDelegate:[[MyBugSplatDelegate alloc] init]];
	    [[BugSplat shared] setPresentModally:YES];
	    [[BugSplat shared] setAutoSubmitCrashReport:NO];
	    [[BugSplat shared] setPersistUserDetails:YES];
	    [[BugSplat shared] setPresentModally:YES];
	    [[BugSplat shared] setBugSplatDatabase:@"OBS_Live"];
	    [[BugSplat shared] start];
    }
}

StreamElementsCrashHandler::~StreamElementsCrashHandler()
{
    // We never destroy the exception handler
    initialized = false;
}
