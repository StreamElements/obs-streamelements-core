#pragma once

#include <string>
#include <vector>

//
// Backend-agnostic crash-time collection.
//
// Everything gathered when the process is dying -- the OBS configuration zip,
// the window screenshot, the walked stack, the async-call and API-call context,
// the system and performance snapshots -- is identical whichever reporting
// backend is compiled in. Only the submission differs: BugSplat wants
// setAttribute/setNotes/sendAdditionalFile, Sentry wants
// sentry_set_tag/sentry_set_context/sentry_attach_file.
//
// So Collect() gathers, and returns a plain result the handler submits in its
// own vocabulary. Extracted from what used to be AddObsConfigurationFiles() in
// StreamElementsBugSplatCrashHandler.cpp, which called into the BugSplat SDK
// inline as it built things.
//
// Strings are UTF-8, deliberately, even though the Windows implementation works
// in wide characters throughout and BugSplat's API takes wchar_t. Converting at
// the submission boundary keeps this type free of a platform flavour it would
// otherwise impose on the macOS handler and on Sentry, whose API is char* UTF-8.
//
// This object is constructed once, at handler construction -- not at crash time
// -- because building it fetches the remote module-of-interest list over HTTP,
// which is not something to attempt on a dying process.
//
// Windows only for now. The macOS BugSplat integration collects nothing beyond
// a single "product" attribute it sets at startup, so there is no macOS
// implementation to extract yet and none is compiled; the Sentry handler will
// need one when it starts attaching data on that platform.
//
class StreamElementsCrashContext {
public:
	struct Attribute {
		std::string name;
		std::string value;
	};

	// A file to be uploaded alongside the report. `path` is an absolute path
	// to a temporary file this class created.
	struct Attachment {
		std::string path;
	};

	struct Result {
		std::vector<Attribute> attributes;
		std::vector<Attachment> attachments;

		// The async-call context, formatted. BugSplat puts this in
		// setNotes(); Sentry will put it in a structured context
		// instead. Empty when there was no async call in flight.
		std::string notes;
	};

public:
	StreamElementsCrashContext();
	~StreamElementsCrashContext();

	StreamElementsCrashContext(const StreamElementsCrashContext &) = delete;
	StreamElementsCrashContext &
	operator=(const StreamElementsCrashContext &) = delete;

public:
	// Walks the crashing thread's stack, which both produces the stack trace
	// and decides the module-of-interest verdict below. Call from the
	// exception filter, before ShouldReport() and Collect().
	//
	// `contextRecord` is a PCONTEXT. Typed as void* so the header does not
	// drag <windows.h> into every translation unit that includes it.
	void WalkStack(void *contextRecord);

	// The module-of-interest verdict: false means the crashing stack never
	// passed through our code and the report must be suppressed entirely.
	// This is the gate that keeps other plugins' crashes out, and it is a
	// decision, not a hint -- a handler that ignores it changes what the
	// product reports on.
	//
	// False until WalkStack() has run.
	bool ShouldReport() const;

	// Called from the exception path with the process already dying. Does no
	// allocation it can avoid, touches no Qt, and must not throw.
	Result Collect();

private:
	struct Impl;

	// Raw, and never freed on the crash path: the stack walker must outlive
	// any exception filter that could still be running.
	Impl *m_impl = nullptr;
};
