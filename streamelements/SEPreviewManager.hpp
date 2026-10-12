#pragma once

#include <QMainWindow>
#include <QWidget>

#include "SEObsAppMonitor.hpp"

class SEPreviewManager : public SEObsAppMonitor {
public:
	SEPreviewManager(QMainWindow *parent);
	virtual ~SEPreviewManager();

protected:
	virtual void OnObsExit();

private:
	QMainWindow *m_parent;
	QObject *m_eventFilter;
};
