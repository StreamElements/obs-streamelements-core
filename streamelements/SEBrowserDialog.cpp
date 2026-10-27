#include "SEBrowserDialog.hpp"
#include "SEDialogApiMessageHandler.hpp"
#include "SEGlobalStateManager.hpp"

#include <QVBoxLayout>

#include "cef-headers.hpp"


SEBrowserDialog::SEBrowserDialog(QWidget *parent, std::string url,
				 std::string executeJavaScriptOnLoad,
				 bool isIncognito, std::string containerType,
				 std::shared_ptr<SEApiServices> apiServices)
	: QDialog(parent),
	  m_apiServices(apiServices),
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

void SEBrowserDialog::SetResultJson(std::string json)
{
	m_result = json;
}

void SEBrowserDialog::Accept()
{
	//
	// Queued, not direct. The API call arrives on a thread that is not
	// Qt's, and accept() runs the finished/accepted handlers that destroy
	// the browser widget -- which is the widget whose page is still
	// inside the handler making this call.
	//
	QMetaObject::invokeMethod(this, "accept", Qt::QueuedConnection);
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

	if (m_ownerTeardown) {
		//
		// No event loop is coming back to run a deferred delete; see
		// NoteOwnerTeardown().
		//
		delete m_browser;
	} else {
		m_browser->deleteLater();
	}

	m_browser = nullptr;
}

void SEBrowserDialog::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);

	if (m_browser)
		return;

	auto apiServices = m_apiServices.lock();

	if (!apiServices) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: SEBrowserDialog: showing without a browser: the services bundle is gone");

		return;
	}

	m_browser = new SEBrowserWidget(
		nullptr, SEMessageBus::DEST_UI, m_url.c_str(),
		m_executeJavaScriptOnLoad.c_str(), "reload", "dialog",
		CreateGloballyUniqueIdString().c_str(), apiServices,
		std::make_shared<SEDialogApiMessageHandler>(
			this, m_containerType, apiServices),
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
