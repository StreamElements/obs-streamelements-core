#pragma once

#include "SEUtils.hpp"
#include <QIcon>
#include <QPixmap>

#include <functional>
#include <memory>

class SERemoteIconLoader {
public:
	typedef std::function<void(const QIcon)> setIcon_callback_t;

public:
	static CefRefPtr<SERemoteIconLoader>
	Create(setIcon_callback_t setIcon, const char *url = nullptr,
	       QPixmap *defaultPixmap = nullptr, bool requireQtPostTaskOnCached = true);

public:
	SERemoteIconLoader(setIcon_callback_t setIcon,
				       const char *url, QPixmap *defaultPixmap,
				       bool requireQtPostTaskOnCached);
	~SERemoteIconLoader();

	void LoadUrlInternal(const char *url, bool requireQtPostTaskOnCached);

public:
	void Cancel();
	void LoadUrl(const char *url);

private:
	std::recursive_mutex m_mutex;
	std::shared_ptr<CancelableTask> m_task = nullptr;
	setIcon_callback_t m_setIcon;
	bool m_cancelled = false;
};
