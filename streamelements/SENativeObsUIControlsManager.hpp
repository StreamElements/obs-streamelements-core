#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QObject>
#include <QTimer>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>

#include "obs.h"
#include "obs-hotkey.h"
#include "obs-frontend-api.h"

#include <mutex>
#include <algorithm>

#include "cef-headers.hpp"
#include "ISENativeObsUIControlsManager.hpp"

#include "SEApiServices.hpp"

#include "SEBrowserWidget.hpp"

//
// SE_ENABLE_CENTRAL_WIDGET_DECORATIONS is defined by the interface
// above, so that the handler bodies guarded on it see the same switch
// without including this header.
//

class SEHostStreamingManager;

//
// QObject first among the bases, which moc requires.
//
class SENativeObsUIControlsManager : public QObject,
				     public ISENativeObsUIControlsManager {
	Q_OBJECT

private:
	struct Private {
		explicit Private() = default;
	};

private:
	class WidgetVisibilityChangeTracker : public QObject {
	public:
		typedef std::function<void(bool)> callback_t;

	private:
		QObject *m_target;
		callback_t m_callback;

	public:
		WidgetVisibilityChangeTracker(QObject *target,
					      callback_t callback)
			: m_target(target), m_callback(callback)
		{
			m_target->installEventFilter(this);
		}

		~WidgetVisibilityChangeTracker()
		{
			if (!m_target) {
				m_target->removeEventFilter(this);
			}
		}

		virtual bool eventFilter(QObject *o, QEvent *e) override
		{
			if (o == m_target) {
				switch (e->type()) {
				case QEvent::Hide:
					m_callback(false);
					break;

				case QEvent::Show:
					m_callback(true);
					break;

				case QEvent::Destroy:
					m_target->removeEventFilter(this);
					m_target = nullptr;
					break;
				default:
					break;
				}
			}

			return QObject::eventFilter(o, e);
		}
	};

public:
	enum start_streaming_mode_t {
		start = 0,
		request = 1
	};

public:
	//
	// Owned by SEGlobalStateManager and published in SEApiServices. It
	// was a singleton with a file-static instance, which is why the root
	// had to call Destroy() on it by name at teardown (CORE-757).
	//
	static std::shared_ptr<SENativeObsUIControlsManager>
	Create(QMainWindow *mainWindow,
	       std::shared_ptr<SEHostStreamingManager> hostStreamingManager);

	SENativeObsUIControlsManager(
		Private, QMainWindow *mainWindow,
		std::shared_ptr<SEHostStreamingManager> hostStreamingManager);
	virtual ~SENativeObsUIControlsManager();

public:
	QMainWindow* mainWindow() { return m_mainWindow; }

private:
	//
	// OBS's own single stream, as a service rather than as four direct
	// calls to obs_frontend_streaming_*. The dependency runs this way
	// round -- a UI class asking a service for the host's state --
	// because the reverse, a service reaching into a Qt class to start a
	// stream, is what this replaces (CORE-757).
	//
	std::shared_ptr<SEHostStreamingManager> m_hostStreamingManager;

public:
	//
	// A setter rather than a constructor argument because this class hands
	// out its own instance through GetInstance(), so Initialize() cannot
	// get a word in while it is being built. It is called there, right
	// after that instance is obtained, and the only thing it feeds is the
	// preview title bar browser, which is created later on demand.
	//
	void SetApiServices(std::shared_ptr<SEApiServices> apiServices)
	{
		m_apiServices = apiServices;
	}

	void SetStartStreamingMode(start_streaming_mode_t mode) { m_start_streaming_mode = mode; }
	start_streaming_mode_t GetStartStreamingMode() { return m_start_streaming_mode; }

	bool GetNativeManageBroadcastButtonVisibility() override
	{
		if (!m_nativeManageBroadcastButton)
			return false;

		return m_nativeManageBroadcastButton->isVisible();
	}

	void SetStartStreamingAckTimeoutSeconds(int seconds) { m_startStreamingRequestAcknowledgeTimeoutSeconds = std::max<int>(seconds, 1); }
	int GetStartStreamingAckTimeoutSeconds() { return m_startStreamingRequestAcknowledgeTimeoutSeconds; }

	bool DeserializeStartStreamingUIHandlerProperties(
		CefRefPtr<CefValue> input) override;

	void AdviseRequestStartStreamingAccepted() override;
	void AdviseRequestStartStreamingRejected() override;

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	bool DeserializePreviewFrame(CefRefPtr<CefValue> input) override;
	void SerializePreviewFrame(CefRefPtr<CefValue>& output);
	void HidePreviewFrame() override;
	bool DeserializePreviewTitleBar(CefRefPtr<CefValue> input) override;
	void SerializePreviewTitleBar(CefRefPtr<CefValue>& output);
	void HidePreviewTitleBar() override;
#endif

	void Reset() override;

public:
	void StartStreaming() override;

private:
	void SetStreamingInitialState();
	void SetStreamingActiveState();
	void SetStreamingStoppedState();
	void SetStreamingTransitionState();
	void SetStreamingTransitionStartingState();
	void SetStreamingTransitionStoppingState();
	void SetStreamingRequestedState();

	void SetStreamingStyle(bool streaming);

	void InitHotkeys();
	void ShutdownHotkeys();

	void StartTimeoutTracker();
	void StopTimeoutTracker();

protected:
	void ClickNativeStartStopStreamingButton();
	void BeginStartStreaming();

private slots:
	void OnStartStopStreamingButtonClicked();
	void OnStartStopStreamingButtonUpdate();

private:
	static void handle_obs_frontend_event(enum obs_frontend_event event, void* data);

	static bool hotkey_enum_callback(void* data, obs_hotkey_id id, obs_hotkey_t* key);
	static void hotkey_change_handler(void* data, calldata_t* param);
	static void hotkey_routing_func(void* data, obs_hotkey_id id, bool pressed);

private:
	QPointer<QWidget> m_nativeCentralWidget = nullptr;
	QPointer<QMainWindow> m_mainWindow = nullptr;
	QPointer<QPushButton> m_startStopStreamingButton = nullptr;
	QPointer<QPushButton> m_nativeStartStopStreamingButton = nullptr;
	std::shared_ptr<WidgetVisibilityChangeTracker>
		m_nativeManageBroadcastButtonVisibilityChangeTracker = nullptr;
	QPointer<QPushButton> m_nativeManageBroadcastButton = nullptr;
	obs_hotkey_id m_startStopStreamingHotkeyId = OBS_INVALID_HOTKEY_ID;
	start_streaming_mode_t m_start_streaming_mode = start;
	int m_startStreamingRequestAcknowledgeTimeoutSeconds = 5;
	QPointer<QTimer> m_timeoutTimer = nullptr;
	std::recursive_mutex m_timeoutTimerMutex;

	bool m_isStreamingTransitionState = false;

	//
	// Weak. Everything this class builds from the bundle -- browser
	// widgets and API message handlers -- holds a strong reference to it,
	// and this class destroys them, so a strong reference here closes a
	// ring the refcount can never unwind. See the comment in
	// SEApiServices.hpp.
	//
	std::weak_ptr<SEApiServices> m_apiServices;

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	QPointer<QFrame> m_previewFrame = nullptr;
	QPointer<QVBoxLayout> m_previewFrameLayout = nullptr;
	QPointer<QLayout> m_nativePreviewLayout = nullptr;
	QPointer<QLayout> m_nativePreviewLayoutParent = nullptr;
	QPointer<QWidget> m_nativePreviewWidget = nullptr;
	bool m_previewFrameVisible = false;
	CefRefPtr<CefDictionaryValue> m_previewFrameSettings =
		CefDictionaryValue::Create();

	QPointer<QWidget> m_previewTitleContainer = nullptr;
	QPointer<QHBoxLayout> m_previewTitleLayout = nullptr;
	QPointer<SEBrowserWidget> m_previewTitleBrowser = nullptr;
	CefRefPtr<CefDictionaryValue> m_previewTitleSettings =
		CefDictionaryValue::Create();
	#endif
};
