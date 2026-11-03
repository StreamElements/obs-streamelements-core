#pragma once

#include "cef-headers.hpp"

#include <string>

//
// What the host API needs from the transaction manager: open a
// deferred-save transaction, close one, and the batch path's defer-save
// bracket.
//
// Create() answers the id Complete() later names, so the two are a pair
// a page drives across separate API calls -- which is why the timeout
// exists at all.
//
class ISEObsApiTransactionManager {
public:
	virtual ~ISEObsApiTransactionManager() {}

	//
	// Opens a transaction and answers its id. The timeout bounds a page
	// that opens one and never completes it.
	//
	virtual std::string Create(int timeoutMilliseconds = 60000) = 0;

	//
	// Closes the transaction with that id.
	//
	virtual void Complete(std::string id) = 0;

	//
	// Brackets a batch in obs_frontend_defer_save_begin/end. The one piece
	// of this contract batchInvokeSeries reaches rather than a handler
	// body.
	//
	// EndDeferSave is reached differently from everything else here, and
	// it is worth knowing because it is what a surface measurement misses:
	// batchInvokeSeries hands this service to its heap-allocated context
	// -- the manager rather than the whole bundle, since a context has no
	// business in the container's refcount -- and the batch's done()
	// lambda calls EndDeferSave through that stored handle. A scan looking
	// for `services->obsApiTransactionManager->Method(` finds Begin and
	// never End, which is how this one was nearly left off the contract.
	//
	virtual void BeginDeferSave() = 0;

	virtual void EndDeferSave() = 0;
};
