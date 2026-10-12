#pragma once

#include "SEUtils.hpp"
#include "SEObsAppMonitor.hpp"
#include "SEBrowserWidget.hpp"

#include "cef-headers.hpp"

#include <string>
#include <map>
#include <mutex>

class SEWorkerManager :
	public SEObsAppMonitor
{
private:
	class SEWorker;

public:
	SEWorkerManager();
	~SEWorkerManager();

public:
	void RemoveAll();
	std::string Add(std::string requestedId,
			std::string url, std::string executeJavascriptOnLoad);
	void Remove(std::string id);
	std::string GetContent(std::string id);
	void GetIdentifiers(std::vector<std::string>& result);

public:
	void Serialize(CefRefPtr<CefValue>& output);
	void Deserialize(CefRefPtr<CefValue>& input);

	bool SerializeOne(std::string id, CefRefPtr<CefValue>& output);
	std::string DeserializeOne(CefRefPtr<CefValue> input);

protected:
	virtual void OnObsExit();

private:
	std::recursive_mutex m_mutex;

	std::map<std::string, std::shared_ptr<SEWorker>> m_items;
};
