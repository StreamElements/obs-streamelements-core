#pragma once

#include <QDialog>

#include "cef-headers.hpp"

#include <string>

#include "SEUtils.hpp"
#include "SEBrowserWidget.hpp"

class SEDialogApiMessageHandler;

class SEBrowserDialog: public QDialog
{
	friend SEDialogApiMessageHandler;

public:
	SEBrowserDialog(QWidget *parent, std::string url,
			std::string executeJavaScriptOnLoad, bool isIncognito,
			std::string containerType,
			std::shared_ptr<SEApiServices> apiServices);
	~SEBrowserDialog();

	std::string result() { return m_result; }

	virtual void showEvent(QShowEvent *event) override;
	virtual void hideEvent(QHideEvent *event) override;

public Q_SLOTS:
	virtual int exec() override;

public:
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

	std::string m_containerType;

private:
	void DestroyBrowser(std::string reason);

protected:
	virtual void closeEvent(QCloseEvent *e) override;
};
