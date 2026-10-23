#pragma once

#include "cef-headers.hpp"

#ifdef _WIN32
#include "deps/server/NamedPipesServer.hpp"
#endif

class SEMessageBus;

///////////////////////////////////////////////////////////////////////
//
// Listens for JSON messages over a network connection (currently
// implemented as Named Pipes server).
//
// In case the messages are properly structured, dispatches them to
// all browsers except Browser Sources as an onHostMessageReceived
// event.
//
// For the message to be properly structured, it must contain the
// following root-level properties:
//
//	version		- number. protocol version (1)
//	source		- object
//	target		- object ({ scope: "broadcast" })
//	payload		- object. the message payload.
//
// In case of a command message, the "payload" property value is
// structured as follows:
//
//	class		- string. "command"
//	command		- object. ({ name: "command-name", "data": object })
//
// The primary purpose of this class is to support integration with
// external controllers, such as Elgato Streamdeck.
//
//
// The accepting flag below replaced
// SEGlobalStateManager::IsInstanceAvailable() (CORE-757). The question the
// pipe callback has to answer is not "is the plug-in available" but "is this
// server still willing to forward" -- and that is strictly narrower: the bus
// that owns this object by value runs its own destructor body before this
// one's, so there was a window in which the singleton still looked available
// while the bus was already coming apart.
//
class SEControllerServer
{
public:
	SEControllerServer(SEMessageBus* bus);
	virtual ~SEControllerServer();

public:
	///
	// Send a message with specified payload to all connected clients
	//
	//	source		- message source type
	//	sourceAddress	- message source address
	//	payload		- message payload
	//
	void NotifyAllClients(
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload);

	///
	// Send an event message to all connected clients
	//
	//	source		- message source type
	//	sourceAddress	- message source address
	//	eventName	- event name
	//	eventData	- event data
	//
	void SendEventAllClients(
		std::string source,
		std::string sourceAddress,
		std::string eventName,
		CefRefPtr<CefValue> eventData);

	///
	// Send a generic message to all connected clients
	//
	//	source		- message source type
	//	sourceAddress	- message source address
	//	message		- message data
	//
	void SendMessageAllClients(
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> message);

private:
	void OnMsgReceivedInternal(std::string& msg);

private:
	SEMessageBus* m_bus = nullptr;

	//
	// Cleared by the destructor before the pipe server is stopped; see
	// the comment above the class.
	//
	bool m_accepting = true;
#ifdef _WIN32
	NamedPipesServer *m_server = nullptr;
#endif
};
