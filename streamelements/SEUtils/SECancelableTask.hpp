#pragma once

//
// A handle a detached task can check to find out whether whoever started it
// has lost interest. One class per file, per the repo convention; it used to
// sit in the middle of SEUtils.hpp.
//

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

class CancelableTask {
private:
	//
	// Atomic, because the two sides of this class are always on different
	// threads: Execute() detaches, so the task calls IsCancelled() on its
	// own thread while whoever started it calls Cancel() on theirs. It
	// was a plain bool, which is a data race by construction -- and the
	// std::recursive_mutex that used to sit beside it was never locked by
	// anything, so the class read as synchronised while being nothing of
	// the kind.
	//
	// Relaxed would do, since cancellation carries no data with it and a
	// late observation only means one more iteration of work. Sequential
	// consistency is the default and costs nothing measurable against a
	// task that is doing HTTP, so it stays.
	//
	std::atomic<bool> cancelled{false};

public:
	static std::shared_ptr<CancelableTask>
	Execute(std::function<void(std::shared_ptr<CancelableTask>)> task)
	{
		auto handle = std::make_shared<CancelableTask>();

		std::thread thread([handle, task]() { task(handle); });

		thread.detach();

		return handle;
	}

public:
	CancelableTask() = default;

public:
	void Cancel() { cancelled = true; }

	bool IsCancelled() const { return cancelled; }
};
