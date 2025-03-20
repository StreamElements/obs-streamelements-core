#include "StreamElementsBrowserDialog.hpp"
#include "StreamElementsApiMessageHandler.hpp"

#include <QVBoxLayout>

#include "cef-headers.hpp"

static std::recursive_mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name)                          \
	RegisterIncomingApiCallHandler(name, []( \
		std::shared_ptr<StreamElementsApiMessageHandler> self, \
		CefRefPtr<CefProcessMessage> message, \
		CefRefPtr<CefListValue> args, \
		CefRefPtr<CefValue>& result, \
		std::shared_ptr<StreamElementsWebsocketApiServer::ClientInfo> target, \
		const long cefClientId, \
		std::function<void()> complete_callback) \
		{ \
			(void)self; \
			(void)message; \
			(void)args; \
			(void)result; \
			(void)target; \
			(void)cefClientId; \
			(void)complete_callback; \
			std::lock_guard<std::recursive_mutex> _api_sync_guard(s_sync_api_call_mutex);
#define API_HANDLER_END()    \
	complete_callback(); \
	});

class StreamElementsDialogApiMessageHandler : public StreamElementsApiMessageHandler
{
public:
	StreamElementsDialogApiMessageHandler(
		StreamElementsBrowserDialog *dialog, std::string containerType)
		: StreamElementsApiMessageHandler(containerType),
		  m_dialog(dialog)
	{
		// RegisterIncomingApiCallHandlers();
	}

	/*
	virtual std::shared_ptr<StreamElementsApiMessageHandler> Clone() override
	{
		return std::make_shared<StreamElementsDialogApiMessageHandler>(m_dialog, m_containerType);
	}
	*/

private:
	StreamElementsBrowserDialog* m_dialog;

protected:
	StreamElementsBrowserDialog* dialog() { return m_dialog; }

	virtual void RegisterIncomingApiCallHandlers() override
	{
		StreamElementsApiMessageHandler::
			RegisterIncomingApiCallHandlers();

		API_HANDLER_BEGIN("endModalDialog");
		{
			result->SetBool(false);

			if (args->GetSize()) {
				std::shared_ptr<StreamElementsDialogApiMessageHandler> msgHandler =
					std::static_pointer_cast<StreamElementsDialogApiMessageHandler>(self);

				CefRefPtr<CefValue> arg = args->GetValue(0);

				msgHandler
					->Shutdown(); // Stop processing any queued messages

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

		API_HANDLER_BEGIN("endNonModalDialog");
		{
			result->SetBool(false);

			if (args->GetSize()) {
				std::shared_ptr<
					StreamElementsDialogApiMessageHandler>
					msgHandler = std::static_pointer_cast<
						StreamElementsDialogApiMessageHandler>(
						self);

				CefRefPtr<CefValue> arg = args->GetValue(0);

				msgHandler
					->Shutdown(); // Stop processing any queued messages

				msgHandler->dialog()->m_result = "null";

				if (arg->GetType() != VTYPE_NULL) {
					CefString json = CefWriteJSON(
						arg, JSON_WRITER_DEFAULT);

					if (json.size() &&
					    json.ToString().c_str()) {
						msgHandler->dialog()->m_result =
							json.ToString().c_str();
					}
				}

				QMetaObject::invokeMethod(msgHandler->dialog(),
							  "accept",
							  Qt::QueuedConnection);
			}
		}
		API_HANDLER_END();

		API_HANDLER_BEGIN("endDialog");
		{
			result->SetBool(false);

			if (args->GetSize()) {
				std::shared_ptr<
					StreamElementsDialogApiMessageHandler>
					msgHandler = std::static_pointer_cast<
						StreamElementsDialogApiMessageHandler>(
						self);

				CefRefPtr<CefValue> arg = args->GetValue(0);

				msgHandler->dialog()->m_result = "null";

				msgHandler
					->Shutdown(); // Stop processing any queued messages

				if (arg->GetType() != VTYPE_NULL) {
					CefString json = CefWriteJSON(
						arg, JSON_WRITER_DEFAULT);

					if (json.size() &&
					    json.ToString().c_str()) {
						msgHandler->dialog()->m_result =
							json.ToString().c_str();
					}
				}

				QMetaObject::invokeMethod(msgHandler->dialog(),
							  "accept",
							  Qt::QueuedConnection);
			}
		}
		API_HANDLER_END();

	}
};

StreamElementsBrowserDialog::StreamElementsBrowserDialog(
	QWidget *parent, std::string url, std::string executeJavaScriptOnLoad,
	bool isIncognito, std::string containerType)
	: QDialog(parent),
	  m_url(url),
	  m_executeJavaScriptOnLoad(executeJavaScriptOnLoad),
	  m_isIncognito(isIncognito),
	  m_containerType(containerType)
{
	setContentsMargins(0, 0, 0, 0);
	setLayout(new QVBoxLayout());
	layout()->setContentsMargins(0, 0, 0, 0);

	this->setWindowFlags((
		(windowFlags() | Qt::CustomizeWindowHint)
		& ~Qt::WindowContextHelpButtonHint
		));

	if (!!parent && IsAlwaysOnTop(parent)) {
		SetAlwaysOnTop(this, true);
	}

	connect(this, &QDialog::finished, [this](int result) { DestroyBrowser("finished"); });
	connect(this, &QDialog::accepted, [this]() { DestroyBrowser("accepted"); });
	connect(this, &QDialog::rejected, [this]() { DestroyBrowser("rejected"); });
}

StreamElementsBrowserDialog::~StreamElementsBrowserDialog()
{
}

void StreamElementsBrowserDialog::closeEvent(QCloseEvent* e)
{
	DestroyBrowser("closeEvent");

	QDialog::closeEvent(e);
}

void StreamElementsBrowserDialog::DestroyBrowser(std::string reason)
{
	if (!m_browser)
		return;

	blog(LOG_INFO,
	     "[obs-streamelements-core]: StreamElementsBrowserDialog::DestroyBrowser('%s'): %s",
	     reason.c_str(), m_url.c_str());

	layout()->removeWidget(m_browser);

	m_browser->DestroyBrowser();
	m_browser->deleteLater();

	m_browser = nullptr;
}

void StreamElementsBrowserDialog::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);

	if (m_browser)
		return;

	m_browser = new StreamElementsBrowserWidget(
		nullptr, StreamElementsMessageBus::DEST_UI, m_url.c_str(),
		m_executeJavaScriptOnLoad.c_str(), "reload", "dialog",
		CreateGloballyUniqueIdString().c_str(),
		std::make_shared<StreamElementsDialogApiMessageHandler>(
			this, m_containerType),
		m_isIncognito);

	layout()->addWidget(m_browser);
}

void StreamElementsBrowserDialog::hideEvent(QHideEvent* event)
{
	DestroyBrowser("hideEvent");

	QDialog::hideEvent(event);
}

int StreamElementsBrowserDialog::exec()
{
	int result = QDialog::exec();

	DestroyBrowser("exec finished");

	return result;
}
