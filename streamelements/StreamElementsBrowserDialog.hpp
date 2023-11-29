#pragma once

#include <QDialog>

#include "cef-headers.hpp"

#include <string>

#include "StreamElementsUtils.hpp"
#include "StreamElementsBrowserWidget.hpp"

class StreamElementsDialogApiMessageHandler;

class StreamElementsBrowserDialog: public QDialog
{
	friend StreamElementsDialogApiMessageHandler;

public:
	StreamElementsBrowserDialog(QWidget* parent, std::string url, std::string executeJavaScriptOnLoad, bool isIncognito);
	~StreamElementsBrowserDialog();

	std::string result() { return m_result; }

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
	StreamElementsBrowserWidget* m_browser = nullptr;
	std::string m_result;

	std::string m_url;
	std::string m_executeJavaScriptOnLoad;

	bool m_isIncognito = false;
};
