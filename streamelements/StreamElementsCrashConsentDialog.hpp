#pragma once

#include <string>

//
// The crash-time consent prompt.
//
// BugSplat shows its own dialog before sending, asking the user what they were
// doing and letting them supply contact details. Sentry has no such UI -- it
// uploads silently -- so switching backends would quietly remove a prompt users
// have been seeing for years, and remove the report descriptions with it. This
// is that dialog, rebuilt.
//
// It is native Win32, built from an in-memory DLGTEMPLATE, and deliberately not
// Qt. The crash handler touches no Qt anywhere: by the time this runs the
// process is dying, the Qt event loop may be the very thing that is wedged, and
// QApplication is not safe to re-enter from an exception filter.
//
// Sentry's User Feedback API is not what carries the answers. sentry_capture_
// feedback() needs an event_id to associate with, and the event here is created
// downstream by sentry's own filter -- there is no id to hand back at this
// point. The answers instead go onto the scope before hand-off, as
// sentry_set_user() plus a `user_report` context, so they travel with the event
// that filter builds.
//
class StreamElementsCrashConsentDialog {
public:
	struct Result {
		// False when the user declined, closed the dialog, or the
		// dialog could not be shown at all. The caller must treat this
		// as "do not send": it is the user's answer, not a hint.
		bool consented = false;

		std::string description;
		std::string name;
		std::string email;
	};

public:
	// Shows the prompt modally on the calling thread, which is the crashing
	// thread. `name` and `email` prefill their fields; pass what was saved
	// from the last report.
	//
	// Returns consented=false if the dialog cannot be created, so a failure
	// here suppresses the report rather than silently sending one the user
	// never agreed to.
	static Result Prompt(const std::string &name, const std::string &email);
};
