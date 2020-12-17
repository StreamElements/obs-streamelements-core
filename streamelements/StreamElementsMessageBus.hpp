#pragma once

#include <mutex>
#include <list>
#include <map>

#include "cef-headers.hpp"

#include "StreamElementsControllerServer.hpp"
#include "StreamElementsHttpServerManager.hpp"

#include <util/threading.h>

// Message bus for exchanging messages between:
//
// * Background Workers
// * CEF Dialogs
// * CEF Docking Widgets
// * Browser Sources
//
// Access singleton instance with StreamElementsMessageBus::GetInstance()
//
class StreamElementsMessageBus
{
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

protected:
	StreamElementsMessageBus();
	virtual ~StreamElementsMessageBus();

public:
	static StreamElementsMessageBus* GetInstance();

public:
	void AddBrowserListener(CefRefPtr<CefBrowser> browser, message_destination_filter_flags_t type);
	void RemoveBrowserListener(CefRefPtr<CefBrowser> browser);

public:
	void DeserializeBrowserHttpServer(CefRefPtr<CefBrowser> browser,
					  CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output);

	void SerializeBrowserHttpServers(CefRefPtr<CefBrowser> browser,
					  CefRefPtr<CefValue> &output);

	void RemoveBrowserHttpServersByIds(CefRefPtr<CefBrowser> browser,
					   CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

public:
	// Deliver event message to specific browser
	//
	void NotifyBrowserEventListener(CefRefPtr<CefBrowser> browser,
					std::string scope,
					std::string source,
					std::string sourceAddress,
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
			os_event_init(&event, OS_EVENT_TYPE_MANUAL);
		}
		~WaitingHttpRequestState()
		{
			os_event_destroy(event);
		}

	public:
		os_event_t *event = nullptr;
		const HttpServer::request_t *request;
		HttpServer::response_t *response;
	};

public:
	void DeserializeHttpRequestResponse(CefRefPtr<CefValue> idInput,
					    CefRefPtr<CefValue> responseInput,
					    CefRefPtr<CefValue> &output);

private:
	std::recursive_mutex m_browser_list_mutex;
	std::map<CefRefPtr<CefBrowser>, message_destination_filter_flags_t> m_browser_list;
	std::map<int,
		 std::shared_ptr<StreamElementsHttpServerManager>>
		m_browser_http_servers;
	std::recursive_mutex m_waiting_http_requests_mutex;
	std::map<std::string, std::shared_ptr<WaitingHttpRequestState>>
		m_waiting_http_requests;
	StreamElementsControllerServer m_external_controller_server;

private:
	static StreamElementsMessageBus* s_instance;
};
