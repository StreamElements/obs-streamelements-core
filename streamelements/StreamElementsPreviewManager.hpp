#pragma once

#include <QMainWindow>
#include <QWidget>

#include "StreamElementsObsAppMonitor.hpp"

class StreamElementsPreviewManager : public StreamElementsObsAppMonitor {
public:
	StreamElementsPreviewManager(QMainWindow *parent);
	virtual ~StreamElementsPreviewManager();

protected:
	virtual void OnObsExit();

private:
	QMainWindow *m_parent;
	QObject *m_eventFilter;
};
