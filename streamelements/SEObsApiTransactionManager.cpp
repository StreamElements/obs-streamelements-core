#include "SEObsApiTransactionManager.hpp"

#include "SEPleaseWaitWindow.hpp"
#include "SEUtils.hpp"

#include <obs-frontend-api.h>

#include <QApplication>
#include <QTimer>

//
// One open transaction. Owns the timer that closes it if the caller never
// does; the timer lives on the Qt thread, because that is where it has to
// fire and where it has to be stopped.
//
class SEObsApiTransactionManager::Handle {
public:
	Handle(std::string id, int timeoutMilliseconds,
	       std::function<void()> onTimer)
		: m_id(id), m_onTimer(onTimer)
	{
		m_timer = new QTimer();
		m_timer->moveToThread(qApp->thread());
		m_timer->setInterval(timeoutMilliseconds);
		m_timer->setSingleShot(true);
		QObject::connect(m_timer, &QTimer::timeout,
				 [this]() { m_onTimer(); });
		QMetaObject::invokeMethod(m_timer, "start",
					  Qt::QueuedConnection,
					  Q_ARG(int, timeoutMilliseconds));
	}

	~Handle()
	{
		QMetaObject::invokeMethod(m_timer, "stop", Qt::QueuedConnection,
					  Q_ARG(int, 0));

		m_timer->deleteLater();
	}

private:
	std::string m_id;
	std::function<void()> m_onTimer;
	QTimer *m_timer;
};

/* ========================================================================= */

SEObsApiTransactionManager::SEObsApiTransactionManager(
	std::shared_ptr<SEPleaseWaitWindow> pleaseWaitWindow)
	: m_pleaseWaitWindow(pleaseWaitWindow)
{
}

SEObsApiTransactionManager::~SEObsApiTransactionManager()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_transactions.empty())
		return;

	//
	// Transactions still open at teardown. Each one is holding an
	// autosave deferral and a share of the dialog, and neither unwinds by
	// itself, so close them here rather than leaving OBS with autosave
	// off and a modal dialog up.
	//
	blog(LOG_INFO,
	     "obs-streamelements-core: SEObsApiTransactionManager: closing %d transaction(s) still open at teardown",
	     static_cast<int>(m_transactions.size()));

	for (auto &pair : m_transactions) {
		delete pair.second;

		obs_frontend_defer_save_end();
	}

	m_transactions.clear();

	if (m_pleaseWaitWindow)
		m_pleaseWaitWindow->Hide();
}

std::string SEObsApiTransactionManager::Create(int timeoutMilliseconds)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string id = CreateGloballyUniqueIdString();

	//
	// Weak: the timer fires on the Qt thread up to a minute later, and
	// this manager can be gone by then -- its destructor above has
	// already closed whatever was open.
	//
	std::weak_ptr<SEObsApiTransactionManager> weakSelf = weak_from_this();

	m_transactions[id] = new Handle(id, timeoutMilliseconds, [weakSelf,
								  id]() {
		if (auto self = weakSelf.lock())
			self->Complete(id);
	});

	if (m_transactions.size() == 1) {
		if (m_pleaseWaitWindow)
			m_pleaseWaitWindow->Show();

		obs_frontend_defer_save_begin();
	}

	return id;
}

void SEObsApiTransactionManager::BeginDeferSave()
{
	obs_frontend_defer_save_begin();
}

void SEObsApiTransactionManager::EndDeferSave()
{
	obs_frontend_defer_save_end();
}

void SEObsApiTransactionManager::Complete(std::string id)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (!m_transactions.count(id))
		return;

	delete m_transactions[id];

	m_transactions.erase(id);

	if (m_transactions.size() == 0) {
		obs_frontend_defer_save_end();

		if (m_pleaseWaitWindow)
			m_pleaseWaitWindow->Hide();
	}
}
