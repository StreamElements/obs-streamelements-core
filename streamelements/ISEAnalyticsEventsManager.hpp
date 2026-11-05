#pragma once

#include "json11/json11.hpp"

//
// What a container consumer needs from the analytics events manager: one
// method, and only SEAuxiliaryControlsBuilder calls it -- the docks menu
// records a show/hide click.
//
// This one is NOT reached by any handler body, which is why it was missing
// from the set of fifteen interfaces that preceded it: that set was derived
// from what SEApiMessageHandler.cpp links against, and the handler never
// touches this service. A measurement that reads only the handler cannot
// see a service the container exposes to somebody else.
//
// Everything else the manager does -- the queue, the worker thread, the
// identity it hands SEHostInfoProvider, AddRawEvent and the flush on
// shutdown -- stays off this contract. The root and SEHostInfoProvider hold
// it concretely, which is where that surface belongs.
//
class ISEAnalyticsEventsManager {
public:
	virtual ~ISEAnalyticsEventsManager() {}

	//
	// Queues one analytics event. Does nothing when eventName is null.
	//
	// The defaults are declared here and on the implementation both,
	// because this is called with one argument and with three, through
	// this contract and through a concrete handle respectively.
	// check_contract_default_arguments_agree_with_their_overrides keeps
	// the two copies in step -- they are bound from the static type at
	// the call site, so two different defaults would mean the same call
	// did two different things depending on the handle it went through.
	//
	virtual void
	trackEvent(const char *eventName,
		   json11::Json::object props = json11::Json::object{},
		   json11::Json::array fields = json11::Json::array{}) = 0;
};
