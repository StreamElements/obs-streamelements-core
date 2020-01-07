#include "StreamElementsPreviewManager.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QApplication>
#include <QEvent>

#include "StreamElementsCefClient.hpp"
#include "StreamElementsObsAppMonitor.hpp"

static class PreviewMouseEventFilter : public QObject {
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

		std::string objectName = o->objectName().toStdString();

		switch (e->type()) {
		case QEvent::MouseButtonDblClick:
			if (QApplication::keyboardModifiers() ==
			    Qt::NoModifier) {
				if (o->objectName() ==
				    QString("previewWindow")) {
					StreamElementsCefClient::DispatchJSEvent(
						"hostVideoPreviewMouseDoubleClicked",
						"null");
				} else if (o->objectName() ==
						   QString("sourcesDockWindow")) {
					StreamElementsCefClient::DispatchJSEvent(
						"hostCurrentSceneItemsListMouseDoubleClicked",
						"null");
				}
			}
			break;

		case QEvent::FocusAboutToChange:
			if (o->objectName() == "OBSBasicWindow") {
				StreamElementsCefClient::DispatchJSEvent(
					"hostBeforeFocusChange", "null");
			}
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
