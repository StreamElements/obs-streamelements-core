#pragma once

#include <QMainWindow>
#include <QWidget>

#include "SEObsAppMonitor.hpp"

#include <memory>

class SEHostEventDispatcher;

class SEPreviewManager : public SEObsAppMonitor {
public:
	SEPreviewManager(
		QMainWindow *parent,
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher);
	virtual ~SEPreviewManager();

protected:
	virtual void OnObsExit();

private:
	QMainWindow *m_parent;
	QObject *m_eventFilter;

	//
	// Handed to the event filter, which is the object that announces the
	// preview's mouse and focus events.
	//
	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;
};
