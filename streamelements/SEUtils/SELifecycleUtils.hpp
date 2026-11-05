#pragma once

//
// Startup and teardown gating: whether libobs is up, whether it is safe to
// destroy UI, and the event-queue choke point.
//
// Nothing here names an obs or a Qt type, deliberately. These are the
// questions a translation unit asks before it does anything at all, and the
// ones that most want asking from code that has no business linking either
// library -- so the vocabulary stays on the .cpp side and the declarations
// are bools. SEDeleteDockWidgetWhenSafe, which used to sit here and was the
// only Qt in the file, is in SEDockUtils.hpp.
//

#include <string>

//
// Whether libobs has finished initializing: obs_initialized(), behind a
// declaration that does not need obs.h.
//
// It is the one obs call the API handler infrastructure makes, and a link
// seam for a test, which defines it and decides the answer -- which is what
// makes "a page's context was created before the plug-in was ready" an
// ordinary assertion rather than a startup race nobody can reproduce.
//
bool IsObsInitialized();

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

void RemoveObsDirtyShutdownMarker();

void RestartCurrentApplication();
