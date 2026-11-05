#pragma once

//
// The host API as a page inside an SE.Live dialog sees it: all 208 methods,
// plus the three that close the dialog it is in.
//
// It was a class defined inside SEBrowserDialog.cpp, which put three host API
// registrations in a QDialog translation unit where nothing looking for host
// API methods would find them. One class per file, and the three bodies are
// one function in SEDialogApiCall.cpp now, so what they do is testable
// without a dialog.
//

#include "SEApiMessageHandler.hpp"
#include "SEDialogApiCall.hpp"

#include <memory>
#include <string>

class SEDialogApiMessageHandler : public SEApiMessageHandler {
public:
	SEDialogApiMessageHandler(ISEDialogHost *dialog,
				  std::string containerType,
				  std::shared_ptr<SEApiServices> services)
		: SEApiMessageHandler(containerType, services), m_dialog(dialog)
	{
	}

	virtual ~SEDialogApiMessageHandler() {}

	//
	// Public, not protected: a registered body is a capture-less lambda,
	// so it reaches the handler through `self`, which is a base pointer.
	// Protected members are unreachable that way even from a derived
	// member function.
	//
	ISEDialogHost *DialogHost() { return m_dialog; }

protected:
	//
	// The base's 208, then the three below.
	//
	virtual void RegisterIncomingApiCallHandlers() override;

private:
	ISEDialogHost *m_dialog;
};
