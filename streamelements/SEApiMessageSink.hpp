#pragma once

#include "SEApiClientInfo.hpp"

#include "cef-headers.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

//
// Where a host API reply goes: the six transport operations the services
// container publishes, and nothing else.
//
// SEWebsocketApiServer implements it. The point of the interface is that
// SEWebsocketApiServer.hpp is a websocketpp server template instantiation --
// asio, openssl and zlib behind it -- and the API handler infrastructure
// needs none of that to do its job. It needs to hand four messages to
// something; this is that something, and a recording stand-in can be handed
// to it instead, which is what makes a dispatch assertable.
//
// Deliberately not the server's whole surface. DispatchTargetJSEvent,
// DispatchTargetClientMessage and the type/payload overload of
// DispatchClientMessage are reached through SEHostEventDispatcher, which
// holds the concrete server; the root hands that out. An interface grows
// when a container consumer needs a method, not in anticipation.
//
class ISEApiMessageSink {
public:
	typedef std::function<void(std::shared_ptr<SEApiClientInfo> clientInfo,
				   CefRefPtr<CefProcessMessage> msg)>
		message_handler_t;

public:
	virtual ~ISEApiMessageSink() {}

	//
	// The port pages connect back on; it goes into the bootstrap script
	// that builds window.host.
	//
	virtual uint16_t GetPort() const = 0;

	//
	// To every connected client, and to one.
	//
	virtual bool DispatchJSEvent(std::string source, std::string event,
				     std::string json) = 0;

	virtual bool
	DispatchJSEvent(std::string source,
			std::shared_ptr<SEApiClientInfo> clientInfo,
			std::string event, std::string json) = 0;

	virtual bool
	DispatchClientMessage(std::string source,
			      std::shared_ptr<SEApiClientInfo> clientInfo,
			      CefRefPtr<CefProcessMessage> msg) = 0;

	//
	// One handler per container id. A second registration for the same id
	// replaces the first.
	//
	virtual bool RegisterMessageHandler(std::string target,
					    message_handler_t handler) = 0;

	virtual bool UnregisterMessageHandler(std::string target,
					      message_handler_t handler) = 0;
};
