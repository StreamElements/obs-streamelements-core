#pragma once

//
// A handle a detached task can check to find out whether whoever started it
// has lost interest. One class per file, per the repo convention; it used to
// sit in the middle of SEUtils.hpp.
//

#include <functional>
#include <memory>
#include <mutex>
#include <thread>

class CancelableTask {
private:
	bool cancelled = false;
	std::recursive_mutex mutex;

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
	CancelableTask() : cancelled(false) {}

public:
	void Cancel() { cancelled = true; }

	bool IsCancelled() { return cancelled; }
};
