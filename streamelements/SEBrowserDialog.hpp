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
	SEBrowserDialog(QWidget* parent, std::string url, std::string executeJavaScriptOnLoad, bool isIncognito, std::string containerType);
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

	std::string m_url;
	std::string m_executeJavaScriptOnLoad;

	bool m_isIncognito = false;

	std::string m_containerType;

private:
	void DestroyBrowser(std::string reason);

protected:
	virtual void closeEvent(QCloseEvent *e) override;
};
