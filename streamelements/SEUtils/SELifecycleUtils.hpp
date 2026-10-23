#pragma once

//
// Startup and teardown gating: whether it is safe to destroy UI, the one
// sanctioned way to delete a dock, the event-queue choke point, and the
// please-wait transaction that defers OBS scene-collection saves.
//

#include <QDockWidget>
#include <QPointer>

#include <string>

//
// UI teardown gate (CORE-786). Defined in obs-streamelements-core-plugin.cpp,
// which is where the detection lives; see the long comment there.
//
// False once OBS has been asked to close before our Initialize() completed --
// the update-thread race. In that state our object graph is half-built and
// destroying any of it corrupts the widget tree, so every UI teardown site
// must check this first.
//
bool SEIsUiTeardownSafe();

// Called at the end of Initialize(). After this, an ordinary close is an
// ordinary close and teardown proceeds normally.
void SENoteInitializeCompleted();

// False while Initialize() is on the stack, and after a close has been caught.
// Defined in obs-streamelements-core-plugin.cpp.
bool SEIsEventPumpAllowed();

// Use instead of QApplication::sendPostedEvents() everywhere: one choke point
// for every event pump the plug-in runs.
void SEDrainEventQueue();

// The only sanctioned way to destroy a QDockWidget. Deletes it when the gate
// above is open; otherwise detaches it from OBS's widget tree and leaks it,
// loudly. Destroying a dock while OBSInit() is still on the stack corrupts
// the tree, and by then obs_frontend_get_main_window() may already be null.
void SEDeleteDockWidgetWhenSafe(QPointer<QDockWidget> dock, const char *id,
				bool useDeleteLater);

void RemoveObsDirtyShutdownMarker();

void RestartCurrentApplication();
