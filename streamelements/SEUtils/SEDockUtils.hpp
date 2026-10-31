#pragma once

//
// Destroying a dock widget, which is never just `delete`.
//
// One function, and it has its own file for a reason that is easy to undo
// by accident: naming QPointer<QDockWidget> in a declaration puts <QPointer>
// and <QDockWidget> into every translation unit that includes the header.
// It used to live beside the teardown gates in SELifecycleUtils.hpp, and it
// was the only thing in that file that touched Qt -- so the gates, which are
// three bools and a drain, could not be consulted from anywhere that wanted
// to stay Qt-free. They can now.
//

#include <QDockWidget>
#include <QPointer>

//
// The only sanctioned way to destroy a QDockWidget. Deletes it when the UI
// teardown gate in SELifecycleUtils.hpp is open; otherwise detaches it from
// OBS's widget tree and leaks it, loudly. Destroying a dock while OBSInit()
// is still on the stack corrupts the tree, and by then
// obs_frontend_get_main_window() may already be null.
//
void SEDeleteDockWidgetWhenSafe(QPointer<QDockWidget> dock, const char *id,
				bool useDeleteLater);
