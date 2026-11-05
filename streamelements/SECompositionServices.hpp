#pragma once

#include <memory>

class SEHostEventDispatcher;
class SEMessageBus;
class SEVideoCompositionManager;

//
// What a composition needs in order to announce itself.
//
// One parameter rather than three, because it travels a long way: through
// the composition base, two derived classes and three factories, all of
// which only pass it along. Grouping it keeps that threading to a single
// argument and means the next thing a composition has to announce with does
// not widen six signatures again.
//
// Compositions are created by, and owned by, their manager -- so this is
// filled in by the manager, which holds the first two by injection.
//
struct SECompositionServices {
	//
	// Host events to pages, and to external controllers. Every
	// composition announces scene changes, scene lists and its own
	// reconfiguration to both.
	//
	std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher;
	std::shared_ptr<SEMessageBus> messageBus;

	//
	// Weak, and only a custom video composition uses it: it builds its
	// own SESignalHandlerData, which carries the lookup service the OBS
	// signal callbacks need. Weak because the manager owns the
	// composition -- a strong handle here is the manager keeping itself
	// alive, which is the one ring CORE-757 names in this layer.
	//
	// Empty for the native composition, which the manager builds inside
	// its own constructor, where there is no shared_ptr to itself yet --
	// and which does not install those callbacks anyway.
	//
	std::weak_ptr<SEVideoCompositionManager> videoCompositionManager;
};
