#include "StreamElementsBrowserDialog.hpp"
#include "StreamElementsApiMessageHandler.hpp"

#include <QVBoxLayout>

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

static std::recursive_mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name)                          \
	RegisterIncomingApiCallHandler(name, []( \
		StreamElementsApiMessageHandler* self, \
		CefRefPtr<CefProcessMessage> message, \
		CefRefPtr<CefListValue> args, \
		CefRefPtr<CefValue>& result, \
		CefRefPtr<CefBrowser> browser, \
		const long cefClientId, \
		std::function<void()> complete_callback) \
		{ \
			(void)self; \
			(void)message; \
			(void)args; \
			(void)result; \
			(void)browser; \
			(void)cefClientId; \
			(void)complete_callback; \
			std::lock_guard<std::recursive_mutex> _api_sync_guard(s_sync_api_call_mutex);
#define API_HANDLER_END()    \
	complete_callback(); \
	});

class StreamElementsDialogApiMessageHandler : public StreamElementsApiMessageHandler
{
public:
	StreamElementsDialogApiMessageHandler(StreamElementsBrowserDialog* dialog) :
		m_dialog(dialog)
	{
		RegisterIncomingApiCallHandlers();
	}

private:
	StreamElementsBrowserDialog* m_dialog;

protected:
	StreamElementsBrowserDialog* dialog() { return m_dialog; }

	virtual void RegisterIncomingApiCallHandlers() override
	{
		API_HANDLER_BEGIN("endModalDialog");
		{
			result->SetBool(false);

			if (args->GetSize()) {
				StreamElementsDialogApiMessageHandler* msgHandler =
					static_cast<StreamElementsDialogApiMessageHandler*>(self);

				CefRefPtr<CefValue> arg = args->GetValue(0);

				msgHandler->dialog()->m_result = "null";

				if (arg->GetType() != VTYPE_NULL) {
					CefString json = CefWriteJSON(arg, JSON_WRITER_DEFAULT);

					if (json.size() && json.ToString().c_str()) {
						msgHandler->dialog()->m_result = json.ToString().c_str();
					}
				}

				QMetaObject::invokeMethod(
					msgHandler->dialog(),
							  "accept",
							  Qt::QueuedConnection);
			}
		}
		API_HANDLER_END();
	}
};

StreamElementsBrowserDialog::StreamElementsBrowserDialog(QWidget* parent, std::string url, std::string executeJavaScriptOnLoad, bool isIncognito)
	: QDialog(parent), m_url(url), m_executeJavaScriptOnLoad(executeJavaScriptOnLoad), m_isIncognito(isIncognito)
{
	setLayout(new QVBoxLayout());

	this->setWindowFlags((
		(windowFlags() | Qt::CustomizeWindowHint)
		& ~Qt::WindowContextHelpButtonHint
		));

	if (!!parent && IsAlwaysOnTop(parent)) {
		SetAlwaysOnTop(this, true);
	}
}

StreamElementsBrowserDialog::~StreamElementsBrowserDialog()
{
}

int StreamElementsBrowserDialog::exec()
{
	m_browser = new StreamElementsBrowserWidget(
		nullptr,
		m_url.c_str(),
		m_executeJavaScriptOnLoad.c_str(),
		"reload",
		"dialog",
		"",
		new StreamElementsDialogApiMessageHandler(this),
		m_isIncognito);

	layout()->addWidget(m_browser);

	int result = QDialog::exec();

	m_browser->deleteLater();

	return result;
}
