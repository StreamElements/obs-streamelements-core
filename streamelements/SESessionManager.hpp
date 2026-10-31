#pragma once

#include <memory>

class QMainWindow;
struct QCefCookieManager;
struct SEApiServices;

//
// The SE.Live login session, and the on-boarding UI that is the signed-out
// state of it.
//
// These five methods were on SEGlobalStateManager and they are one state
// machine: Reset() announces hostStateReset, optionally clears the cookies
// and resets the menu, then starts the on-boarding UI, which pushes the
// on-boarding URL into the central widget, drops every dock, worker and
// managed hotkey binding, and sets STARTUP_FLAGS_ONBOARDING_MODE -- which
// also clears STARTUP_FLAGS_SIGNED_IN. SwitchToOBSStudio() is the way back,
// behind a confirmation.
//
class SESessionManager {
public:
	//
	// Default is a plain reset; OnBoarding and Import each add a query
	// parameter to the on-boarding URL so the page knows which flow it is
	// showing.
	//
	enum UiModifier { Default = 0, OnBoarding = 1, Import = 2 };

	//
	// The cookie manager is borrowed: SEGlobalStateManager creates it from
	// the browser panel and deletes it in Shutdown(), which can run before
	// this object is destroyed, so it calls ForgetCookieManager() first.
	//
	// The services bundle is held weakly and locked per call. This is the
	// second place whose dependency set genuinely is most of the surface
	// -- a session reset touches the widgets, workers, hotkeys, scenes,
	// outputs, compositions, the menu and the persisted state -- and the
	// alternative is a twelve-parameter constructor that has to be
	// rewritten every time the reset grows a step. Each public method
	// below bails with a warning if the bundle is gone, which is also
	// what makes those methods callable in a test with a hand-built
	// bundle.
	//
	SESessionManager(QMainWindow *mainWindow,
			 QCefCookieManager *cefCookieManager,
			 std::shared_ptr<SEApiServices> apiServices);
	~SESessionManager();

	void Reset(bool deleteAllCookies = true,
		   UiModifier uiModifier = Default);
	void DeleteCookies();

	void StartOnBoardingUI(UiModifier uiModifier);
	void StopOnBoardingUI();

	//
	// Has no callers today. Moved as-is with the rest of the cluster
	// rather than left behind on the root, where it would have been the
	// only reason those five methods could not travel together.
	//
	void SwitchToOBSStudio();

	//
	// Called by SEGlobalStateManager::Shutdown() immediately before it
	// deletes the cookie manager this object borrows.
	//
	void ForgetCookieManager() { m_cefCookieManager = nullptr; }

private:
	QMainWindow *m_mainWindow = nullptr;
	QCefCookieManager *m_cefCookieManager = nullptr;

	std::weak_ptr<SEApiServices> m_apiServices;
};
