#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QObject>
#include <QTimer>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "obs.h"
#include "obs-hotkey.h"
#include "obs-frontend-api.h"

#include <mutex>
#include <algorithm>

#include "cef-headers.hpp"

#include "StreamElementsBrowserWidget.hpp"

class StreamElementsNativeOBSControlsManager : public QObject
{
	Q_OBJECT

public:
	enum start_streaming_mode_t {
		start = 0,
		request = 1
	};

public:
	static StreamElementsNativeOBSControlsManager* GetInstance();

private:
	StreamElementsNativeOBSControlsManager(QMainWindow* mainWindow);
	virtual ~StreamElementsNativeOBSControlsManager();

public:
	QMainWindow* mainWindow() { return m_mainWindow; }

	void SetStartStreamingMode(start_streaming_mode_t mode) { m_start_streaming_mode = mode; }
	start_streaming_mode_t GetStartStreamingMode() { return m_start_streaming_mode; }

	void SetStartStreamingAckTimeoutSeconds(int seconds) { m_startStreamingRequestAcknowledgeTimeoutSeconds = std::max<int>(seconds, 1); }
	int GetStartStreamingAckTimeoutSeconds() { return m_startStreamingRequestAcknowledgeTimeoutSeconds; }

	bool DeserializeStartStreamingUIHandlerProperties(CefRefPtr<CefValue> input);

	void AdviseRequestStartStreamingAccepted();
	void AdviseRequestStartStreamingRejected();

	bool DeserializePreviewFrame(CefRefPtr<CefValue> input);
	void SerializePreviewFrame(CefRefPtr<CefValue>& output);
	void HidePreviewFrame();
	bool DeserializePreviewTitleBar(CefRefPtr<CefValue> input);
	void SerializePreviewTitleBar(CefRefPtr<CefValue>& output);
	void HidePreviewTitleBar();

	void Reset();

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
	QWidget *m_nativeCentralWidget = nullptr;
	QMainWindow* m_mainWindow = nullptr;
	QPushButton* m_startStopStreamingButton = nullptr;
	QPushButton* m_nativeStartStopStreamingButton = nullptr;
	obs_hotkey_id m_startStopStreamingHotkeyId = OBS_INVALID_HOTKEY_ID;
	start_streaming_mode_t m_start_streaming_mode = start;
	int m_startStreamingRequestAcknowledgeTimeoutSeconds = 5;
	QTimer* m_timeoutTimer = nullptr;
	std::recursive_mutex m_timeoutTimerMutex;

	QFrame *m_previewFrame = nullptr;
	QVBoxLayout *m_previewFrameLayout = nullptr;
	QLayout *m_nativePreviewLayout = nullptr;
	QLayout *m_nativePreviewLayoutParent = nullptr;
	QWidget *m_nativePreviewWidget = nullptr;
	bool m_previewFrameVisible = false;
	CefRefPtr<CefDictionaryValue> m_previewFrameSettings =
		CefDictionaryValue::Create();

	QWidget *m_previewTitleContainer = nullptr;
	QHBoxLayout *m_previewTitleLayout = nullptr;
	StreamElementsBrowserWidget *m_previewTitleBrowser = nullptr;
	CefRefPtr<CefDictionaryValue> m_previewTitleSettings =
		CefDictionaryValue::Create();
};
