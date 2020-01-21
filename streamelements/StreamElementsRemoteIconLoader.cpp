#include "StreamElementsRemoteIconLoader.hpp"

#include <QCache>
#include <QReadWriteLock>

static class CachedPixmap : QObject {
private:
	QIcon m_icon;

public:
	CachedPixmap(QIcon icon) : m_icon(icon) {}
	~CachedPixmap() {}

	QIcon icon() { return m_icon; }
};

static QCache<QString, CachedPixmap> s_cache;
QReadWriteLock s_cacheLock;

static bool GetCached(const QString &key, QIcon &icon)
{
	bool result = false;

	s_cacheLock.lockForRead();

	CachedPixmap *object = s_cache.object(key);

	if (object) {
		icon = object->icon();

		result = true;
	}

	s_cacheLock.unlock();

	return result;
}

static void SetCached(const QString &key, const QIcon &icon)
{
	s_cacheLock.lockForWrite();

	s_cache.insert(key, new CachedPixmap(icon));

	s_cacheLock.unlock();
}

StreamElementsRemoteIconLoader::StreamElementsRemoteIconLoader(
	setIcon_callback_t setIcon, const char *url, QPixmap *defaultPixmap,
	bool requireQtPostTaskOnCached)
	: m_setIcon(setIcon)
{
	if (url && *url) {
		LoadUrlInternal(url, requireQtPostTaskOnCached);
	} else {
		if (defaultPixmap) {
			this->AddRef();

			QPixmap pixmap(16, 16);

			if (defaultPixmap) {
				pixmap = *defaultPixmap;
			} else {
				pixmap.fill(Qt::transparent);
			}

			if (!requireQtPostTaskOnCached) {
				std::lock_guard<std::recursive_mutex> guard(
					m_mutex);

				if (!m_cancelled) {
					m_setIcon(QIcon(pixmap));
				}
			} else {
				this->AddRef();

				QtPostTask([=]() {
					std::lock_guard<std::recursive_mutex>
						guard(m_mutex);

					if (!m_cancelled) {
						m_setIcon(QIcon(pixmap));
					}

					this->Release();
				});
			}
		}
	}
}

StreamElementsRemoteIconLoader::~StreamElementsRemoteIconLoader()
{
	Cancel();
}

CefRefPtr<StreamElementsRemoteIconLoader>
StreamElementsRemoteIconLoader::Create(setIcon_callback_t setIcon,
				       const char *url, QPixmap *defaultPixmap,
				       bool requireQtPostTaskOnCached)
{
	return new StreamElementsRemoteIconLoader(setIcon, url, defaultPixmap,
						  requireQtPostTaskOnCached);
}

void StreamElementsRemoteIconLoader::Cancel()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	m_cancelled = true;

	if (m_task) {
		// m_task->Cancel();

		m_task = nullptr;
	}

	if (m_request) {
		m_request->Cancel();

		m_request = nullptr;
	}
}

void StreamElementsRemoteIconLoader::LoadUrl(const char* url)
{
	LoadUrlInternal(url, true);
}

void StreamElementsRemoteIconLoader::LoadUrlInternal(
	const char *url, bool requireQtPostTaskOnCached)
{
	if (!url)
		return;

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	Cancel();

	m_cancelled = false;

	QString cacheKey(url);

	QIcon cached;
	if (GetCached(cacheKey, cached)) {
		if (!requireQtPostTaskOnCached) {
			if (!m_cancelled) {
				m_setIcon(cached);
			}
		} else {
			this->AddRef();

			QtPostTask([this, cached]() {
				std::lock_guard<std::recursive_mutex> guard(
					m_mutex);

				if (!m_cancelled) {
					m_setIcon(cached);
				}

				this->Release();
			});
		}

		return;
	}

	this->AddRef();

	m_task = CefHttpGetAsync(
		url,
		[&](CefRefPtr<CefURLRequest> request) { m_request = request; },
		[this, cacheKey](bool success, void *data, size_t len) {
			std::lock_guard<std::recursive_mutex> guard(m_mutex);

			if (m_request) {
				m_request = nullptr;
			}

			m_task = nullptr;

			if (success && !m_cancelled) {
				QByteArray buffer = QByteArray::fromRawData(
					(char *)data, len);
				QPixmap pixmap;
				if (pixmap.loadFromData(buffer)) {
					QIcon icon(pixmap);

					SetCached(cacheKey, icon);

					this->AddRef();

					QtPostTask([this, icon]() {
						std::lock_guard<
							std::recursive_mutex>
							guard(m_mutex);

						if (!m_cancelled) {
							m_setIcon(icon);
						}

						this->Release();
					});
				}
			}

			this->Release();
		});
}
