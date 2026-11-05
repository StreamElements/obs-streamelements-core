#include "SEDockUtils.hpp"

#include "SELifecycleUtils.hpp"
#include "SEDiagnosticsUtils.hpp"

#include <util/base.h>

void SEDeleteDockWidgetWhenSafe(QPointer<QDockWidget> dock, const char *id,
				bool useDeleteLater)
{
	if (!dock)
		return;

	if (!SEIsUiTeardownSafe()) {
		blog(LOG_WARNING,
		     "[obs-streamelements-core]: leaking dock widget '%s': OBS was asked to close before initialization completed, deleting it now would corrupt the widget tree",
		     id ? id : "(unnamed)");

		// Detach from the main window so OBS's own teardown does not
		// walk into it later.
		dock->setParent(nullptr);

		return;
	}

	//
	// Out of the paint tree before it is destroyed.
	//
	// QMainWindow::removeDockWidget() hides a dock but leaves it parented,
	// so Qt can still walk into it from a repaint while something below is
	// destroying it -- and destroying our browser widget spins a nested
	// event loop inside obs-browser's closeBrowser(), which is exactly when
	// such a repaint runs. A paint that reaches a half-destroyed widget
	// calls a virtual that is pure in the base it has been demoted to, and
	// the process aborts in _purecall (CORE-1922, SELIVE-8G; CORE-777 is
	// the same mechanism from the event-queue side).
	//
	// Cheap and unconditional: a dock on its way out has nothing to paint.
	//
	dock->hide();
	dock->setParent(nullptr);

	if (useDeleteLater) {
		// The destructor runs later, from the event loop, so the scope
		// marker belongs to whatever destroys it then -- our widget's
		// own destructor carries one.
		dock->deleteLater();
	} else {
		SEWidgetTeardownScope marker("dock", id);

		delete dock.data();
	}
}
