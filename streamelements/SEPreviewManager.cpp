#include "SEPreviewManager.hpp"
#include "SEHostEventDispatcher.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QApplication>
#include <QEvent>

#include "SEObsAppMonitor.hpp"
#include "SEUtils.hpp"

class PreviewMouseEventFilter : public QObject {
private:
	QMainWindow *m_mainWindow;
	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;

public:
	PreviewMouseEventFilter(
		QMainWindow *mainWindow,
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher)
		: m_mainWindow(mainWindow),
		  m_hostEventDispatcher(hostEventDispatcher)
	{
		QCoreApplication::instance()->installEventFilter(this);
	}

	//
	// One place that checks the handle rather than three.
	//
	void Dispatch(std::string event, std::string argsJson)
	{
		if (!m_hostEventDispatcher)
			return;

		m_hostEventDispatcher->DispatchJSEventGlobal(event, argsJson);
	}

	virtual ~PreviewMouseEventFilter()
	{
		QCoreApplication::instance()->removeEventFilter(this);
	}

	virtual bool eventFilter(QObject *o, QEvent *e) override
	{
		QWidget *centralWidget = m_mainWindow->centralWidget();

		switch (e->type()) {
		case QEvent::MouseButtonDblClick:
			if (QApplication::keyboardModifiers() ==
			    Qt::NoModifier) {
				if (o->objectName() ==
				    QString("previewWindow")) {
					Dispatch(
						"hostVideoPreviewMouseDoubleClicked",
						"null");
				} else if (o->objectName() ==
						   QString("sourcesDockWindow")) {
					Dispatch(
						"hostCurrentSceneItemsListMouseDoubleClicked",
						"null");
				}
			}
			break;

		case QEvent::FocusAboutToChange:
			if (o->objectName() == "OBSBasicWindow") {
				Dispatch(
					"hostBeforeFocusChange", "null");
			}
			break;
		default:
			break;
		}

		return QObject::eventFilter(o, e);
	}
};

SEPreviewManager::SEPreviewManager(
	QMainWindow *parent,
	std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher)
	: m_parent(parent)
{
	m_eventFilter = new PreviewMouseEventFilter(m_parent,
						    m_hostEventDispatcher);
}

SEPreviewManager::~SEPreviewManager()
{
	if (m_eventFilter) {
		delete m_eventFilter;

		m_eventFilter = nullptr;
	}
}

void SEPreviewManager::OnObsExit()
{
	if (m_eventFilter) {
		delete m_eventFilter;

		m_eventFilter = nullptr;
	}
}
