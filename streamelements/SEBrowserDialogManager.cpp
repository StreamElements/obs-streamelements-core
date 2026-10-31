#include "SEBrowserDialogManager.hpp"

#include "SEUtils.hpp"
#include "SEApiServices.hpp"
#include "SEBrowserDialog.hpp"

#include <QMainWindow>
#include <QMetaObject>

#include <obs-module.h>

SEBrowserDialogManager::SEBrowserDialogManager(
	QMainWindow *mainWindow, std::shared_ptr<SEApiServices> apiServices)
	: m_mainWindow(mainWindow), m_apiServices(apiServices)
{
}

SEBrowserDialogManager::~SEBrowserDialogManager()
{
	std::map<std::string, NonModalDialog> dialogs;

	{
		std::lock_guard<decltype(m_mutex)> guard(m_mutex);

		//
		// Disarm, then take the registry away, and keep the two
		// adjacent: reject() below turns Qt's event loop, so a
		// finished handler can run inside this destructor. With
		// *m_alive false it returns immediately instead of erasing
		// from a map that is being destroyed and calling set_value on
		// a promise this loop has already satisfied -- which throws
		// std::future_error.
		//
		*m_alive = false;

		dialogs.swap(m_nonModalDialogs);
	}

	if (!dialogs.empty()) {
		//
		// Logged because it is otherwise unobservable: by the time
		// this runs the pages are gone, so nothing can report that
		// the promise was released -- and a dialog whose browser Qt
		// already tore down (it is parented to the main window) looks
		// identical in the log to one this destructor handled.
		//
		blog(LOG_INFO,
		     "obs-streamelements-core: SEBrowserDialogManager: closing %d non-modal dialog(s) still open at teardown",
		     static_cast<int>(dialogs.size()));
	}

	for (auto &pair : dialogs) {
		auto &entry = pair.second;

		if (!entry.dialog)
			continue;

		//
		// reject() runs the dialog's own rejected/finished
		// connections, which call DestroyBrowser() -- shutting the
		// dialog's API message handler and its CEF browser down while
		// the widget tree is still intact.
		//
		//
		// Before reject(), which runs DestroyBrowser(): from here on
		// the dialog deletes its browser widget outright, because the
		// event loop that would run a deleteLater() is not coming
		// back.
		//
		entry.dialog->NoteOwnerTeardown();

		entry.dialog->reject();

		//
		// Release whoever is parked on the future. Without this, an
		// API call that opened a dialog still open at shutdown left a
		// detached thread waiting on it for the life of the process.
		//
		if (entry.promise) {
			CefRefPtr<CefValue> output = CefValue::Create();

			output->SetNull();

			entry.promise->set_value(output);
		}

		//
		// After reject() has returned, never from inside it: the same
		// trade ~SEBrowserWidgetManager makes for docks (CORE-786).
		//
		delete entry.dialog;
	}

	//
	// Modal dialogs need no handling: exec() is on the stack for as long
	// as one is open, and a modal SE.Live window disables the main
	// window, which makes OBS unclosable -- so one cannot be open when
	// this runs.
	//
}

bool SEBrowserDialogManager::DeserializeModalDialog(CefRefPtr<CefValue> input,
						    CefRefPtr<CefValue> &output)
{
	bool result = false;

	output->SetNull();

	//
	// `input` is whatever a page passed to window.host.showModalDialog().
	// Every typed getter on the CefValue model is
	// `if (m_type != VTYPE_X) throw;` -- a bare throw outside a catch,
	// so std::terminate rather than an exception. Each read below is
	// therefore type-checked first, members included: HasKey() is not a
	// type check, because GetString(k) is `m_map[k]->GetString()` and
	// lands on exactly the same getter.
	//
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("url") && d->GetType("url") == VTYPE_STRING) {
		auto apiServices = m_apiServices.lock();

		if (!apiServices) {
			blog(LOG_WARNING,
			     "obs-streamelements-core: SEBrowserDialogManager: not opening a modal dialog: the services bundle is gone");

			return false;
		}

		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;

		if (d->HasKey("executeJavaScriptOnLoad") &&
		    d->GetType("executeJavaScriptOnLoad") == VTYPE_STRING) {
			executeJavaScriptOnLoad =
				d->GetString("executeJavaScriptOnLoad");
		}

		int width = 800;
		int height = 600;
		bool isIncognito = false;

		if (d->HasKey("width") && d->GetType("width") == VTYPE_INT) {
			width = d->GetInt("width");
		}

		if (d->HasKey("height") && d->GetType("height") == VTYPE_INT) {
			height = d->GetInt("height");
		}

		if (d->HasKey("incognitoMode") &&
		    d->GetType("incognitoMode") == VTYPE_BOOL &&
		    d->GetBool("incognitoMode")) {
			isIncognito = true;
		}

		auto dialog = new SEBrowserDialog(m_mainWindow, url,
						  executeJavaScriptOnLoad,
						  isIncognito, "modalDialog",
						  apiServices);

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			dialog->setWindowTitle(QString(
				d->GetString("title").ToString().c_str()));
		}

		if (d->HasKey("isResizable") &&
		    d->GetType("isResizable") == VTYPE_BOOL &&
		    d->GetBool("isResizable")) {
			auto savedMaxSize = dialog->maximumSize();

			dialog->setFixedSize(width, height);

			SEDrainEventQueue();

			dialog->setMinimumSize(width, height);
			dialog->setMaximumSize(savedMaxSize);
		} else {
			dialog->setFixedSize(width, height);
		}

		if (dialog->exec() == QDialog::Accepted) {
			output =
				CefParseJSON(dialog->result(),
					     JSON_PARSER_ALLOW_TRAILING_COMMAS);

			result = true;
		}

		dialog->deleteLater();
	}

	return result;
}

bool SEBrowserDialogManager::HasNonModalDialog(const char *id)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	return m_nonModalDialogs.count(id) > 0;
}

std::string SEBrowserDialogManager::GetNonModalDialogUrl(const char *id)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_nonModalDialogs.count(id) > 0) {
		return m_nonModalDialogs[id].dialog->getUrl();
	}

	return "";
}

void SEBrowserDialogManager::SerializeAllNonModalDialogs(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefDictionaryValue> result = CefDictionaryValue::Create();

	for (auto pair : m_nonModalDialogs) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		auto dialog = pair.second.dialog;

		d->SetString("id", pair.first);
		d->SetString("url", dialog->getUrl());
		d->SetString("executeJavaScriptOnLoad",
			     dialog->getExecuteJavaScriptOnLoad());
		d->SetBool("isIncognito", dialog->getIsIncognito());
		d->SetString("title", dialog->windowTitle().toStdString());
		d->SetInt("width", dialog->width());
		d->SetInt("height", dialog->height());

		result->SetDictionary(pair.first, d);
	}

	output->SetDictionary(result);
}

bool SEBrowserDialogManager::DeserializeCloseNonModalDialogsByIds(
	CefRefPtr<CefValue> input)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (input->GetType() != VTYPE_LIST)
		return false;

	auto list = input->GetList();

	for (size_t i = 0; i < list->GetSize(); ++i) {
		if (list->GetType(i) != VTYPE_STRING)
			return false;
	}

	for (size_t i = 0; i < list->GetSize(); ++i) {
		std::string id = list->GetString(i);

		if (!m_nonModalDialogs.count(id))
			continue;

		auto dialog = m_nonModalDialogs[id].dialog;

		// Schedule dialog reject
		QMetaObject::invokeMethod(dialog, &QDialog::reject,
					  Qt::QueuedConnection);
	}

	return true;
}

bool SEBrowserDialogManager::DeserializeFocusNonModalDialogById(
	CefRefPtr<CefValue> input)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (input->GetType() != VTYPE_STRING)
		return false;

	std::string id = input->GetString();

	if (!m_nonModalDialogs.count(id))
		return false;

	auto dialog = m_nonModalDialogs[id].dialog;

	dialog->activateWindow();

	return true;
}

bool SEBrowserDialogManager::DeserializeNonModalDialogDimensionsById(
	CefRefPtr<CefValue> dialogId, CefRefPtr<CefValue> dimensions)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (dialogId->GetType() != VTYPE_STRING)
		return false;

	if (dimensions->GetType() != VTYPE_DICTIONARY)
		return false;

	auto d = dimensions->GetDictionary();

	if (!d->HasKey("width") || d->GetType("width") != VTYPE_INT)
		return false;

	if (!d->HasKey("height") || d->GetType("height") != VTYPE_INT)
		return false;

	std::string id = dialogId->GetString();

	if (!m_nonModalDialogs.count(id))
		return false;

	int width = d->GetInt("width");
	int height = d->GetInt("height");

	if (width < 1)
		return false;

	if (height < 1)
		return false;

	auto dialog = m_nonModalDialogs[id].dialog;

	int centerX = dialog->x() + (dialog->width() / 2);
	int centerY = dialog->y() + (dialog->height() / 2);

	dialog->setFixedWidth(width);
	dialog->setFixedHeight(height);
	dialog->move(centerX - (width / 2), centerY - (height / 2));

	return true;
}

std::shared_ptr<std::promise<CefRefPtr<CefValue>>>
SEBrowserDialogManager::DeserializeNonModalDialog(CefRefPtr<CefValue> input)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	auto promise = std::make_shared<std::promise<CefRefPtr<CefValue>>>();

	//
	// Every path out of here either stores the promise in
	// m_nonModalDialogs, where QDialog::finished or this object's
	// destructor will fulfil it, or fulfils it in the else branch below.
	// Keep that true: the caller parks a detached thread on
	// get_future().wait(), so a promise returned unfulfilled and
	// unstored is a thread, an api call context and a page promise that
	// never come back.
	//
	// That is also why the type check below does not return early.
	// `input` is whatever a page passed to
	// window.host.showNonModalDialog(), and a typed getter on the
	// CefValue model is `if (m_type != VTYPE_X) throw;` -- a bare throw
	// outside a catch, so std::terminate. HasKey() is not a type check
	// either: GetString(k) is `m_map[k]->GetString()`, the same getter.
	//
	CefRefPtr<CefDictionaryValue> d = input->GetType() == VTYPE_DICTIONARY
						  ? input->GetDictionary()
						  : nullptr;

	auto apiServices = m_apiServices.lock();

	if (d.get() && d->HasKey("url") && d->GetType("url") == VTYPE_STRING &&
	    apiServices) {
		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;
		std::string id;

		if (d->HasKey("id") && d->GetType("id") == VTYPE_STRING) {
			id = d->GetString("id");
		}

		while (!id.size() || m_nonModalDialogs.count(id)) {
			id = CreateGloballyUniqueIdString();
		}

		if (d->HasKey("executeJavaScriptOnLoad") &&
		    d->GetType("executeJavaScriptOnLoad") == VTYPE_STRING) {
			executeJavaScriptOnLoad =
				d->GetString("executeJavaScriptOnLoad");
		}

		int width = 800;
		int height = 600;
		bool isIncognito = false;

		if (d->HasKey("width") && d->GetType("width") == VTYPE_INT) {
			width = d->GetInt("width");
		}

		if (d->HasKey("height") && d->GetType("height") == VTYPE_INT) {
			height = d->GetInt("height");
		}

		if (d->HasKey("incognitoMode") &&
		    d->GetType("incognitoMode") == VTYPE_BOOL &&
		    d->GetBool("incognitoMode")) {
			isIncognito = true;
		}

		auto dialog = new SEBrowserDialog(m_mainWindow, url,
						  executeJavaScriptOnLoad,
						  isIncognito, "nonModalDialog",
						  apiServices);

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			dialog->setWindowTitle(QString(
				d->GetString("title").ToString().c_str()));
		}

		if (d->HasKey("isResizable") &&
		    d->GetType("isResizable") == VTYPE_BOOL &&
		    d->GetBool("isResizable")) {
			auto savedMaxSize = dialog->maximumSize();

			dialog->setFixedSize(width, height);

			SEDrainEventQueue();

			dialog->setMinimumSize(width, height);
			dialog->setMaximumSize(savedMaxSize);
		} else {
			dialog->setFixedSize(width, height);
		}

		m_nonModalDialogs[id] = {dialog, promise};

		std::weak_ptr<bool> alive = m_alive;

		dialog->connect(
			dialog, &QDialog::finished,
			[this, alive, dialog, promise, id](int result) {
				//
				// Queued rejects outlive their manager: a
				// close scheduled by
				// DeserializeCloseNonModalDialogsByIds can
				// land after this object is gone, and the
				// destructor's own reject() lands while it is
				// still running. In both cases the destructor
				// has already fulfilled the promise and
				// deleted the dialog.
				//
				auto live = alive.lock();

				if (!live || !*live)
					return;

				std::lock_guard<decltype(m_mutex)> guard(
					m_mutex);

				m_nonModalDialogs.erase(id);

				CefRefPtr<CefValue> output = CefValue::Create();

				output->SetNull();

				if (result == QDialog::Accepted) {
					output = CefParseJSON(
						dialog->result(),
						JSON_PARSER_ALLOW_TRAILING_COMMAS);
				}

				dialog->deleteLater();

				promise->set_value(output);
			});

		dialog->setModal(false);
		dialog->show();
	} else {
		if (d.get() && d->HasKey("url") && !apiServices) {
			blog(LOG_WARNING,
			     "obs-streamelements-core: SEBrowserDialogManager: not opening a non-modal dialog: the services bundle is gone");
		}

		auto nullResult = CefValue::Create();
		nullResult->SetNull();

		promise->set_value(nullResult);
	}

	return promise;
}
