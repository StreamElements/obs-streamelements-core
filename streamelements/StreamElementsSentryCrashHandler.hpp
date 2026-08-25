#pragma once

#include "StreamElementsCrashHandler.hpp"

//
// Sentry crash-reporting backend, built on sentry-native with
// SENTRY_BACKEND=native.
//
// The design is "arm, then hand off", but the hand-off is not what the original
// plan assumed, and the difference matters. Two things were established by
// reading the vendored SDK rather than its documentation:
//
// 1. sentry_handle_exception() does NOT produce the minidump under the `native`
//    backend. It processes the event -- applying scope metadata and running the
//    on_crash/before_send hooks -- and that is all. The minidump is written out
//    of process by the sentry-crash daemon, which is triggered by an IPC notify
//    that sentry's own exception filter performs *after* calling
//    sentry_handle_exception. Calling sentry_handle_exception ourselves and
//    stopping there would produce an event with no dump.
//
// 2. sentry's own filter never chains. It stores the filter it displaced and
//    only restores it at shutdown; on the crash path it returns
//    EXCEPTION_CONTINUE_SEARCH without calling it. Since Windows keeps exactly
//    one top-level filter, merely calling sentry_init() therefore severs the
//    chain and would silently stop OBS from writing its own crash log.
//
// So this handler owns the chain explicitly. It records the host filter (OBS's)
// *before* sentry_init installs its own, then installs its own filter last so
// that it runs first. On a crash it:
//
//   1. grows the stack for EXCEPTION_STACK_OVERFLOW,
//   2. walks the stack and applies the module-of-interest gate,
//   3. if the crash is ours, arms the Sentry scope with everything
//      StreamElementsCrashContext collected, then calls sentry's filter, which
//      emits the event and has the daemon write and upload the minidump,
//   4. calls the host filter either way, so OBS's crash log is still written.
//
// Suppression is therefore "skip sentry's filter", not "skip
// sentry_handle_exception" -- a crash whose stack never passed through our code
// goes straight from step 2 to step 4 and Sentry never hears about it.
//
// 3. sentry_init() installs TWO crash entry points on Windows, not one: the
//    top-level filter above, and a plain signal(SIGABRT, ...). Owning only the
//    first left abort() -- and therefore every _purecall, which is the whole
//    double-destruction family behind CORE-777 and CORE-786 -- going straight
//    to sentry with no gate, no consent prompt and no payload. So this handler
//    owns both doors, and both funnel into the same path. The only difference
//    is that the SIGABRT path captures its own CONTEXT, which puts our handler
//    on the stack and means the leading frames must be dropped before the gate
//    is applied, or the gate would pass for every abort in the process.
//    See CORE-860.
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
