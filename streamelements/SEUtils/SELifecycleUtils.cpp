#include "SELifecycleUtils.hpp"

#include "SEDiagnosticsUtils.hpp"
#include "SEStringUtils.hpp"
#include "SEPathUtils.hpp"


#include <obs-frontend-api.h>
#include <obs-module.h>

#include <util/platform.h>

#include <QApplication>
#include <QObject>
#include <QTimer>
#include <QMainWindow>
#include <QProcess>

#include <map>
#include <mutex>

bool IsObsInitialized()
{
	return obs_initialized();
}
void SEDrainEventQueue()
{
	// Not while Initialize() is on the stack -- pumping there delivers the
	// update thread's queued close() and nests OBS's EXIT dispatch inside
	// its FINISHED_LOADING dispatch -- and not once a close has been caught,
	// when every further dispatch feeds a world being torn down (CORE-786).
	if (!SEIsEventPumpAllowed())
		return;

	QApplication::sendPostedEvents();
}
void RemoveObsDirtyShutdownMarker()
{
	char *safeModeMarkerPathPtr =
		os_get_config_path_ptr("obs-studio/safe_mode");
	if (safeModeMarkerPathPtr) {
		// We'll remove this flag so the next time OBS starts, it won't alert that it was terminated abnormally
		os_unlink(safeModeMarkerPathPtr);
	}
	bfree(safeModeMarkerPathPtr);
}
#ifdef WIN32
//
// Windows-only body; the macOS half is in SEUtils.mm. The guard came
// with it out of the single translation unit.
//
void RestartCurrentApplication()
{
	bool success = false;

	QProcess proc;
	if (proc.startDetached(
		    QCoreApplication::instance()->applicationFilePath(),
		    QCoreApplication::instance()->arguments())) {
		success = true;

		/* Exit OBS */

		/* This is not the nicest way to terminate our own process,
			* yet, given that we are not looking for a clean shutdown
			* but will rather overwrite settings files, this is
			* acceptable.
			*
			* It is also likely to overcome any shutdown issues OBS
			* might have, and which appear from time to time. We definitely
			* do NOT want those attributed to Cloud Restore.
			*/

		::TerminateProcess(GetCurrentProcess(), 0);
		//::exit(0);
		//QApplication::quit();
		//auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());

		//mainWindow->close();
	}
}
#endif /* WIN32 */
