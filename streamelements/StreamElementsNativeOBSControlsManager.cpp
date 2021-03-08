#include "StreamElementsNativeOBSControlsManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"
#include <QDockWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMetaObject>
#include <QMessageBox>
#include <QPalette>
#include <QColor>
#include <obs-module.h>
#include <obs-frontend-api.h>

StreamElementsNativeOBSControlsManager* StreamElementsNativeOBSControlsManager::GetInstance()
{
	static StreamElementsNativeOBSControlsManager* s_instance = nullptr;
	static std::mutex s_mutex;

	if (s_instance == nullptr) {
		std::lock_guard<std::mutex> guard(s_mutex);

		s_instance = new StreamElementsNativeOBSControlsManager((QMainWindow*) obs_frontend_get_main_window());
	}

	return s_instance;
}

StreamElementsNativeOBSControlsManager::StreamElementsNativeOBSControlsManager(QMainWindow* mainWindow) :
	m_mainWindow(mainWindow)
{
	QDockWidget* controlsDock = (QDockWidget*)m_mainWindow->findChild<QDockWidget*>("controlsDock");

	m_nativeStartStopStreamingButton = (QPushButton*)controlsDock->findChild<QPushButton*>("streamButton");

	if (m_nativeStartStopStreamingButton) {
		m_nativeStartStopStreamingButton->setVisible(false);
	}

	QVBoxLayout* buttonsVLayout = (QVBoxLayout*)controlsDock->findChild<QVBoxLayout*>("buttonsVLayout");

	if (buttonsVLayout) {
		m_startStopStreamingButton = new QPushButton();
		m_startStopStreamingButton->setFixedHeight(28);
		buttonsVLayout->insertWidget(0, m_startStopStreamingButton);

		// Real state will be set by OBS_FRONTEND_EVENT_FINISHED_LOADING event
		// handled by handle_obs_frontend_event()
		SetStreamingStoppedState();

		connect(m_startStopStreamingButton, SIGNAL(clicked()),
			this, SLOT(OnStartStopStreamingButtonClicked()));

		InitHotkeys();

	}

	obs_frontend_add_event_callback(handle_obs_frontend_event, this);

	m_nativeCentralWidget = mainWindow->centralWidget();

	m_nativePreviewLayout =
		m_nativeCentralWidget->findChild<QLayout *>("previewLayout");

	m_nativePreviewLayoutParent =
		m_nativeCentralWidget->findChild<QLayout *>(
			"horizontalLayout_2");

	m_nativePreviewWidget =
		m_nativeCentralWidget->findChild<QWidget *>("preview");

	if (m_nativePreviewLayout && m_nativePreviewLayoutParent &&
	    m_nativePreviewWidget) {
		m_previewTitleContainer = new QWidget();
		m_previewTitleContainer->setContentsMargins(0, 0, 0, 0);
		m_previewTitleContainer->setObjectName(
			"streamelements_preview_title_container");

		m_previewTitleLayout = new QHBoxLayout();
		m_previewTitleLayout->setContentsMargins(0, 0, 0, 0);

		m_previewTitleContainer->setLayout(m_previewTitleLayout);

		m_previewFrame = new QFrame();
		m_previewFrameLayout = new QVBoxLayout();

		m_previewFrame->setObjectName(
			"streamelements_central_widget_frame");
		m_previewFrame->setLayout(m_previewFrameLayout);

		m_nativePreviewLayoutParent->removeItem(
			m_nativePreviewLayout); // ownership remains the same as when added
		m_nativePreviewLayoutParent->addWidget(m_previewFrame);

		m_previewFrameLayout->addWidget(m_previewTitleContainer);
		m_previewFrameLayout->addLayout(m_nativePreviewLayout);

		m_previewFrameLayout->setContentsMargins(0, 0, 0, 0);
		m_previewFrameLayout->setSpacing(0);

		m_previewFrame->setFrameStyle(QFrame::Panel);
		m_previewFrame->setFrameShadow(QFrame::Plain);
		m_previewFrame->setContentsMargins(0, 0, 0, 0);

		HidePreviewTitleBar();
		HidePreviewFrame();
	}
}

StreamElementsNativeOBSControlsManager::~StreamElementsNativeOBSControlsManager()
{
	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);

	if (m_startStopStreamingButton) {
		ShutdownHotkeys();

		// OBS Main window owns the button and might have destroyed it by now
		//
		// m_startStopStreamingButton->setVisible(false);
		// m_startStopStreamingButton->deleteLater();

		m_startStopStreamingButton = nullptr;
	}

	if (m_nativeStartStopStreamingButton) {
		// OBS Main window owns the button and might have destroyed it by now
		//
		// m_nativeStartStopStreamingButton->setVisible(true);

		m_nativeStartStopStreamingButton = nullptr;
	}

	HidePreviewTitleBar();
	HidePreviewFrame();

	if (m_nativePreviewLayout && m_nativePreviewLayoutParent &&
	    m_nativePreviewWidget) {
		m_nativePreviewLayoutParent->removeWidget(m_previewFrame); // ownership remains the same as when added

		m_nativePreviewLayoutParent->addItem(
			m_nativePreviewLayout);

		m_previewFrame->disconnect();

		m_previewFrame->deleteLater();

		m_previewFrame = nullptr;
		m_nativePreviewLayout = nullptr;

		m_previewTitleLayout = nullptr;
		m_previewTitleContainer = nullptr;
	}

	m_nativeCentralWidget = nullptr;
}

bool StreamElementsNativeOBSControlsManager::DeserializePreviewFrame(
	CefRefPtr<CefValue> input)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	int width = 6;
	std::string color = "#F98215";
	std::string style = "solid";

	if (d->HasKey("width") && d->GetType("width") == VTYPE_INT) {
		width = d->GetInt("width");
	}

	if (d->HasKey("color") && d->GetType("color") == VTYPE_STRING) {
		color = d->GetString("color").ToString();
	}

	if (d->HasKey("style") && d->GetType("style") == VTYPE_STRING) {
		style = d->GetString("style").ToString();
	}

	char widthStr[24];
	sprintf(widthStr, "%d", width);

	m_previewFrame->setStyleSheet(
		(std::string(
			 "QFrame#streamelements_central_widget_frame { border: ") +
		 std::string(widthStr) + std::string("px ") +
		 style + std::string(" ") + color +
		 std::string(
			 "; padding: 0; margin: 0; background-color: transparent; }"))
			.c_str());

	m_previewFrameSettings = d->Copy(true);

	m_previewFrameVisible = true;

	return true;
}

void StreamElementsNativeOBSControlsManager::SerializePreviewFrame(
	CefRefPtr<CefValue> &output)
{
	if (m_previewFrameVisible) {
		output->SetDictionary(m_previewFrameSettings->Copy(true));
	} else {
		output->SetNull();
	}
}

void StreamElementsNativeOBSControlsManager::HidePreviewFrame()
{
	m_previewFrameVisible = false;

	m_previewFrame->setStyleSheet(
		"QFrame#streamelements_central_widget_frame { border: none; padding: 0; margin: 0; background-color: transparent;");

	m_previewFrameSettings = CefDictionaryValue::Create();
}

bool StreamElementsNativeOBSControlsManager::DeserializePreviewTitleBar(
	CefRefPtr<CefValue> input)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	HidePreviewTitleBar();

	CefRefPtr<CefDictionaryValue> rootDictionary =
		input->GetDictionary();

	int height = 100;
	std::string url = "about:blank";
	std::string executeJavaScriptOnLoad = "";

	if (rootDictionary->HasKey("height")) {
		height = rootDictionary->GetInt("height");
	}

	if (rootDictionary->HasKey("url")) {
		url = rootDictionary->GetString("url").ToString();
	}

	if (rootDictionary->HasKey("executeJavaScriptOnLoad")) {
		executeJavaScriptOnLoad =
			rootDictionary
				->GetString("executeJavaScriptOnLoad")
				.ToString();
	}

	m_previewTitleBrowser = new StreamElementsBrowserWidget(
		nullptr, url.c_str(), executeJavaScriptOnLoad.c_str(),
		"reload", "obsPreviewAreaTitleBar", "");

	m_previewTitleBrowser->setSizePolicy(QSizePolicy(
		QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));

	m_previewTitleContainer->setFixedHeight(height);

	m_previewTitleLayout->addWidget(m_previewTitleBrowser);

	m_previewTitleContainer->show();

	m_previewTitleSettings = rootDictionary->Copy(true);

	return true;
}

void StreamElementsNativeOBSControlsManager::SerializePreviewTitleBar(
	CefRefPtr<CefValue> &output)
{
	if (m_previewTitleContainer->isVisible()) {
		output->SetDictionary(m_previewTitleSettings->Copy(true));
	} else {
		output->SetNull();
	}
}

void StreamElementsNativeOBSControlsManager::HidePreviewTitleBar()
{
	m_previewTitleContainer->hide();

	QApplication::sendPostedEvents();

	if (m_previewTitleBrowser) {
		m_previewTitleLayout->removeWidget(m_previewTitleBrowser);

		m_previewTitleBrowser->deleteLater();

		m_previewTitleBrowser = nullptr;
	}

	m_previewTitleSettings = CefDictionaryValue::Create();
}

void StreamElementsNativeOBSControlsManager::Reset()
{
	HidePreviewTitleBar();
	HidePreviewFrame();
}

void StreamElementsNativeOBSControlsManager::SetStreamingInitialState()
{
	if (!m_startStopStreamingButton) return;

	if (obs_frontend_streaming_active()) {
		SetStreamingActiveState();
	}
	else {
		SetStreamingStoppedState();
	}
}

void StreamElementsNativeOBSControlsManager::SetStreamingActiveState()
{
	if (!m_startStopStreamingButton) return;

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StopStreaming"));
		m_startStopStreamingButton->setEnabled(true);

		SetStreamingStyle(true);

		OnStartStopStreamingButtonUpdate();
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingStoppedState()
{
	if (!m_startStopStreamingButton) return;

	StopTimeoutTracker();

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StartStreaming"));
		m_startStopStreamingButton->setEnabled(true);

		SetStreamingStyle(false);

		OnStartStopStreamingButtonUpdate();
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingTransitionState()
{
	if (!m_startStopStreamingButton) return;

	if (obs_frontend_streaming_active()) {
		SetStreamingTransitionStoppingState();
	}
	else {
		SetStreamingTransitionStartingState();
	}
}

void StreamElementsNativeOBSControlsManager::SetStreamingTransitionStartingState()
{
	if (!m_startStopStreamingButton) return;

	StopTimeoutTracker();

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StartStreaming.InProgress"));
		m_startStopStreamingButton->setEnabled(false);

		SetStreamingStyle(true);

		OnStartStopStreamingButtonUpdate();
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingRequestedState()
{
	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StartStreaming.RequestInProgress"));
		m_startStopStreamingButton->setEnabled(false);

		SetStreamingStyle(true);

		OnStartStopStreamingButtonUpdate();

		StartTimeoutTracker();
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingTransitionStoppingState()
{
	if (!m_startStopStreamingButton) return;

	StopTimeoutTracker();

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StopStreaming.InProgress"));
		m_startStopStreamingButton->setEnabled(false);

		SetStreamingStyle(false);

		OnStartStopStreamingButtonUpdate();
	});
}

void StreamElementsNativeOBSControlsManager::OnStartStopStreamingButtonClicked()
{
	if (obs_frontend_streaming_active()) {
		blog(LOG_INFO, "obs-browser: streaming stop requested by UI control");

		// obs_frontend_streaming_stop();
		m_nativeStartStopStreamingButton->click();
	}
	else {
		blog(LOG_INFO, "obs-browser: streaming start requested by UI control");

		BeginStartStreaming();
	}
}

void StreamElementsNativeOBSControlsManager::OnStartStopStreamingButtonUpdate()
{
	QtExecSync([this]() -> void {
		m_startStopStreamingButton->setMenu(m_nativeStartStopStreamingButton->menu());
	});
}

void StreamElementsNativeOBSControlsManager::handle_obs_frontend_event(enum obs_frontend_event event, void* data)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		self->SetStreamingInitialState();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		self->SetStreamingTransitionStartingState();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		self->SetStreamingActiveState();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		self->SetStreamingTransitionStoppingState();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		self->SetStreamingStoppedState();
		break;
	}
}

void StreamElementsNativeOBSControlsManager::SetStreamingStyle(bool streaming)
{
	if (streaming) {
		m_startStopStreamingButton->setStyleSheet(QString(
			"QPushButton { background-color: #823929; color: #ffffff; font-weight:600; } "
			"QPushButton:hover { background-color: #dc6046; color: #000000; } "
			"QPushButton:pressed { background-color: #e68d7a; color: #000000; } "
			"QPushButton:disabled { background-color: #555555; color: #000000; } "
		));
	}
	else {
		m_startStopStreamingButton->setStyleSheet(QString(
			"QPushButton { background-color: #29826a; color: #ffffff; font-weight:600; } "
			"QPushButton:hover { background-color: #46dcb3; color: #000000; } "
			"QPushButton:pressed { background-color: #7ae6c8; color: #000000; } "
			"QPushButton:disabled { background-color: #555555; color: #000000; } "
		));
	}
}

static const char* HOTKEY_NAME_START_STREAMING = "OBSBasic.StartStreaming";

bool StreamElementsNativeOBSControlsManager::hotkey_enum_callback(void* data, obs_hotkey_id id, obs_hotkey_t* key)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	if (obs_hotkey_get_registerer_type(key) == OBS_HOTKEY_REGISTERER_FRONTEND) {
		if (strcmp(obs_hotkey_get_name(key), HOTKEY_NAME_START_STREAMING) == 0) {
			self->m_startStopStreamingHotkeyId = id;

			return false; // stop enumerating
		}
	}

	return true; // continue enumerating
}

void StreamElementsNativeOBSControlsManager::hotkey_routing_func(void* data, obs_hotkey_id id, bool pressed)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	if (id == self->m_startStopStreamingHotkeyId) {
		if (pressed &&
			!obs_frontend_streaming_active() &&
			self->m_startStopStreamingButton->isEnabled()) {
			blog(LOG_INFO, "obs-browser: streaming start requested by hotkey");

			self->BeginStartStreaming();
		}
	}
	else {
		QtPostTask(
			[id, pressed]() {
				obs_hotkey_trigger_routed_callback(id, pressed);
			});
	}
}

void StreamElementsNativeOBSControlsManager::hotkey_change_handler(void* data, calldata_t* param)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	auto key = static_cast<obs_hotkey_t*>(calldata_ptr(param, "key"));

	if (obs_hotkey_get_registerer_type(key) == OBS_HOTKEY_REGISTERER_FRONTEND) {
		if (strcmp(obs_hotkey_get_name(key), HOTKEY_NAME_START_STREAMING) == 0) {
			self->m_startStopStreamingHotkeyId = obs_hotkey_get_id(key);
		}
	}
}

void StreamElementsNativeOBSControlsManager::InitHotkeys()
{
	obs_enum_hotkeys(hotkey_enum_callback, this);

	signal_handler_connect(
		obs_get_signal_handler(),
		"hotkey_register",
		hotkey_change_handler,
		this
	);

	obs_hotkey_set_callback_routing_func(hotkey_routing_func, this);
	obs_hotkey_enable_callback_rerouting(true);
}

void StreamElementsNativeOBSControlsManager::ShutdownHotkeys()
{
	signal_handler_disconnect(
		obs_get_signal_handler(),
		"hotkey_register",
		hotkey_change_handler,
		this
	);
}

void StreamElementsNativeOBSControlsManager::BeginStartStreaming()
{
	switch (m_start_streaming_mode) {
	case start:
		// obs_frontend_streaming_start();
		m_nativeStartStopStreamingButton->click();
		break;

	case request:
		SetStreamingRequestedState();

		StreamElementsCefClient::DispatchJSEvent("hostStreamingStartRequested", "null");
		break;
	}
}

bool StreamElementsNativeOBSControlsManager::DeserializeStartStreamingUIHandlerProperties(CefRefPtr<CefValue> input)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d->HasKey("autoStart") && d->GetType("autoStart") == VTYPE_BOOL) {
		if (d->GetBool("autoStart")) {
			SetStartStreamingMode(start);
		}
		else {
			SetStartStreamingMode(request);
		}
	}

	if (d->HasKey("requestAcknowledgeTimeoutSeconds") && d->GetType("requestAcknowledgeTimeoutSeconds") == VTYPE_INT) {
		int v = d->GetInt("requestAcknowledgeTimeoutSeconds");

		if (v > 0) {
			SetStartStreamingAckTimeoutSeconds((int64_t)v);
		}
	}

	return true;
}

void StreamElementsNativeOBSControlsManager::StartTimeoutTracker()
{
	std::lock_guard<std::recursive_mutex> guard(m_timeoutTimerMutex);

	StopTimeoutTracker();

	m_timeoutTimer = new QTimer();
	m_timeoutTimer->moveToThread(qApp->thread());
	m_timeoutTimer->setSingleShot(true);

	QObject::connect(m_timeoutTimer, &QTimer::timeout, [this]() {
		std::lock_guard<std::recursive_mutex> guard(m_timeoutTimerMutex);

		m_timeoutTimer->deleteLater();
		m_timeoutTimer = nullptr;

		QMessageBox::warning(
			m_mainWindow,
			QString(obs_module_text("StreamElements.Action.StartStreaming.Timeout.Title")),
			QString(obs_module_text("StreamElements.Action.StartStreaming.Timeout.Text")));

		SetStreamingInitialState();

		// obs_frontend_streaming_start();
		m_nativeStartStopStreamingButton->click();
	});

	QMetaObject::invokeMethod(m_timeoutTimer, "start", Qt::QueuedConnection, Q_ARG(int, m_startStreamingRequestAcknowledgeTimeoutSeconds * 1000));
}

void StreamElementsNativeOBSControlsManager::StopTimeoutTracker()
{
	std::lock_guard<std::recursive_mutex> guard(m_timeoutTimerMutex);

	if (m_timeoutTimer) {
		if (QThread::currentThread() == qApp->thread()) {
			m_timeoutTimer->stop();
		}
		else {
			QMetaObject::invokeMethod(m_timeoutTimer, "stop", Qt::BlockingQueuedConnection);
		}

		m_timeoutTimer->deleteLater();
		m_timeoutTimer = nullptr;
	}
}

void StreamElementsNativeOBSControlsManager::AdviseRequestStartStreamingAccepted()
{
	StopTimeoutTracker();
}

void StreamElementsNativeOBSControlsManager::AdviseRequestStartStreamingRejected()
{
	StopTimeoutTracker();

	SetStreamingInitialState();
}
