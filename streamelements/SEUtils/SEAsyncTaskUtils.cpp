#include "SEAsyncTaskUtils.hpp"

#include "SEDiagnosticsUtils.hpp"
#include "SELifecycleUtils.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <chrono>

static std::atomic<bool> s_shuttingDown(false);
bool IsShuttingDown()
{
	return s_shuttingDown.load();
}
void SetShuttingDown()
{
	s_shuttingDown.store(true);
}

std::future<void> __QtDelayTask_Impl(std::function<void()> task, int delayMs,
				     const char *file, const int line)
{
	auto promise = std::make_shared<std::promise<void>>();

	auto item = AsyncCallContextPush(file, line, false);

	QTimer::singleShot(std::chrono::milliseconds(delayMs), qApp, [=]() {
		// Same reasoning as __QtPostTask_Impl: cleanup must survive a
		// throwing task, or the context entry leaks into every later
		// crash report.
		auto finish = [=]() {
			AsyncCallContextRemove(item);

			promise->set_value();
		};

		// Same gate as __QtPostTask_Impl, and for the same reason
		// (CORE-862). A timer fires through the same event dispatcher,
		// so a Win32 modal loop delivers it just as readily.
		if (IsCrashReportingInProgress()) {
			finish();

			return;
		}

		item->running = true;

		try {
			task();
		} catch (...) {
			finish();

			throw;
		}

		finish();
	});

	return promise->get_future();
}
std::future<void> __QtPostTask_Impl(std::function<void()> task,
				    std::string file, int line)
{
	std::shared_ptr<std::promise<void>> promise =
		std::make_shared<std::promise<void>>();

	auto item = AsyncCallContextPush(file, line, false);

	auto executor = [=]() {
		// The entry has to come off the stack, and the waiter has to be
		// released, even if the task throws. Otherwise the entry stays
		// for the life of the process and appears in every later crash
		// report, and any QtExecSync blocked on this never learns it
		// finished.
		auto finish = [=]() {
			AsyncCallContextRemove(item);

			promise->set_value();
		};

		//
		// Nothing queued before a crash may run after it (CORE-862).
		//
		// The consent prompt is a native Win32 modal dialog, and every
		// Win32 modal loop calls DispatchMessage on all messages --
		// including the private one Qt's event dispatcher posts to its
		// own hidden window to drain the posted-event queue. So merely
		// putting the prompt up runs whatever was queued here, inside
		// the crash handler, on the crashing thread.
		//
		// When one of those tasks calls QDialog::exec(), its nested
		// event loop sits on top of the prompt's loop and the crash
		// path cannot continue until that unrelated dialog is answered
		// -- which the user has no reason to connect to the crash
		// prompt, and may never see behind a topmost window. Observed:
		// the prompt was answered and hidden while DialogBox2 was still
		// on the stack, unable to return.
		//
		// finish() still runs. The point is not to strand anyone: a
		// QtExecSync caller is blocked in result.wait() on another
		// thread, and dropping the task without releasing it would wedge
		// that thread too.
		//
		// This is the same single-choke-point shape as
		// SEDrainEventQueue() (CORE-786), and it covers only OUR queued
		// work -- OBS's own posted calls and other plug-ins' can still
		// open a dialog inside the prompt.
		//
		if (IsCrashReportingInProgress()) {
			finish();

			return;
		}

		// Before the task, not after.
		//
		// This flag is what tells a crash report which queued call was
		// executing at the moment of the fault -- the crash handler
		// writes it into async-context.json. Setting it afterwards meant
		// the one task that actually was running reported running=false,
		// which is precisely backwards, and on the macro used at most of
		// the call sites in this codebase. QtDelayTask below has always
		// had it the right way round.
		//
		// Below the gate above, so a task that was dropped is not
		// reported as having been running.
		item->running = true;

		try {
			task();
		} catch (...) {
			finish();

			throw;
		}

		finish();
	};

	QMetaObject::invokeMethod(qApp, executor, Qt::QueuedConnection);

	return promise->get_future();
}
std::future<void> __QtExecSync_Impl(std::function<void()> task,
				    std::string file, int line)
{
	if (QThread::currentThread() == qApp->thread()) {
		// Same gate as the queued path (CORE-862), so the contract is
		// the same whichever thread asks: once crash reporting has
		// begun, a marshalled task does not run. This branch bypasses
		// the queue entirely, so it needs its own check -- and the
		// crashing thread is usually this one.
		if (!IsCrashReportingInProgress())
			task();

		std::promise<void> promise;
		promise.set_value();
		return promise.get_future();
	} else {
		// Same contract as the crash gate above, for the same reason:
		// once the Qt thread has left its event loop the task will never
		// run, so waiting for it is waiting forever.
		//
		// This is not hypothetical. The updater's shutdown blocks the Qt
		// thread until an in-flight update check finishes, and that check
		// finishes by calling QtExecSync -- each waiting on the other, a
		// deadlock caught in a live dump with both halves visible
		// (CORE-1131).
		if (IsShuttingDown()) {
			std::promise<void> promise;
			promise.set_value();
			return promise.get_future();
		}

		//
		// A slot shared with the posted task, so that abandoning the
		// wait below is safe.
		//
		// QtExecSync callers legitimately capture by reference -- the
		// call is synchronous by contract -- so a task left queued after
		// this frame unwinds would run against dangling references.
		// Claiming the slot under its mutex leaves only two cases: the
		// task has not started, and now never will, or it has already
		// finished. There is no third.
		//
		struct Slot {
			std::mutex mutex;
			bool abandoned = false;
		};

		auto slot = std::make_shared<Slot>();

		std::future<void> result = __QtPostTask_Impl(
			[slot, task]() {
				std::lock_guard<std::mutex> lock(slot->mutex);

				if (!slot->abandoned)
					task();
			},
			file, line);

		//
		// Wait in slices rather than in one shot.
		//
		// The gate above only covers a caller that arrives after
		// shutdown has begun. One already parked here when it begins
		// would go on waiting for a Qt thread that has left its event
		// loop -- and that is the common case, because the deadlock
		// exists precisely when the call was already in flight. The
		// entry check alone moved CORE-1193 five seconds later instead
		// of removing it: the updater's wait timed out, shutdown
		// advanced to ~NetworkDialog, and that joined the same worker
		// still parked right here.
		//
		while (result.wait_for(std::chrono::milliseconds(50)) !=
		       std::future_status::ready) {
			if (!IsShuttingDown())
				continue;

			std::lock_guard<std::mutex> lock(slot->mutex);

			slot->abandoned = true;

			std::promise<void> promise;
			promise.set_value();
			return promise.get_future();
		}

		return result;
	}
}
