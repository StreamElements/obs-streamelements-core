#pragma once

#include <mutex>
#include <list>
#include <map>

#include "cef-headers.hpp"

#include "SEControllerServer.hpp"
#include "SEHttpServerManager.hpp"


#include <future>

// Message bus for exchanging messages between:
//
// * Background Workers
// * CEF Dialogs
// * CEF Docking Widgets
// * Browser Sources
//
class SEHostEventDispatcher;
class SEConfig;

class SEMessageBus
{
private:
	struct Private {
		explicit Private() = default;
	};

public:
	typedef uint32_t message_destination_filter_flags_t;

	// Message destination type flags
	static const message_destination_filter_flags_t DEST_ALL;		  // All destinations
	static const message_destination_filter_flags_t DEST_ALL_LOCAL;		  //   All local destinations
	static const message_destination_filter_flags_t DEST_UI;		  //     UI
	static const message_destination_filter_flags_t DEST_WORKER;		  //     Background workers
	static const message_destination_filter_flags_t DEST_BROWSER_SOURCE;	  //     Browser sources
	static const message_destination_filter_flags_t DEST_ALL_EXTERNAL;	  //   All external destinations
	static const message_destination_filter_flags_t DEST_EXTERNAL_CONTROLLER; //     External controllers

	// Message sources
	static const char* const SOURCE_APPLICATION;
	static const char* const SOURCE_WEB;
	static const char* const SOURCE_EXTERNAL;

public:
	//
	// Takes the dispatcher rather than calling the free functions that
	// fetched the websocket server from the singleton. The Private tag
	// keeps construction behind Create(), so that nothing can build a
	// second bus by accident -- listeners registered on one bus would
	// never hear events published on the other.
	//
	SEMessageBus(Private,
		     std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher,
		     std::shared_ptr<SEConfig> config);
	virtual ~SEMessageBus();

	//
	// A host* event, announced to external controllers. The translation
	// -- drop the "host" prefix, lower-case what follows -- lived in five
	// identical copies of a file-static helper, one per class that
	// announces anything.
	//
	void NotifyExternalHostEvent(std::string hostEventName,
				     std::string argsJson);

public:
	//
	// The only way to build one. SEGlobalStateManager owns the instance
	// and publishes it in SEApiServices; there is no static instance any
	// more.
	//
	static std::shared_ptr<SEMessageBus>
	Create(std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher,
	       std::shared_ptr<SEConfig> config);

public:
	void AddListener(std::string target, message_destination_filter_flags_t type);
	void RemoveListener(std::string target);

public:
	void DeserializeBrowserHttpServer(std::string target,
					  CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output);

	void SerializeBrowserHttpServers(std::string target,
					 CefRefPtr<CefValue> &output);

	void RemoveBrowserHttpServersByIds(std::string target,
					   CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

public:
	// Deliver event message to specific browser
	//
	void NotifyEventListener(std::string target, std::string scope,
				 std::string source, std::string sourceAddress,
				 std::string event,
				 CefRefPtr<CefValue> payload);

	// Deliver event message to all local listeners (CEF UI, CEF Dialog, Background Worker)
	// except Browser Sources.
	//
	void NotifyAllLocalEventListeners(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		std::string event,
		CefRefPtr<CefValue> payload);

	// Deliver message to all external controllers
	//
	void NotifyAllExternalEventListeners(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		std::string event,
		CefRefPtr<CefValue> payload);

	// Deliver message to all listeners, including Browser Sources and External Controllers
	//
	void NotifyAllMessageListeners(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload)
	{
		if (HandleSystemCommands(types, source, sourceAddress, payload)) {
			// System command was handled
		}

		if (DEST_ALL_LOCAL & types) {
			NotifyAllLocalEventListeners(
				types,
				source,
				sourceAddress,
				"hostMessageReceived",
				payload);
		}

		if (DEST_ALL_EXTERNAL & types) {
			m_external_controller_server.SendMessageAllClients(
				source,
				sourceAddress,
				payload);
		}
	}

protected:
	// Handle special system command messages.
	//
	// System commands are usually received from external controllers.
	//
	virtual bool HandleSystemCommands(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload);

public:
	// Notify listeners about the current system state.
	//
	// The system state contains at least <isSignedIn = true/false>.
	//
	virtual void PublishSystemState();

private:
	class WaitingHttpRequestState {
	public:
		WaitingHttpRequestState(const HttpServer::request_t *req,
					HttpServer::response_t *res)
			: request(req), response(res)
		{
		}
		~WaitingHttpRequestState()
		{
		}

	public:
		std::promise<void> promise;
		std::shared_future<void> future = promise.get_future();
		const HttpServer::request_t *request;
		HttpServer::response_t *response;
	};

public:
	void DeserializeHttpRequestResponse(CefRefPtr<CefValue> idInput,
					    CefRefPtr<CefValue> responseInput,
					    CefRefPtr<CefValue> &output);

private:
	class ListenerItem {
	public:
		ListenerItem(std::string target_,
			     message_destination_filter_flags_t flags_)
			: target(target_), flags(flags_)
		{
		}

	public:
		std::string target;
		message_destination_filter_flags_t flags;
	};

	std::recursive_mutex m_listener_list_mutex;
	std::map<std::string, std::shared_ptr<ListenerItem>> m_listener_list;
	std::map<std::string,
		 std::shared_ptr<SEHttpServerManager>>
		m_listener_http_servers;
	std::recursive_mutex m_waiting_http_requests_mutex;
	std::map<std::string, std::shared_ptr<WaitingHttpRequestState>>
		m_waiting_http_requests;
	SEControllerServer m_external_controller_server;

private:

	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;
	std::shared_ptr<SEConfig> m_config;
};
