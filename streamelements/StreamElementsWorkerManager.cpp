#include "StreamElementsWorkerManager.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <QUuid>
#include <QWidget>

#include "cef-headers.hpp"

/* ========================================================================= */

class StreamElementsWorkerManager::StreamElementsWorker {
public:
	StreamElementsWorker(std::string id,
			     std::string url,
			     std::string executeJavascriptOnLoad)
		: m_url(url),
		  m_executeJavascriptOnLoad(executeJavascriptOnLoad)
	{
		auto browserWidget = new StreamElementsBrowserWidget(
			nullptr, StreamElementsMessageBus::DEST_WORKER,
			m_url.c_str(), m_executeJavascriptOnLoad.c_str(),
			"default", "popup",
			CreateGloballyUniqueIdString().c_str(),
			std::make_shared<StreamElementsApiMessageHandler>("backgroundWorker"),
			false);

		auto mainWindow =
			StreamElementsGlobalStateManager::GetInstance()
				->mainWindow();

		//
		// HACK: QCefWidget will only create the browser if visible
		//	 Since we do not want to show it, we create a QDockWidget and
		//	 position it out of bounds of the current desktop.
		//

#if QT_VERSION_MAJOR >= 6
		QRect rec = QApplication::primaryScreen()->geometry();
#else
		QRect rec = QApplication::desktop()->screenGeometry();
#endif

		m_dockWidget = new QDockWidget();
		m_dockWidget->setVisible(true);
		m_dockWidget->layout()->addWidget(browserWidget);
		m_dockWidget->setGeometry(QRect(rec.width() * 2, rec.height() * 2, 1920, 1080));

		mainWindow->addDockWidget(Qt::NoDockWidgetArea, m_dockWidget);

		auto timer = new QTimer();
		timer->moveToThread(qApp->thread());
		timer->setSingleShot(true);
		timer->setInterval(3000);

		QObject::connect(timer, &QTimer::timeout, [timer, this]() {
			timer->deleteLater();

			m_dockWidget->hide();
		});

		QMetaObject::invokeMethod(timer, "start",
					  Qt::QueuedConnection, Q_ARG(int, 0));
	}

	~StreamElementsWorker() {
		auto mainWindow =
			StreamElementsGlobalStateManager::GetInstance()
				->mainWindow();

		mainWindow->removeDockWidget(m_dockWidget);
		m_dockWidget->deleteLater();
	}

	std::string GetUrl() { return m_url; }
	std::string GetExecuteJavaScriptOnLoad() { return m_executeJavascriptOnLoad; }

private:
	std::string m_url;
	std::string m_executeJavascriptOnLoad;
	QDockWidget *m_dockWidget = nullptr;
};

StreamElementsWorkerManager::StreamElementsWorkerManager() {}

StreamElementsWorkerManager::~StreamElementsWorkerManager() {}

void StreamElementsWorkerManager::OnObsExit() {}

void StreamElementsWorkerManager::RemoveAll()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	while (m_items.begin() != m_items.end()) {
		Remove(m_items.begin()->first);
	}
}

std::string
StreamElementsWorkerManager::Add(std::string requestedId,
				 std::string url,
				 std::string executeJavascriptOnLoad)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	std::string id = requestedId;

	if (!id.size() || m_items.count(id)) {
		id = QUuid::createUuid().toString().toStdString();
	}

	m_items[id] = new StreamElementsWorker(id, url,
					       executeJavascriptOnLoad);

	return id;
}

void StreamElementsWorkerManager::Remove(std::string id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_items.count(id)) {
		StreamElementsWorker *item = m_items[id];

		m_items.erase(id);

		delete item;
	}
}

void StreamElementsWorkerManager::GetIdentifiers(
	std::vector<std::string> &result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto it = m_items.begin(); it != m_items.end(); ++it) {
		result.push_back(it->first);
	}
}

void StreamElementsWorkerManager::Serialize(CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefDictionaryValue> root = CefDictionaryValue::Create();
	output->SetDictionary(root);

	for (auto it = m_items.begin(); it != m_items.end(); ++it) {
		CefRefPtr<CefValue> itemValue = CefValue::Create();
		CefRefPtr<CefDictionaryValue> item =
			CefDictionaryValue::Create();
		itemValue->SetDictionary(item);

		item->SetString("id", it->first);
		item->SetString("url", it->second->GetUrl());
		item->SetString("executeJavaScriptOnLoad",
				it->second->GetExecuteJavaScriptOnLoad());

		root->SetValue(it->first, itemValue);
	}
}

void StreamElementsWorkerManager::Deserialize(CefRefPtr<CefValue> &input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!!input.get() && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

		CefDictionaryValue::KeyList keys;
		if (root->GetKeys(keys)) {
			for (auto key : keys) {
				std::string id = key.ToString();

				CefRefPtr<CefDictionaryValue> dict =
					root->GetDictionary(key);

				if (!!dict.get() &&
				    dict->HasKey("url") && dict->GetType("url") == VTYPE_STRING) {
					std::string url =
						dict->GetString("url");

					std::string executeJavaScriptOnLoad =
						"";

					if (dict->HasKey(
						    "executeJavaScriptOnLoad") &&
					    dict->GetType(
						    "executeJavaScriptOnLoad") ==
						    VTYPE_STRING) {
						executeJavaScriptOnLoad =
							dict->GetString(
								    "executeJavaScriptOnLoad")
								.ToString();
					}

					Add(id, url, executeJavaScriptOnLoad);
				}
			}
		}
	}
}

bool StreamElementsWorkerManager::SerializeOne(std::string id,
					       CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();
	output->SetDictionary(item);

	item->SetString("id", id);
	item->SetString("url", m_items[id]->GetUrl());
	item->SetString("executeJavaScriptOnLoad",
			m_items[id]->GetExecuteJavaScriptOnLoad());

	return true;
}

std::string
StreamElementsWorkerManager::DeserializeOne(CefRefPtr<CefValue> input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (input.get() && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> dict = input->GetDictionary();

		if (!!dict.get() && dict->HasKey("url") && dict->GetType("url") == VTYPE_STRING) {
			std::string id =
				dict->HasKey("id") ? dict->GetString("id") : "";
			std::string url = dict->GetString("url");

			std::string executeJavaScriptOnLoad = "";

			if (dict->HasKey("executeJavaScriptOnLoad") &&
			    dict->GetType("executeJavaScriptOnLoad") ==
				    VTYPE_STRING) {
				executeJavaScriptOnLoad =
					dict->GetString(
						    "executeJavaScriptOnLoad")
						.ToString();
			}

			return Add(id, url, executeJavaScriptOnLoad);
		}
	}

	return "";
}
