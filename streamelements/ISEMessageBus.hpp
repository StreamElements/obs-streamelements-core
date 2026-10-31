#pragma once

#include "cef-headers.hpp"

#include <cstdint>
#include <string>

//
// What the host API needs from the message bus, together with the vocabulary
// that goes with it.
//
// The destination flags and the source names are on the contract rather than
// on the implementation because they are part of what a caller says: "deliver
// this to all local listeners" names a destination on the bus, not a detail
// of how one bus happens to be built. SEMessageBus inherits them, so the
// seventeen sites outside this header that spell SEMessageBus::DEST_UI keep
// resolving unchanged.
//
// They are constexpr rather than const with a definition in the .cpp, which
// is what they were: a static constexpr data member is implicitly inline in
// C++17, so no translation unit has to carry a definition -- and a test that
// links a fake bus would otherwise have to supply all ten of them itself.
//
class ISEMessageBus {
public:
	virtual ~ISEMessageBus() {}

public:
	typedef uint32_t message_destination_filter_flags_t;

	//
	// Message destination type flags. The two ALL_ masks are ranges, not
	// unions of the flags below them: a destination added later falls into
	// its half without anything here changing.
	//
	static constexpr message_destination_filter_flags_t DEST_ALL =
		0xFFFFFFFFUL;

	// All local destinations
	static constexpr message_destination_filter_flags_t DEST_ALL_LOCAL =
		0x0000FFFFUL;
	static constexpr message_destination_filter_flags_t DEST_UI =
		0x00000001UL;
	static constexpr message_destination_filter_flags_t DEST_WORKER =
		0x00000002UL;
	static constexpr message_destination_filter_flags_t DEST_BROWSER_SOURCE =
		0x00000004UL;

	// All external destinations
	static constexpr message_destination_filter_flags_t DEST_ALL_EXTERNAL =
		0xFFFF0000UL;
	static constexpr message_destination_filter_flags_t
		DEST_EXTERNAL_CONTROLLER = 0x00010000UL;

	// Message sources
	static constexpr const char *const SOURCE_APPLICATION = "application";
	static constexpr const char *const SOURCE_WEB = "web";
	static constexpr const char *const SOURCE_EXTERNAL = "external";

public:
	//
	// Registers a destination under a target address. Called by
	// SEBrowserWidget as each browser comes up, rather than by a handler:
	// the widget knows which kind of destination it is.
	//
	virtual void AddListener(std::string target,
				 message_destination_filter_flags_t type) = 0;

public:
	//
	// The per-listener HTTP servers a page can ask the bus to run on its
	// behalf, keyed by the listener's target address.
	//
	virtual void
	DeserializeBrowserHttpServer(std::string target,
				     CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) = 0;

	virtual void
	SerializeBrowserHttpServers(std::string target,
				    CefRefPtr<CefValue> &output) = 0;

	virtual void
	RemoveBrowserHttpServersByIds(std::string target,
				      CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) = 0;

	//
	// Answers a request one of those servers parked, by its id.
	//
	virtual void
	DeserializeHttpRequestResponse(CefRefPtr<CefValue> idInput,
				       CefRefPtr<CefValue> responseInput,
				       CefRefPtr<CefValue> &output) = 0;

public:
	//
	// Three fan-outs, and which one a caller wants is the whole of the
	// difference: local listeners only, external controllers only, or
	// everything including browser sources. Only the third interprets
	// system commands on the way through.
	//
	virtual void NotifyAllLocalEventListeners(
		message_destination_filter_flags_t types, std::string source,
		std::string sourceAddress, std::string event,
		CefRefPtr<CefValue> payload) = 0;

	virtual void NotifyAllExternalEventListeners(
		message_destination_filter_flags_t types, std::string source,
		std::string sourceAddress, std::string event,
		CefRefPtr<CefValue> payload) = 0;

	virtual void
	NotifyAllMessageListeners(message_destination_filter_flags_t types,
				  std::string source, std::string sourceAddress,
				  CefRefPtr<CefValue> payload) = 0;
};
