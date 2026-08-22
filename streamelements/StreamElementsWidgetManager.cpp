#include "StreamElementsWidgetManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <cassert>
#include <mutex>

#include <QApplication>

StreamElementsWidgetManager::StreamElementsWidgetManager(QMainWindow *parent)
	: m_parent(parent), m_nativeCentralWidget(nullptr)
{
	assert(parent);
}

//
// Destroying a dock widget while OBSInit() is still on the stack corrupts the
// widget tree: OBS can emit FINISHED_LOADING and EXIT from two separate
// on_event() calls inside OBSBasic::OnFirstLoad() without ever returning to
// its own event loop, so our widgets get built into a main window that is
// already being dismantled and then torn down seconds later. The fault landed
// in QWidget::~QWidget reading a child's d->parent, which pointed at neither
// the dock nor anything live (CORE-786).
//
// So: only delete when OBS has demonstrably reached its own event loop.
// Otherwise detach the dock from OBS's tree and leak it. This path is reached
// during shutdown of a process that is exiting anyway, and a handful of leaked
// docks is a straight trade against a crash. IsObsInitFinished() is true for
// the entire normal lifetime of the plugin, so RemoveDockWidget() at runtime
// still deletes and nothing accumulates.
//
static void SafeDeleteDockWidget(QPointer<QDockWidget> dock, const char *id,
				 bool useDeleteLater)
{
	if (!dock)
		return;

	if (!IsObsInitFinished()) {
		blog(LOG_WARNING,
		     "[obs-streamelements-core]: leaking dock widget '%s': OBS has not finished initializing, deleting it now would corrupt the widget tree",
		     id);

		// Detach from the main window so OBS's own teardown does not
		// walk into it later.
		dock->setParent(nullptr);

		return;
	}

	if (useDeleteLater)
		dock->deleteLater();
	else
		delete dock.data();
}

StreamElementsWidgetManager::~StreamElementsWidgetManager()
{
	while (DestroyCurrentCentralWidget()) {
		// NOP
	}

	for (auto pair : m_dockWidgets) {
		blog(LOG_INFO,
		     "[obs-streamelements-core]: destroying dock widget '%s'",
		     pair.first.c_str());

		// pair.second is already a QPointer (see the header), so a dock
		// destroyed behind our back -- by its QMainWindow parent during
		// OBS teardown, or by a deleteLater() posted elsewhere -- reads
		// back null here rather than dangling.
		//
		// Wrapping the raw pointer in a QPointer *here* would not work
		// and was the gap in the first version of this fix: QPointer
		// only tracks destructions that happen after it is constructed,
		// so one built from an already-dangling pointer is born
		// non-null and the guard passes (CORE-786).
		QPointer<QDockWidget> dock = pair.second;

		if (!dock) {
			blog(LOG_INFO,
			     "[obs-streamelements-core]: dock widget '%s' was already destroyed; skipping",
			     pair.first.c_str());
			continue;
		}

		if (m_parent)
			m_parent->removeDockWidget(dock);

		// Deliberately NOT draining the event queue here (CORE-777).
		// A QApplication::sendPostedEvents() at this point dispatches
		// any DeferredDelete already posted for this very widget,
		// destroying it, and the delete below then runs on an
		// already-destructed object. That aborts inside Qt, where
		// ~QObject deletes its QObjectData through a pure virtual
		// destructor: the second pass finds the vtable degraded to
		// QObjectData and lands in _purecall.
		//
		// Draining buys nothing anyway -- ~QObject discards the events
		// posted to the object being destroyed.

		if (!dock)
			continue;

		SafeDeleteDockWidget(dock, pair.first.c_str(), false);
	}

	m_dockWidgets.clear();
	m_dockWidgetAreas.clear();
}

//
// The QApplication::sendPostedEvents() and setMinimumSize() black
// magic below is a very unpleasant hack to ensure that the central
// widget remains the same size as the previously visible central
// widget.
//
// This is done by first measuring the size() of the current
// central widget, then removing the current central widget and
// replacing it with a new one, setting the new widget MINIMUM
// size to the previously visible central widget width & height
// and draining the Qt message queue with a call to
// QApplication::sendPostedEvents().
// Then we reset the new central widget minimum size to 0x0.
//
void StreamElementsWidgetManager::PushCentralWidget(
	QWidget *widget)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	//if (m_currentCentralWidget) return;
	if (m_nativeCentralWidget) return;

	// This will be additionally enforced by OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED event
	// handler in handle_obs_frontend_event defined in StreamElementsGlobalStateManager.cpp
	//obs_frontend_set_preview_program_mode(false);

	// Make sure changes take effect by draining the event queue
	SEDrainEventQueue();

	QSize prevSize = mainWindow()->centralWidget()->size();

	widget->setMinimumSize(prevSize);

	m_nativeCentralWidget = m_parent->takeCentralWidget();

	m_parent->setCentralWidget(widget);

	// Drain event queue
	SEDrainEventQueue();

	widget->setMinimumSize(0, 0);

	/*
	QLayout* layout = m_parent->centralWidget()->findChild<QLayout*>("previewLayout");
	QWidget* preview = m_parent->centralWidget()->findChild<QWidget*>("preview");

	preview->setVisible(false);
	layout->addWidget(widget);

	m_currentCentralWidget = widget;
	*/
}

bool StreamElementsWidgetManager::DestroyCurrentCentralWidget()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	//if (!m_currentCentralWidget) return false;
	if (!m_nativeCentralWidget) return false;

	/*
	QLayout* layout = m_parent->centralWidget()->findChild<QLayout*>("previewLayout");
	QWidget* preview = m_parent->centralWidget()->findChild<QWidget*>("preview");

	m_currentCentralWidget->setVisible(false);

	m_currentCentralWidget->parentWidget()->layout()->removeWidget(m_currentCentralWidget);
	m_currentCentralWidget->deleteLater();

	preview->setVisible(true);

	m_currentCentralWidget = nullptr;
	*/

	SaveDockWidgetsGeometry();

	auto currentCentralWisget = m_parent->centralWidget();
	if (currentCentralWisget != m_nativeCentralWidget) {
		static_cast<StreamElementsBrowserWidget *>(currentCentralWisget)
			->RemoveVideoCompositionView();
	}

	SEDrainEventQueue();
	QSize currSize = mainWindow()->centralWidget()->size();

	m_parent->setCentralWidget(m_nativeCentralWidget);

	m_nativeCentralWidget = nullptr;

	mainWindow()->centralWidget()->setMinimumSize(currSize);

	// Drain event queue
	SEDrainEventQueue();

	mainWindow()->centralWidget()->setMinimumSize(0, 0);

	RestoreDockWidgetsGeometry();

	// No more widgets
	return false;
}

bool StreamElementsWidgetManager::HasCentralWidget()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	//return !!m_currentCentralWidget;
	return !!m_nativeCentralWidget;
}

void StreamElementsWidgetManager::OnObsExit()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Empty stack
	while (DestroyCurrentCentralWidget()) {
	}
}

bool StreamElementsWidgetManager::AddDockWidget(
	const char *const id, const char *const title, QWidget *const widget,
	const Qt::DockWidgetArea area, const Qt::DockWidgetAreas allowedAreas,
	const QDockWidget::DockWidgetFeatures features)
{
	assert(id);
	assert(title);
	assert(widget);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_dockWidgets.count(id)) {
		return false;
	}

	class TrackedDockWidget : public QDockWidget {
	public:
		TrackedDockWidget(const QString &title,
				  QWidget *parent = Q_NULLPTR,
				  Qt::WindowFlags flags = Qt::WindowFlags())
			: QDockWidget(title, parent, flags)
		{
			setAttribute(Qt::WA_NativeWindow);
		}

	protected:
		virtual void resizeEvent(QResizeEvent *event) override
		{
			QDockWidget::resizeEvent(event);

			AdviseHostUserInterfaceStateChanged();
		}

		virtual void moveEvent(QMoveEvent *event) override
		{
			QDockWidget::moveEvent(event);

			AdviseHostUserInterfaceStateChanged();
		}

		virtual void closeEvent(QCloseEvent *event) override
		{
			event->ignore();

			setVisible(false);
		}

		virtual bool event(QEvent* event) override
		{
			if (event->type() == QEvent::NonClientAreaMouseButtonDblClick) {
				event->ignore();

				setFloating(!isFloating());

				return true;
			}

			return QDockWidget::event(event);
		}
	};

	QDockWidget *dock = new TrackedDockWidget(title, m_parent);

	dock->setObjectName(QString(id));

	dock->setAllowedAreas(allowedAreas);
	dock->setFeatures(features);
	dock->setWindowTitle(title);

	dock->setWidget(widget);
	m_parent->addDockWidget(area, dock);

	m_dockWidgets[id] = dock;
	m_dockWidgetAreas[id] = area;

	if (area == Qt::NoDockWidgetArea) {
		dock->setFloating(false);
		SEDrainEventQueue();
		dock->setFloating(true);
		SEDrainEventQueue();
	}

	std::string savedId = id;

	QObject::connect(
		dock, &QDockWidget::dockLocationChanged,
		[savedId, dock, this](Qt::DockWidgetArea area) {
			std::lock_guard<std::recursive_mutex> guard(m_mutex);

			if (!m_dockWidgets.count(savedId)) {
				return;
			}

			m_dockWidgetAreas[savedId] = area;

			QtPostTask([]() -> void {
				if (!StreamElementsGlobalStateManager::
					    IsInstanceAvailable())
					return;

				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();
			});
		});

	QObject::connect(dock, &QDockWidget::visibilityChanged, [this]() {
		QtPostTask([]() -> void {
			if (!StreamElementsGlobalStateManager::
				    IsInstanceAvailable())
				return;

			StreamElementsGlobalStateManager::GetInstance()
				->PersistState();
		});

		QtDelayTask(
			[]() -> void {
				if (!StreamElementsGlobalStateManager::
					    IsInstanceAvailable())
					return;

				if (!StreamElementsGlobalStateManager::
					     GetInstance()
						     ->IsInitialized())
					return;

				auto menuManager =
					StreamElementsGlobalStateManager::
						GetInstance()
							->GetMenuManager();

				if (!menuManager)
					return;

				menuManager->Update();
			},
			1);
	});

	// Make effort to display QDockWidget on top of the main window right away
	dock->raise();
	dock->activateWindow();
	dock->raise();

	return true;
}

bool StreamElementsWidgetManager::ShowWidgetById(const char *const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget *dock = m_dockWidgets[id];

	// Null when Qt destroyed the dock behind our back (CORE-786).
	if (!dock)
		return false;

	dock->setVisible(true);

	return true;
}

bool StreamElementsWidgetManager::SetWidgetTitleById(const char* const id,
	const char* const title)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget *dock = m_dockWidgets[id];

	// Null when Qt destroyed the dock behind our back (CORE-786).
	if (!dock)
		return false;

	dock->setWindowTitle(QString(title));

	return true;
}

bool StreamElementsWidgetManager::HideWidgetById(const char *const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget *dock = m_dockWidgets[id];

	// Null when Qt destroyed the dock behind our back (CORE-786).
	if (!dock)
		return false;

	dock->setVisible(false);

	return true;
}

static QDockWidget *GetSystemWidgetById(const char *widgetId)
{
	QMainWindow *main = (QMainWindow *)obs_frontend_get_main_window();

	if (!main)
		return nullptr;

	auto list = main->findChildren<QDockWidget *>();

	for (auto item : list) {
		auto name = QString(":") + item->objectName();

		if (name == widgetId)
			return item;
	}

	return nullptr;
}

bool StreamElementsWidgetManager::ToggleWidgetFloatingStateById(
	const char *const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QDockWidget *dock = nullptr;

	if (m_dockWidgets.count(id)) {
		dock = m_dockWidgets[id];
	} else {
		dock = GetSystemWidgetById(id);
	}

	if (!dock)
		return false;

	dock->setFloating(!dock->isFloating());

	return true;
}

bool StreamElementsWidgetManager::SetWidgetDimensionsById(const char *const id,
							  const int width,
							  const int height)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget *dock = m_dockWidgets[id];

	// Null when Qt destroyed the dock behind our back (CORE-786).
	if (!dock)
		return false;

	if (!dock->isFloating() || !dock->window()) {
		return false;
	}

	SEDrainEventQueue();

	QSize prevMin = dock->window()->minimumSize();
	QSize prevMax = dock->window()->maximumSize();

	if (width >= 0) {
		dock->window()->setMinimumWidth(width);
		dock->window()->setMaximumWidth(width);
	}

	if (height >= 0) {
		dock->window()->setMinimumHeight(height);
		dock->window()->setMaximumHeight(height);
	}

	SEDrainEventQueue();

	dock->window()->setMinimumSize(prevMin);
	dock->window()->setMaximumSize(prevMax);

	return true;
}

bool StreamElementsWidgetManager::SetWidgetPositionById(const char *const id,
							const int left,
							const int top)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QDockWidget *dock = nullptr;

	if (m_dockWidgets.count(id)) {
		dock = m_dockWidgets[id];
	} else {
		dock = GetSystemWidgetById(id);
	}

	if (!dock)
		return false;

	QPoint pos = dock->window()->pos();

	if (left >= 0) {
		pos.setX(left);
	}

	if (top >= 0) {
		pos.setY(top);
	}

	dock->window()->move(pos);

	return true;
}

bool StreamElementsWidgetManager::RemoveDockWidget(const char *const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QPointer<QDockWidget> dock = m_dockWidgets[id];

	m_dockWidgets.erase(id);
	m_dockWidgetAreas.erase(id);

	// The map entry goes either way; the widget is only touched if Qt has
	// not already destroyed it (CORE-786).
	if (!dock)
		return true;

	if (m_parent)
		m_parent->removeDockWidget(dock);

	SafeDeleteDockWidget(dock, id, true);

	return true;
}

void StreamElementsWidgetManager::GetDockWidgetIdentifiers(
	std::vector<std::string> &result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto imap : m_dockWidgets) {
		result.push_back(imap.first);
	}
}

QDockWidget *StreamElementsWidgetManager::GetDockWidget(const char *const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return nullptr;
	}

	return m_dockWidgets[id];
}

StreamElementsWidgetManager::DockWidgetInfo *
StreamElementsWidgetManager::GetDockWidgetInfo(const char *const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QDockWidget *dockWidget = GetDockWidget(id);

	if (!dockWidget) {
		return nullptr;
	}

	StreamElementsWidgetManager::DockWidgetInfo *result =
		new StreamElementsWidgetManager::DockWidgetInfo();

	result->m_widget = dockWidget->widget();

	result->m_id = id;
	result->m_title = dockWidget->windowTitle().toStdString();
	result->m_visible = dockWidget->isVisible();

	if (dockWidget->isFloating()) {
		result->m_dockingArea = "floating";
	} else {
		switch (m_dockWidgetAreas[id]) {
		case Qt::LeftDockWidgetArea:
			result->m_dockingArea = "left";
			break;
		case Qt::RightDockWidgetArea:
			result->m_dockingArea = "right";
			break;
		case Qt::TopDockWidgetArea:
			result->m_dockingArea = "top";
			break;
		case Qt::BottomDockWidgetArea:
			result->m_dockingArea = "bottom";
			break;
		//case Qt::NoDockWidgetArea:
		default:
			result->m_dockingArea = "floating";
			break;
		}
	}

	return result;
}

void StreamElementsWidgetManager::SerializeDockingWidgets(std::string &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefValue> root = CefValue::Create();

	SerializeDockingWidgets(root);

	// Convert data to JSON
	CefString jsonString = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	output = jsonString.ToString();
}

void StreamElementsWidgetManager::DeserializeDockingWidgets(std::string &input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Convert JSON string to CefValue
	CefRefPtr<CefValue> root = CefParseJSON(
		CefString(input), JSON_PARSER_ALLOW_TRAILING_COMMAS);

	DeserializeDockingWidgets(root);
}

void StreamElementsWidgetManager::SaveDockWidgetsGeometry()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	m_dockWidgetSavedMinSize.clear();

	SEDrainEventQueue();

	for (auto iter : m_dockWidgets) {
		// Null when Qt destroyed the dock behind our back (CORE-786).
		if (!iter.second)
			continue;

		m_dockWidgetSavedMinSize[iter.first] = iter.second->size();
	}
}

void StreamElementsWidgetManager::RestoreDockWidgetsGeometry()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	SEDrainEventQueue();

	std::map<std::string, QSize> maxSize;

	for (auto iter : m_dockWidgetSavedMinSize) {
		QPointer<QDockWidget> dock;

		if (m_dockWidgets.count(iter.first))
			dock = m_dockWidgets[iter.first];

		if (dock) {
			maxSize[iter.first] = dock->maximumSize();

			dock->setMinimumSize(iter.second);
			dock->setMaximumSize(iter.second);
		}
	}

	SEDrainEventQueue();

	for (auto iter : m_dockWidgetSavedMinSize) {
		QPointer<QDockWidget> dock;

		if (m_dockWidgets.count(iter.first))
			dock = m_dockWidgets[iter.first];

		if (dock) {
			dock->setMinimumSize(QSize(0, 0));
			dock->setMaximumSize(maxSize[iter.first]);
		}
	}

	SEDrainEventQueue();
}
