#pragma once

#include "StreamElementsUtils.hpp"
#include <QIcon>
#include <QPixmap>

#include <functional>

static class StreamElementsRemoteIconLoader : public CefBaseRefCounted {
public:
	typedef std::function<void(const QIcon)> setIcon_callback_t;

public:
	static CefRefPtr<StreamElementsRemoteIconLoader>
	Create(setIcon_callback_t setIcon, const char *url = nullptr,
	       QPixmap *defaultPixmap = nullptr, bool requireQtPostTaskOnCached = true);

private:
	StreamElementsRemoteIconLoader(setIcon_callback_t setIcon,
				       const char *url, QPixmap *defaultPixmap,
				       bool requireQtPostTaskOnCached);
	~StreamElementsRemoteIconLoader();

	void LoadUrlInternal(const char *url, bool requireQtPostTaskOnCached);

public:
	void Cancel();
	void LoadUrl(const char *url);

private:
	std::recursive_mutex m_mutex;
	CefRefPtr<CefURLRequest> m_request = nullptr;
	CefRefPtr<CefCancelableTask> m_task = nullptr;
	setIcon_callback_t m_setIcon;
	bool m_cancelled = false;

	IMPLEMENT_REFCOUNTING(StreamElementsRemoteIconLoader);
};
