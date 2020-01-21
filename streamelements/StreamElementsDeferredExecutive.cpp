#include "StreamElementsDeferredExecutive.hpp"
#include "StreamElementsUtils.hpp"

StreamElementsDeferredExecutive::StreamElementsDeferredExecutive() {}

StreamElementsDeferredExecutive::~StreamElementsDeferredExecutive()
{
	Cancel();
}

void StreamElementsDeferredExecutive::Cancel()
{
	if (!m_timer)
		return;

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	m_timer->stop();
	m_timer->deleteLater();
	m_timer = nullptr;
}

void StreamElementsDeferredExecutive::Signal(std::function<void()> callback,
					     int delayMilliseconds)
{
	Cancel();

	m_callback = callback;

	m_timer = new QTimer();
	m_timer->moveToThread(qApp->thread());
	m_timer->setSingleShot(true);
	m_timer->setInterval(delayMilliseconds);

	QObject::connect(m_timer, &QTimer::timeout, [&]() {
		std::lock_guard<std::recursive_mutex> guard(m_mutex);

		m_timer->deleteLater();
		m_timer = nullptr;

		m_callback();
	});

	QMetaObject::invokeMethod(m_timer, "start", Qt::QueuedConnection,
				  Q_ARG(int, 0));
}
