// CORE-4733 / SELIVE-4N: CEF browser close runs a nested Qt event loop.
// Dialog teardown must be safe when that loop re-enters close/accept/hide,
// and when Qt destroys the browser through its parent during the loop.
//
// This mirrors SEBrowserDialog::DestroyBrowser and QPointer semantics. The
// source invariant binds the model to the production implementation. Invalid
// calls are recorded rather than executed as real C++ undefined behaviour.

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;
int invalidDeletionCalls = 0;

void check(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		++failures;
	}
}

struct Browser {
	bool alive = true;
	bool cefDestroyed = false;
	int closeCount = 0;
	int deferredDeletes = 0;
	int immediateDeletes = 0;
	std::function<void()> duringClose;

	void DestroyBrowser()
	{
		if (cefDestroyed)
			return;

		cefDestroyed = true;
		++closeCount;

		if (duringClose)
			duringClose();
	}
};

// Every copy shares one slot, just as every QPointer to a QObject is nulled
// when that QObject is destroyed.
struct Tracked {
	std::shared_ptr<Browser *> slot;

	explicit operator bool() const { return slot && *slot; }
	Browser *operator->() const { return *slot; }
	Browser *get() const { return slot ? *slot : nullptr; }
};

std::vector<std::shared_ptr<Browser *>> trackedSlots;

Tracked MakeTracked(Browser *browser)
{
	auto slot = std::make_shared<Browser *>(browser);
	trackedSlots.push_back(slot);
	return Tracked{slot};
}

void DestroyThroughQt(Browser *browser)
{
	browser->alive = false;

	for (auto &slot : trackedSlots) {
		if (*slot == browser)
			*slot = nullptr;
	}
}

void DeleteLater(Browser *browser)
{
	if (!browser || !browser->alive) {
		++invalidDeletionCalls;
		return;
	}

	++browser->deferredDeletes;
}

void DeleteNow(Browser *browser)
{
	if (!browser || !browser->alive) {
		++invalidDeletionCalls;
		return;
	}

	++browser->immediateDeletes;
	DestroyThroughQt(browser);
}

struct FixedDialog {
	Tracked browser;
	bool ownerTeardown = false;

	void DestroyBrowser()
	{
		Tracked local = browser;
		browser = Tracked{};

		if (!local)
			return;

		local->DestroyBrowser();

		if (!local)
			return;

		if (ownerTeardown)
			DeleteNow(local.get());
		else
			DeleteLater(local.get());
	}
};

struct LegacyDialog {
	Browser *browser = nullptr;
	bool ownerTeardown = false;

	void DestroyBrowser()
	{
		if (!browser)
			return;

		browser->DestroyBrowser();

		if (ownerTeardown)
			DeleteNow(browser);
		else
			DeleteLater(browser);

		browser = nullptr;
	}
};

void Reset()
{
	invalidDeletionCalls = 0;
	trackedSlots.clear();
}

void ordinary_close_defers_exactly_one_deletion()
{
	Reset();
	Browser browser;
	FixedDialog dialog{MakeTracked(&browser)};

	dialog.DestroyBrowser();

	check(browser.closeCount == 1,
	      "ordinary close must close CEF exactly once");
	check(browser.deferredDeletes == 1,
	      "ordinary close must schedule exactly one deferred deletion");
	check(browser.immediateDeletes == 0,
	      "ordinary close must not take the owner-teardown branch");
	check(!dialog.browser, "the browser member must be cleared");
	check(invalidDeletionCalls == 0,
	      "ordinary close must not make an invalid deletion call");
}

void reentrant_close_paths_cannot_delete_the_browser_twice()
{
	const char *const reasons[] = {"closeEvent", "accepted", "finished",
				       "hideEvent"};

	for (const char *reason : reasons) {
		Reset();
		Browser browser;
		FixedDialog dialog{MakeTracked(&browser)};
		bool memberWasClearDuringClose = false;

		browser.duringClose = [&] {
			memberWasClearDuringClose = !dialog.browser;
			dialog.DestroyBrowser();
		};

		dialog.DestroyBrowser();

		check(memberWasClearDuringClose,
		      "the browser member must be cleared before CEF can re-enter");
		check(browser.closeCount == 1,
		      "re-entrant close must not close CEF twice");
		check(browser.deferredDeletes == 1,
		      "re-entrant close must schedule exactly one deletion");
		check(invalidDeletionCalls == 0, reason);
	}
}

void owner_teardown_still_deletes_immediately_once()
{
	Reset();
	Browser browser;
	FixedDialog dialog{MakeTracked(&browser), true};

	dialog.DestroyBrowser();

	check(browser.immediateDeletes == 1,
	      "owner teardown must delete immediately exactly once");
	check(browser.deferredDeletes == 0,
	      "owner teardown must not use deleteLater");
	check(invalidDeletionCalls == 0,
	      "owner teardown must not make an invalid deletion call");
}

void qt_destruction_during_cef_close_clears_every_tracked_handle()
{
	Reset();
	Browser browser;
	FixedDialog dialog{MakeTracked(&browser)};

	browser.duringClose = [&] { DestroyThroughQt(&browser); };
	dialog.DestroyBrowser();

	check(!dialog.browser,
	      "Qt destruction must clear the dialog's tracked member");
	check(browser.deferredDeletes == 0 && browser.immediateDeletes == 0,
	      "a browser Qt already destroyed must not be deleted again");
	check(invalidDeletionCalls == 0,
	      "the tracked local must suppress deletion after Qt destroys the browser");
}

void sequential_close_notifications_are_idempotent()
{
	Reset();
	Browser browser;
	FixedDialog dialog{MakeTracked(&browser)};

	for (int i = 0; i < 5; ++i)
		dialog.DestroyBrowser();

	check(browser.closeCount == 1 && browser.deferredDeletes == 1,
	      "any sequence of close notifications must close and delete once");
	check(invalidDeletionCalls == 0,
	      "sequential close notifications must not make invalid calls");
}

void the_old_late_clear_reproduces_the_reentrant_invalid_call()
{
	Reset();
	Browser browser;
	LegacyDialog dialog{&browser};

	browser.duringClose = [&] { dialog.DestroyBrowser(); };
	dialog.DestroyBrowser();

	check(browser.deferredDeletes == 1,
	      "the inner legacy close must schedule its deletion");
	check(invalidDeletionCalls == 1,
	      "non-vacuity: the outer legacy close must reuse the cleared member");
}

} // namespace

int main()
{
	ordinary_close_defers_exactly_one_deletion();
	reentrant_close_paths_cannot_delete_the_browser_twice();
	owner_teardown_still_deletes_immediately_once();
	qt_destruction_during_cef_close_clears_every_tracked_handle();
	sequential_close_notifications_are_idempotent();
	the_old_late_clear_reproduces_the_reentrant_invalid_call();

	if (failures) {
		std::fprintf(stderr, "%d assertion(s) failed\n", failures);
		return 1;
	}

	std::puts("test_browser_dialog_lifetime: all assertions hold");
	return 0;
}
