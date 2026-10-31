#pragma once

#include <mutex>
#include <list>
#include <map>

#include "cef-headers.hpp"
#include "ISEMessageBus.hpp"

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

class SEMessageBus : public ISEMessageBus {
private:
	struct Private {
		explicit Private() = default;
	};

public:
	//
	// The flags typedef, the destination flags and the source names
	// moved to ISEMessageBus and are inherited from there. They are
	// vocabulary a caller uses to say where a message goes, which
	// makes them part of the contract rather than of this
	// implementation of it -- and every SEMessageBus::DEST_* spelling
	// elsewhere in the plug-in still resolves through this class, so
	// none of those sites had to change.
	//

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
	void AddListener(std::string target,
			 message_destination_filter_flags_t type) override;
	void RemoveListener(std::string target);

public:
	void DeserializeBrowserHttpServer(std::string target,
					  CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) override;

	void SerializeBrowserHttpServers(std::string target,
					 CefRefPtr<CefValue> &output) override;

	void
	RemoveBrowserHttpServersByIds(std::string target,
				      CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) override;

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
		message_destination_filter_flags_t types, std::string source,
		std::string sourceAddress, std::string event,
		CefRefPtr<CefValue> payload) override;

	// Deliver message to all external controllers
	//
	void NotifyAllExternalEventListeners(
		message_destination_filter_flags_t types, std::string source,
		std::string sourceAddress, std::string event,
		CefRefPtr<CefValue> payload) override;

	// Deliver message to all listeners, including Browser Sources and External Controllers
	//
	void NotifyAllMessageListeners(message_destination_filter_flags_t types,
				       std::string source,
				       std::string sourceAddress,
				       CefRefPtr<CefValue> payload) override
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
	void
	DeserializeHttpRequestResponse(CefRefPtr<CefValue> idInput,
				       CefRefPtr<CefValue> responseInput,
				       CefRefPtr<CefValue> &output) override;

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
