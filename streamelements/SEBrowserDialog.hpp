#pragma once

#include <QDialog>

#include "cef-headers.hpp"

#include <string>

#include "SEUtils.hpp"
#include "SEBrowserWidget.hpp"
#include "SEDialogApiCall.hpp"

//
// ISEDialogHost is how the dialog's own three host API methods reach it --
// SetResultJson and Accept, and nothing else. It replaces a friendship with
// SEDialogApiMessageHandler, which let those three bodies write m_result
// directly from a class defined in this file's .cpp.
//
class SEBrowserDialog : public QDialog, public ISEDialogHost {
public:
	SEBrowserDialog(QWidget *parent, std::string url,
			std::string executeJavaScriptOnLoad, bool isIncognito,
			std::string containerType,
			std::shared_ptr<SEApiServices> apiServices);
	~SEBrowserDialog();

	std::string result() { return m_result; }

	virtual void SetResultJson(std::string json) override;
	virtual void Accept() override;

	virtual void showEvent(QShowEvent *event) override;
	virtual void hideEvent(QHideEvent *event) override;

public Q_SLOTS:
	virtual int exec() override;

public:
	//
	// Called by SEBrowserDialogManager's destructor before it rejects
	// what is still open. DestroyBrowser() then deletes the browser
	// widget outright instead of deferring it: the event loop that would
	// run a deleteLater() is not coming back, so a deferred delete is a
	// leak. Everywhere else -- a user closing the dialog, hideEvent --
	// the deferral is what keeps us from deleting a widget from inside
	// its own event handler.
	//
	// This used to be SEGlobalStateManager::IsInstanceAvailable(), which
	// happened to answer it: the instance is released before the
	// container that owns the dialog manager is destroyed (CORE-757).
	//
	void NoteOwnerTeardown() { m_ownerTeardown = true; }

	std::string getUrl() { return m_url; }
	bool getIsIncognito() { return m_isIncognito; }
	std::string getExecuteJavaScriptOnLoad()
	{
		return m_executeJavaScriptOnLoad;
	}

private:
	SEBrowserWidget* m_browser = nullptr;
	std::string m_result;

	//
	// Weak. Everything this class builds from the bundle -- browser
	// widgets and API message handlers -- holds a strong reference to it,
	// and this class destroys them, so a strong reference here closes a
	// ring the refcount can never unwind. See the comment in
	// SEApiServices.hpp.
	//
	std::weak_ptr<SEApiServices> m_apiServices;

	std::string m_url;
	std::string m_executeJavaScriptOnLoad;

	bool m_isIncognito = false;
	bool m_ownerTeardown = false;

	std::string m_containerType;

private:
	void DestroyBrowser(std::string reason);

protected:
	virtual void closeEvent(QCloseEvent *e) override;
};
