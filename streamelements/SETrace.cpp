#include "SETrace.hpp"

#include <obs.h>

#include <string>
#include <map>
#include <memory>
#include <list>
#include <shared_mutex>
#include <filesystem>

#include "deps/StackWalker/StackWalker.h"

#include <Windows.h>

static class SETraceRefDataItem {
public:
	std::string m_file;
	int m_line;
	std::string m_statement;
	int m_count = 0;

	SETraceRefDataItem(const char *file, const int line,
			   const char *statement)
		: m_file(file), m_line(line), m_statement(statement)
	{
		++m_count;
	}
};

static class SETraceData {
public:
	std::map<std::string, std::shared_ptr<SETraceRefDataItem>> m_addref;
	std::map<std::string, std::shared_ptr<SETraceRefDataItem>> m_releases;
	int m_refCount = 0;

	SETraceData(const char* stackKey, const char *file, const int line, const char *statement) {
		++m_refCount;

		m_addref[stackKey] = std::make_shared<SETraceRefDataItem>(
			file, line, statement);
	}

	void Dump()
	{
		if (m_refCount == 0)
			return;

		blog(LOG_INFO,
		     "[obs-streamelements-core]: --------------------------------------------------------------");

		blog(LOG_ERROR,
		     "[obs-streamelements-core]: unbalanced reference count (%d) of data object:", m_refCount);

		int addCount = 0;
		for (const auto &kv : m_addref) {
			addCount += kv.second->m_count;
		}

		blog(LOG_ERROR, "[obs-streamelements-core]:");
		blog(LOG_ERROR,
		     "[obs-streamelements-core]:     -> ++++++++++++++++++++++++ AddRefs (%d): ++++++++++++++++++++++++", addCount);
		blog(LOG_ERROR, "[obs-streamelements-core]:");

		for (const auto &kv : m_addref) {
			blog(LOG_ERROR,
			     "[obs-streamelements-core]:         + %s:%d : count(%d) %s : %s",
			     kv.second->m_file.c_str(), kv.second->m_line,
			     kv.second->m_count, kv.second->m_statement.c_str(),
			     kv.first.c_str());
		}

		int releaseCount = 0;
		for (const auto &kv : m_releases) {
			releaseCount += kv.second->m_count;
		}

		blog(LOG_ERROR, "[obs-streamelements-core]:");
		blog(LOG_ERROR,
		     "[obs-streamelements-core]:     -> ------------------------ DecRefs (%d): ------------------------",
		     releaseCount);
		blog(LOG_ERROR, "[obs-streamelements-core]:");

		for (const auto &kv : m_releases) {
			blog(LOG_ERROR,
			     "[obs-streamelements-core]:         -> %s:%d : count(%d) %s : %s",
			     kv.second->m_file.c_str(), kv.second->m_line,
			     kv.second->m_count, kv.second->m_statement.c_str(),
			     kv.first.c_str());
		}

		blog(LOG_INFO,
		     "[obs-streamelements-core]: --------------------------------------------------------------");
	}

	~SETraceData() {
		Dump();
	}
};

static class MyStackWalker : public StackWalker {
public:
	std::string *m_output = nullptr;

	MyStackWalker(int options)
		: StackWalker(options)
	{
	}

protected:
	virtual void OnCallstackEntry(CallstackEntryType eType,
				      CallstackEntry &entry) override
	{
		StackWalker::OnCallstackEntry(eType, entry);

		if (!m_output)
			return;

		//*m_output += entry.loadedImageName;
		//*m_output += "!";
		//*m_output += entry.undFullName;
		//*m_output += " ";
		std::filesystem::path p = entry.lineFileName;

		std::string filename = p.filename().u8string();

		if (filename._Starts_with("StreamElements")) {
			if (m_output->size()) {
				*m_output += " <- ";
			}

			*m_output += filename;
			*m_output += ":";
			char numbuf[32];
			*m_output += ltoa(entry.lineNumber, numbuf, 10);
		}
	}
};

static void GetStackKey(const char* file, int line, std::string &stackKey) {
	CONTEXT context;
	RtlCaptureContext(&context);

	std::filesystem::path p = file;

	stackKey += p.filename().u8string();
	char numbuf[8];
	stackKey += ":";
	stackKey += ltoa(line, numbuf, 10);

	// NOT thread safe!
	static MyStackWalker walker(       //StackWalker::RetrieveSymbol |
		StackWalker::RetrieveLine  //|
					   //StackWalker::RetrieveModuleInfo
	);

	walker.m_output = &stackKey;

	walker.ShowCallstack(::GetCurrentThread(), &context);
}

std::map<const void *, std::shared_ptr<SETraceData>> g_traceRefData;
std::shared_mutex g_traceRefDataMutex;

void* __SETrace_Trace_AddRef(const char* file, const int line,
	const char* statement, void* ptr)
{
	std::unique_lock guard(g_traceRefDataMutex);

	std::filesystem::path p = file;

	std::string stackKey;
	GetStackKey(file, line, stackKey);

	if (!g_traceRefData.count(ptr)) {
		g_traceRefData[ptr] = std::make_shared<SETraceData>(
			stackKey.c_str(), p.filename().u8string().c_str(), line,
			statement);
	} else {
		if (g_traceRefData[ptr]->m_addref.count(stackKey)) {
			g_traceRefData[ptr]->m_addref[stackKey]->m_count++;
		} else {
			g_traceRefData[ptr]->m_addref[stackKey] =
				std::make_shared<SETraceRefDataItem>(
					p.filename().u8string().c_str(), line,
					statement);
		}

		g_traceRefData[ptr]->m_refCount++;
	}

	return ptr;
}

void* __SETrace_Trace_DecRef(const char* file, const int line,
	const char* statement, void* ptr)
{
	std::unique_lock guard(g_traceRefDataMutex);

	std::filesystem::path p = file;

	std::string stackKey;
	GetStackKey(file, line, stackKey);

	if (g_traceRefData.count(ptr)) {
		g_traceRefData[ptr]->m_refCount--;


		if (g_traceRefData[ptr]->m_releases.count(stackKey)) {
			g_traceRefData[ptr]->m_releases[stackKey]->m_count++;
		} else {
			g_traceRefData[ptr]->m_releases[stackKey] =
				std::make_shared<SETraceRefDataItem>(
					p.filename().u8string().c_str(), line,
					statement);
		}
	} else {
		blog(LOG_ERROR,
		     "[obs-streamelements-core]: release reference of data which was not previously allocated at '%s' line '%d': %s",
		     p.filename().u8string().c_str(), line, statement);
	}

	return ptr;
}

void __SETrace_Dump(const char* file, int line)
{
	std::unique_lock guard(g_traceRefDataMutex);

	std::filesystem::path p = file;

	int badCount = 0;
	for (const auto &kv : g_traceRefData) {
		if (kv.second->m_refCount != 0)
			++badCount;
	}

	blog(LOG_INFO,
	     "[obs-streamelements-core]: --------------------------------------------------------------");

	blog(LOG_INFO,
	     "[obs-streamelements-core]: start of reference count trace dump for %d pointers at %s:%d",
	     badCount, p.filename().u8string().c_str(), line);

	bool isFirst = false;
	for (const auto &kv : g_traceRefData) {
		kv.second->Dump();
	}

	blog(LOG_INFO,
	     "[obs-streamelements-core]: end of reference count trace dump for %d pointers at %s:%d",
	     badCount, p.filename().u8string().c_str(), line);

	blog(LOG_INFO,
	     "[obs-streamelements-core]: --------------------------------------------------------------");
}
