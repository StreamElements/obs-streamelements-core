#include "StreamElementsPreviewManager.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QApplication>
#include <QEvent>

#include "StreamElementsObsAppMonitor.hpp"
#include "StreamElementsUtils.hpp"

class PreviewMouseEventFilter : public QObject {
private:
	QMainWindow *m_mainWindow;

public:
	PreviewMouseEventFilter(QMainWindow *mainWindow)
		: m_mainWindow(mainWindow)
	{
		QCoreApplication::instance()->installEventFilter(this);
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
					DispatchJSEventGlobal(
						"hostVideoPreviewMouseDoubleClicked",
						"null");
				} else if (o->objectName() ==
						   QString("sourcesDockWindow")) {
					DispatchJSEventGlobal(
						"hostCurrentSceneItemsListMouseDoubleClicked",
						"null");
				}
			}
			break;

		case QEvent::FocusAboutToChange:
			if (o->objectName() == "OBSBasicWindow") {
				DispatchJSEventGlobal(
					"hostBeforeFocusChange", "null");
			}
			break;
		default:
			break;
		}

		return QObject::eventFilter(o, e);
	}
};

StreamElementsPreviewManager::StreamElementsPreviewManager(QMainWindow *parent)
	: m_parent(parent)
{
	m_eventFilter = new PreviewMouseEventFilter(m_parent);
}

StreamElementsPreviewManager::~StreamElementsPreviewManager()
{
	if (m_eventFilter) {
		delete m_eventFilter;

		m_eventFilter = nullptr;
	}
}

void StreamElementsPreviewManager::OnObsExit()
{
	if (m_eventFilter) {
		delete m_eventFilter;

		m_eventFilter = nullptr;
	}
}
