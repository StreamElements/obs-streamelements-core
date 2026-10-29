#pragma once

#include "ISEWorkerManager.hpp"

#include "SEUtils.hpp"
#include "SEObsAppMonitor.hpp"
#include "SEBrowserWidget.hpp"

#include "cef-headers.hpp"

#include <string>
#include <map>
#include <mutex>

class SEWorkerManager : public SEObsAppMonitor, public ISEWorkerManager {
private:
	class SEWorker;

public:
	SEWorkerManager(std::shared_ptr<SEApiServices> apiServices);
	~SEWorkerManager();

public:
	void RemoveAll() override;
	std::string Add(std::string requestedId,
			std::string url, std::string executeJavascriptOnLoad);
	void Remove(std::string id) override;
	std::string GetContent(std::string id);
	void GetIdentifiers(std::vector<std::string> &result) override;

public:
	void Serialize(CefRefPtr<CefValue> &output) override;
	void Deserialize(CefRefPtr<CefValue>& input);

	bool SerializeOne(std::string id, CefRefPtr<CefValue>& output);
	std::string DeserializeOne(CefRefPtr<CefValue> input) override;

protected:
	virtual void OnObsExit();

private:
	std::recursive_mutex m_mutex;

	std::map<std::string, std::shared_ptr<SEWorker>> m_items;

	//
	// Weak. Everything this class builds from the bundle -- browser
	// widgets and API message handlers -- holds a strong reference to it,
	// and this class destroys them, so a strong reference here closes a
	// ring the refcount can never unwind. See the comment in
	// SEApiServices.hpp.
	//
	std::weak_ptr<SEApiServices> m_apiServices;
};
