#pragma once

//
// The host API surface: the 208 window.host.* methods, registered in one
// function in SEApiMessageHandler.cpp.
//
// Keeping them in a single file is deliberate -- that file is what the host
// exposes, and reading it is how you find out. The infrastructure they run on
// is in SEApiMessageHandlerBase, which is also what a test subclasses.
//

#include "SEApiMessageHandlerBase.hpp"

class SEApiMessageHandler : public SEApiMessageHandlerBase {
public:
	SEApiMessageHandler(std::string containerType,
			    std::shared_ptr<SEApiServices> services)
		: SEApiMessageHandlerBase(containerType, services)
	{
	}

	virtual ~SEApiMessageHandler() {}

protected:
	//
	// All 208 of them. Defined in SEApiMessageHandler.cpp, which is the
	// only place a host API method is registered.
	//
	virtual void RegisterIncomingApiCallHandlers() override;

public:
	class InvokeHandler;
};

class SEApiMessageHandler::InvokeHandler
	: public SEApiMessageHandler {
public:
	InvokeHandler(std::string containerType,
		      std::shared_ptr<SEApiServices> services)
		: SEApiMessageHandler(containerType, services)
	{
		// We must init this here since Invoke handler does not get "context initialized" event from
		// the browser script which does not exist here :)
		RegisterIncomingApiCallHandlers();
	}

public:
	~InvokeHandler() {}

	//
	// Owned by SEAuxiliaryControlsBuilder, which is the only thing that
	// invokes an auxiliary action. It was a singleton for as long as its
	// caller was a free function with nothing to hand it: a file-static
	// shared_ptr, never released, which kept the services bundle's
	// refcount off zero even with no widget alive.
	//
	bool
	InvokeApiCallAsync(std::string invoke, CefRefPtr<CefListValue> args,
			   std::function<void(CefRefPtr<CefValue>)> callback);
};
