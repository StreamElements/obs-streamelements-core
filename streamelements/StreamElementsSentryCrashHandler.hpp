#pragma once

#include "StreamElementsCrashHandler.hpp"

//
// Sentry crash-reporting backend, built on sentry-native with SENTRY_BACKEND=native.
//
// The design is "arm, then hand off": the existing exception filter keeps its
// job -- growing the stack for EXCEPTION_STACK_OVERFLOW, walking the stack,
// applying the module-of-interest gate -- and once it has decided the crash is
// ours, it puts the collected data on the Sentry scope and calls
// sentry_handle_exception(). The backend then produces and uploads the minidump
// out of process, and control still chains on to the previous filter so OBS's
// own crash log is written.
//
// See CORE-554 for the full design, including why the `native` backend rather
// than crashpad.
//
class StreamElementsSentryCrashHandler : public StreamElementsCrashHandler {
public:
	StreamElementsSentryCrashHandler();
	virtual ~StreamElementsSentryCrashHandler();

public:
	virtual void StopAsyncHangDetection() override;
};
