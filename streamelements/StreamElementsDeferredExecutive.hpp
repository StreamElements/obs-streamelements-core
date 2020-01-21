#pragma once

#include <QTimer>

#include <mutex>

class StreamElementsDeferredExecutive
{
public:
	StreamElementsDeferredExecutive();
	~StreamElementsDeferredExecutive();

public:
	void Cancel();
	void Signal(std::function<void()> callback, int delayMilliseconds);

private:
	QTimer *m_timer = nullptr;
	std::function<void()> m_callback;
	std::recursive_mutex m_mutex;
};
