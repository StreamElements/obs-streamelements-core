#include "SEBrowserDialog.hpp"
#include "SEApiMessageHandler.hpp"
#include "SEGlobalStateManager.hpp"

#include <QVBoxLayout>

#include "cef-headers.hpp"

static std::recursive_mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name)                          \
	RegisterIncomingApiCallHandler(name, []( \
		std::shared_ptr<SEApiMessageHandler> self, \
		CefRefPtr<CefProcessMessage> message, \
		CefRefPtr<CefListValue> args, \
		CefRefPtr<CefValue>& result, \
		std::shared_ptr<SEWebsocketApiServer::ClientInfo> target, \
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

class SEDialogApiMessageHandler : public SEApiMessageHandler
{
public:
	SEDialogApiMessageHandler(
		SEBrowserDialog *dialog, std::string containerType)
		: SEApiMessageHandler(containerType),
		  m_dialog(dialog)
	{
		// RegisterIncomingApiCallHandlers();
	}

	/*
	virtual std::shared_ptr<SEApiMessageHandler> Clone() override
	{
		return std::make_shared<SEDialogApiMessageHandler>(m_dialog, m_containerType);
	}
	*/

private:
	SEBrowserDialog* m_dialog;

protected:
	SEBrowserDialog* dialog() { return m_dialog; }

	virtual void RegisterIncomingApiCallHandlers() override
	{
		SEApiMessageHandler::
			RegisterIncomingApiCallHandlers();

		API_HANDLER_BEGIN("endModalDialog");
		{
			result->SetBool(false);

			if (args->GetSize()) {
				std::shared_ptr<SEDialogApiMessageHandler> msgHandler =
					std::static_pointer_cast<SEDialogApiMessageHandler>(self);

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
					SEDialogApiMessageHandler>
					msgHandler = std::static_pointer_cast<
						SEDialogApiMessageHandler>(
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
					SEDialogApiMessageHandler>
					msgHandler = std::static_pointer_cast<
						SEDialogApiMessageHandler>(
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

SEBrowserDialog::SEBrowserDialog(
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

SEBrowserDialog::~SEBrowserDialog()
{
}

void SEBrowserDialog::closeEvent(QCloseEvent* e)
{
	DestroyBrowser("closeEvent");

	QDialog::closeEvent(e);
}

void SEBrowserDialog::DestroyBrowser(std::string reason)
{
	if (!m_browser)
		return;

	blog(LOG_INFO,
	     "[obs-streamelements-core]: SEBrowserDialog::DestroyBrowser('%s'): %s",
	     reason.c_str(), m_url.c_str());

	layout()->removeWidget(m_browser);

	m_browser->DestroyBrowser();

	if (SEGlobalStateManager::IsInstanceAvailable()) {
		m_browser->deleteLater();
	} else {
		delete m_browser;
	}

	m_browser = nullptr;
}

void SEBrowserDialog::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);

	if (m_browser)
		return;

	m_browser = new SEBrowserWidget(
		nullptr, SEMessageBus::DEST_UI, m_url.c_str(),
		m_executeJavaScriptOnLoad.c_str(), "reload", "dialog",
		CreateGloballyUniqueIdString().c_str(),
		std::make_shared<SEDialogApiMessageHandler>(
			this, m_containerType),
		m_isIncognito);

	layout()->addWidget(m_browser);
}

void SEBrowserDialog::hideEvent(QHideEvent* event)
{
	DestroyBrowser("hideEvent");

	QDialog::hideEvent(event);
}

int SEBrowserDialog::exec()
{
	int result = QDialog::exec();

	DestroyBrowser("exec finished");

	return result;
}
