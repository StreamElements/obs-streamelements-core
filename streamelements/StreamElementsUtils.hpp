#pragma once

#include <cef-headers.hpp>
#include <obs.h>
#include <obs-module.h>

#include <memory>
#include <iostream>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <map>

#include <functional>
#include <future>

#include <util/threading.h>
#include <util/platform.h>
#include <curl/curl.h>

#include <QMenu>
#include <QWidget>

#define SYNC_ACCESS()                                                    \
	static std::recursive_mutex __sync_access_mutex;                 \
	std::lock_guard<std::recursive_mutex> __sync_access_mutex_guard( \
		__sync_access_mutex);

#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QWidget>

/* ========================================================= */

bool IsTraceLogLevel();

/* ========================================================= */

std::string wstring_to_utf8(const std::wstring wstr);
std::wstring utf8_to_wstring(const std::string str);

/* ========================================================= */

template<typename... Args> std::string FormatString(const char *format, ...)
{
	int size = 512;
	char *buffer = 0;
	buffer = new char[size];
	va_list vl;
	va_start(vl, format);
	int nsize = vsnprintf(buffer, size, format, vl);
	if (size <= nsize) { //fail delete buffer and try again
		delete[] buffer;
		buffer = 0;
		buffer = new char[nsize + 1]; //+1 for /0
		nsize = vsnprintf(buffer, size, format, vl);
	}
	std::string ret(buffer);
	va_end(vl);
	delete[] buffer;
	return ret;
}

/* ========================================================= */

class StreamElementsApiContextItem {
public:
	StreamElementsApiContextItem(CefString method_,
				     CefRefPtr<CefListValue> args_,
				     uint32_t thread_)
		: method(method_), args(args_), thread(thread_)
	{
	}

public:
	CefString method;
	CefRefPtr<CefListValue> args;
	uint32_t thread;
};

class StreamElementsApiContext_t
	: public std::list<std::shared_ptr<StreamElementsApiContextItem>> {
public:
	StreamElementsApiContext_t() {}
	~StreamElementsApiContext_t()
	{
		clear();
	}
};

void GetApiContext(std::function<void(StreamElementsApiContext_t *)> callback);
std::shared_ptr<StreamElementsApiContextItem> PushApiContext(CefString method, CefRefPtr<CefListValue> args);
void RemoveApiContext(std::shared_ptr<StreamElementsApiContextItem> item);

/* ========================================================= */

class StreamElementsAsyncCallContextItem {
public:
	StreamElementsAsyncCallContextItem(std::string& file_, int line_, bool running_)
		: file(file_), line(line_), running(running_)
	{
#if _WIN32
		thread = GetCurrentThreadId();
#else
		thread = 0;
#endif
	}

public:
	std::string file = "";
	int line = -1;
	uint32_t thread = 0;
	bool running = false;
};

class StreamElementsAsyncCallContextStack_t : public
	std::list<std::shared_ptr<StreamElementsAsyncCallContextItem>> {
public:
	StreamElementsAsyncCallContextStack_t() {}
	~StreamElementsAsyncCallContextStack_t()
	{
		clear();
	}
};

void GetAsyncCallContextStack(
	std::function<void(const StreamElementsAsyncCallContextStack_t *)>
		callback);

std::shared_ptr<StreamElementsAsyncCallContextItem> AsyncCallContextPush(std::string file, int line, bool running);
void AsyncCallContextRemove(
	std::shared_ptr<StreamElementsAsyncCallContextItem> item);

class SEAsyncCallContextMarker {
private:
	std::shared_ptr<StreamElementsAsyncCallContextItem> m_contextItem = nullptr;

public:
	SEAsyncCallContextMarker(const char *file, const int line, bool running = true)
	{
		m_contextItem = AsyncCallContextPush(file, line, running);
	}

	~SEAsyncCallContextMarker()
	{
		AsyncCallContextRemove(m_contextItem);

		m_contextItem = nullptr;
	}
};

std::future<void> __QtPostTask_Impl(std::function<void()> task,
				    std::string file, int line);
std::future<void> __QtExecSync_Impl(std::function<void()> task,
				    std::string file, int line);

class QtAsyncCallFunctor {
private:
	typedef std::future<void> (*call_t)(std::function<void()> /*task*/,
					    std::string /*file*/, int /*line*/);

	std::string file;
	int line;
	call_t impl;

public:
	QtAsyncCallFunctor(const char* file_, const int line_, const call_t impl_)
		: file(file_), line(line_), impl(impl_)
	{
	}

	std::future<void> operator()(std::function<void()> task) const
	{
		return impl(task, file, line);
	}
};

std::future<void> __QtDelayTask_Impl(std::function<void()> task, int delayMs, const char* file, const int line);

#define QtDelayTask(task, delayMs) __QtDelayTask_Impl(task, delayMs, __FILE__, __LINE__)
#define QtPostTask QtAsyncCallFunctor(__FILE__, __LINE__, &__QtPostTask_Impl)
#define QtExecSync QtAsyncCallFunctor(__FILE__, __LINE__, &__QtExecSync_Impl)

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area);
std::string GetCommandLineOptionValue(const std::string key);
std::string LoadResourceString(std::string path);

/* ========================================================= */

void SerializeSystemTimes(CefRefPtr<CefValue> &output);
void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output);
void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output);

/* ========================================================= */

CefRefPtr<CefListValue> SerializeObsSourceFilters(obs_source_t *source,
						  bool serializeProperties);
bool DeserializeObsSourceFilters(obs_source_t *source,
				 CefRefPtr<CefValue> filtersValue);

void SerializeObsSource(obs_source_t *source, CefRefPtr<CefDictionaryValue> dic,
			bool isExistingSource, bool serializeProperties);

void SerializeObsSourceProperties(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output);

void SerializeAvailableInputSourceTypes(
	CefRefPtr<CefValue> &output, uint32_t requireAnyOfOutputFlagsMask,
	std::vector<obs_source_type> requiredSourceTypes,
	bool serializeProperties);
void SerializeExistingInputSources(
	CefRefPtr<CefValue> &output, uint32_t requireAnyOfOutputFlagsMask, uint32_t requireOutputFlagsMask,
	std::vector<obs_source_type> requireSourceTypes,
	bool serializeProperties);

/* ========================================================= */

std::string GetCurrentThemeName();
std::string SerializeAppStyleSheet();
std::string GetAppStyleSheetSelectorContent(std::string selector);

/* ========================================================= */

std::string GetCefVersionString();
std::string GetCefPlatformApiHash();
std::string GetCefUniversalApiHash();
std::string GetStreamElementsPluginVersionString();
std::string GetStreamElementsApiVersionString();

/* ========================================================= */

void SetGlobalCURLOptions(CURL *curl, const char *url);

typedef std::function<bool(void *data, size_t datalen, void *userdata,
			   char *error_msg, int http_code)>
	http_client_callback_t;
typedef std::function<void(char *data, void *userdata, char *error_msg,
			   int http_code)>
	http_client_string_callback_t;
typedef std::function<void(void *data, size_t datalen, void *userdata, char *error_msg,
			   int http_code)>
	http_client_buffer_callback_t;
typedef std::multimap<std::string, std::string> http_client_headers_t;

bool HttpGet(const char *url, http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata);

bool HttpGet(std::string method, const char *url,
	     http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata);

bool HttpPost(std::string method, const char *url,
	      http_client_headers_t request_headers, void *buffer,
	      size_t buffer_len, http_client_callback_t callback,
	      void *userdata);

bool HttpPost(const char *url, http_client_headers_t request_headers,
	      void *buffer, size_t buffer_len, http_client_callback_t callback,
	      void *userdata);

bool HttpGetString(std::string method, const char *url,
		   http_client_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata);

bool HttpGetString(const char *url, http_client_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata);

bool HttpGetBuffer(const char *url, http_client_headers_t request_headers,
		   http_client_buffer_callback_t callback, void *userdata);

bool HttpPostString(std::string method, const char *url,
		    http_client_headers_t request_headers, const char *postData,
		    http_client_string_callback_t callback, void *userdata);

bool HttpPostString(const char *url, http_client_headers_t request_headers,
		    const char *postData,
		    http_client_string_callback_t callback, void *userdata);

/* ========================================================= */

std::string CreateCryptoSecureRandomNumberString();
std::string CreateGloballyUniqueIdString();
std::string GetComputerSystemUniqueId();

/* ========================================================= */

struct streamelements_env_update_request {
public:
	std::string product;
	std::string key;
	std::string value;
};

typedef std::vector<streamelements_env_update_request>
	streamelements_env_update_requests;

std::string ReadProductEnvironmentConfigurationString(const char *key);
bool WriteProductEnvironmentConfigurationString(const char *key,
						const char *value);
bool WriteProductEnvironmentConfigurationStrings(
	streamelements_env_update_requests requests);
bool WriteEnvironmentConfigStrings(streamelements_env_update_requests requests);
bool WriteEnvironmentConfigString(const char *regValueName,
				  const char *regValue,
				  const char *productName);

/* ========================================================= */

bool ParseQueryString(std::string input,
		      std::map<std::string, std::string> &result);
std::string CreateSHA256Digest(std::string &input);
std::string CreateSessionMessageSignature(std::string &message);
bool VerifySessionMessageSignature(std::string &message,
				   std::string &signature);
std::string CreateSessionSignedAbsolutePathURL(std::wstring path);
bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path);

/* ========================================================= */

bool IsAlwaysOnTop(QWidget *window);
void SetAlwaysOnTop(QWidget *window, bool enable);

/* ========================================================= */

class RecursiveNestingLevelCounter {
public:
	RecursiveNestingLevelCounter(long *counter) : m_counter(counter)
	{
		m_level = os_atomic_inc_long(m_counter);
	}

	~RecursiveNestingLevelCounter() { os_atomic_dec_long(m_counter); }

	long level() { return m_level; }

private:
	long *m_counter;
	long m_level;
};

#define PREVENT_RECURSIVE_REENTRY()                                   \
	static long __recursive_nesting_level = 0L;                   \
	RecursiveNestingLevelCounter __recursive_nesting_level_guard( \
		&__recursive_nesting_level);                          \
	if (__recursive_nesting_level_guard.level() > 1)              \
		return;

double GetObsGlobalFramesPerSecond();

/* ========================================================= */

void AdviseHostUserInterfaceStateChanged();
void AdviseHostHotkeyBindingsChanged();

/* ========================================================= */

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId);

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId);

/* ========================================================= */

#if ENABLE_DECRYPT_COOKIES
void StreamElementsDecryptCefCookiesFile(const char *path_utf8);
void StreamElementsDecryptCefCookiesStoragePath(const char *path_utf8);
#endif /* ENABLE_DECRYPT_COOKIES */

/* ========================================================= */

std::string GetIdFromPointer(const void *ptr);
const void *GetPointerFromId(const char *id);

/* ========================================================= */

bool GetTemporaryFilePath(std::string prefixString, std::string &result);
std::string GetUniqueFileNameFromPath(std::string path, size_t maxLength);
std::string GetFolderPathFromFilePath(std::string filePath);

/* ========================================================= */

bool ReadListOfObsSceneCollections(std::map<std::string, std::string> &output);
bool ReadListOfObsProfiles(std::map<std::string, std::string> &output);

/* ========================================================= */

class CancelableTask {
private:
	bool cancelled = false;
	std::recursive_mutex mutex;

public:
	static std::shared_ptr<CancelableTask> Execute(std::function<void(std::shared_ptr<CancelableTask>)> task) {
		auto handle = std::make_shared<CancelableTask>();

		std::thread thread([handle, task]() {
			task(handle);
		});

		thread.detach();

		return handle;
	}

public:
	CancelableTask()
		: cancelled(false)
	{
	}

public:
	void Cancel()
	{
		cancelled = true;
	}

	bool IsCancelled()
	{
		return cancelled;
	}
};

/* ========================================================= */

typedef std::function<void(bool success, void *, size_t)>
	async_http_request_callback_t;

std::shared_ptr<CancelableTask>
HttpGetAsync(std::string url,
		async_http_request_callback_t callback);

/* ========================================================= */

bool DeserializeAndInvokeAction(CefRefPtr<CefValue> input,
				std::function<void()> defaultAction,
				std::function<void()> defaultContextMenu);

bool DeserializeDocksMenu(QMenu &menu);

bool DeserializeMenu(
	CefRefPtr<CefValue> input, QMenu &menu,
	std::function<void()> defaultAction = []() {},
	std::function<void()> defaultContextMenu = []() {});

QWidget *DeserializeAuxiliaryControlWidget(
	CefRefPtr<CefValue> input,
	std::function<void()> defaultAction = []() {},
	std::function<void()> defaultContextMenu = []() {});

QWidget *DeserializeRemoteIconWidget(CefRefPtr<CefValue> input,
				     QPixmap *defaultPixmap = nullptr);

/* ========================================================= */

void ObsSceneEnumAllItems(obs_scene_t *scene,
			  std::function<bool(obs_sceneitem_t *)> func);

void ObsSceneEnumAllItems(obs_source_t *source,
			  std::function<bool(obs_sceneitem_t *)> func);

void ObsCurrentSceneEnumAllItems(std::function<bool(obs_sceneitem_t *)> func);

/* ========================================================= */

bool IsCefValueEqual(CefRefPtr<CefValue> a, CefRefPtr<CefValue> b);

/* ========================================================= */

void ObsEnumAllScenes(std::function<bool(obs_source_t *scene)> func);

/* ========================================================= */

std::string CreateTimedObsApiTransaction(int timeoutMilliseconds = 60000);
void CompleteTimedObsApiTransaction(std::string id);

/* ========================================================= */

void RemoveObsDirtyShutdownMarker();

void RestartCurrentApplication();

bool IsSafeFileExtension(std::string path);

/* ========================================================= */

void DispatchClientMessage(std::string target,
			   CefRefPtr<CefProcessMessage> msg);

void DispatchJSEventContainer(std::string target, std::string event,
			   std::string eventArgsJson);

void DispatchJSEventGlobal(std::string event, std::string eventArgsJson);

/* ========================================================= */

bool SecureJoinPaths(std::string base, std::string subpath,
			    std::string &result);

/* ========================================================= */

bool SerializeObsProperty(obs_property_t *prop, CefRefPtr<CefValue> &output);
bool SerializeObsProperties(obs_properties_t *props,
			    CefRefPtr<CefValue> &output);

CefRefPtr<CefValue> SerializeObsData(obs_data_t *data);
bool DeserializeObsData(CefRefPtr<CefValue> input, obs_data_t *data);

CefRefPtr<CefValue>
SerializeObsEncoderProperties(std::string id, obs_data_t *settings = nullptr);

uint32_t GetInt32FromAlignmentId(std::string alignment);
std::string GetAlignmentIdFromInt32(uint32_t a);

/* ========================================================= */

void SerializeObsTransition(std::string videoCompositionId, obs_source_t *t,
			    int durationMilliseconds,
			    CefRefPtr<CefValue> &output);

obs_source_t *GetExistingObsTransition(std::string lookupId);

bool DeserializeObsTransition(CefRefPtr<CefValue> input, obs_source_t **t,
			      int *durationMilliseconds, bool useExisting);

/* ========================================================= */

CefRefPtr<CefDictionaryValue> SerializeObsEncoder(obs_encoder_t *e);

obs_encoder_t *DeserializeObsVideoEncoder(CefRefPtr<CefValue> input);
obs_encoder_t *DeserializeObsAudioEncoder(CefRefPtr<CefValue> input,
					  int mixer_idx = -1);
