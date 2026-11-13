// A queued task must not call a virtual on a widget Qt has already destroyed.
//
// SEPleaseWaitWindow is dual-owned, and its own Create() says so: Qt owns it
// through the parent widget, the services bundle owns a shared_ptr, and either
// can get there first. Create() guards its DELETER with a QPointer for exactly
// that reason.
//
// Show() and Hide() did not. Both posted a lambda capturing a raw `this`:
//
//     QtPostTask([this]() { this->hide(); });
//
// open() and hide() both reach QWidget::setVisible, which is virtual. A queued
// call that lands after destruction dispatches through a vtable that
// destruction has already rewound, and the CRT answers with _purecall ->
// abort. On Windows that is a process kill, and it is reported as a crash in
// the CRT rather than here.
//
// It is the exact shape of SELIVE-3J, whose innermost real frames are
// QMetaCallEvent::placeMetaCall -> the lambda -> QWidget::setVisible ->
// QLayout::activate -> _purecall. Over 90 days crash.kind:purecall is 1,668
// events across 1,288 machines, and this is one contributor to it.
//
// Two idioms in the tree already handle the hazard -- a QPointer capture, and
// SEVideoCompositionViewWidget's registry test. SEPleaseWaitWindow had
// neither. This models the QPointer one.
//
// Mirrors SEPleaseWaitWindow::Show/Hide and the Qt semantics they depend on.
// The real ones need Qt; the semantics under test are reproduced here.

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

/* ================================================================= */

// How many times a virtual call reached an object that was already destroyed.
// In the real process each one of these is a _purecall and a dead OBS.
static int g_virtualCallsOnDestroyedObject = 0;

static int g_opens = 0;
static int g_hides = 0;

// Minimal stand-in for the widget. `alive` is what a real vtable rewind makes
// observable: calling a virtual after destruction is undefined, and on MSVC it
// lands in _purecall.
struct Widget {
	bool alive = true;

	void open()
	{
		if (!alive) {
			++g_virtualCallsOnDestroyedObject;
			return;
		}
		++g_opens;
	}

	void hide()
	{
		if (!alive) {
			++g_virtualCallsOnDestroyedObject;
			return;
		}
		++g_hides;
	}
};

// Stand-in for QPointer: Qt zeroes every one of them when the object is
// destroyed. That is the whole property the fix relies on, so it is modelled
// explicitly rather than assumed.
// Every copy shares one slot. That is the property that matters and the one a
// naive stand-in gets wrong: a lambda captures the handle BY VALUE, so if only
// the original were cleared the copy would still be a raw pointer and the
// model would quietly stop modelling QPointer. Qt registers each QPointer with
// the object, so a copy is zeroed too.
struct Tracked {
	std::shared_ptr<Widget *> slot;

	explicit operator bool() const { return slot && *slot != nullptr; }
	Widget *operator->() const { return *slot; }
};

static std::vector<std::shared_ptr<Widget *>> g_trackedSlots;

static Tracked MakeTracked(Widget *w)
{
	auto slot = std::make_shared<Widget *>(w);
	g_trackedSlots.push_back(slot);
	return Tracked{slot};
}

// What Qt does on destruction: run the destructor AND zero every QPointer,
// copies included.
static void DestroyWidget(Widget *w)
{
	w->alive = false;

	for (auto &slot : g_trackedSlots)
		if (*slot == w)
			*slot = nullptr;
}

// The Qt thread's queue, drained on demand so "has not run yet" is assertable
// rather than raced.
static std::vector<std::function<void()>> g_queue;

static void QtPostTask(std::function<void()> fn)
{
	g_queue.push_back(fn);
}

static void DrainQueue()
{
	auto queued = g_queue;
	g_queue.clear();

	for (auto &fn : queued)
		fn();
}

/* ================================================================= */

// The shipping shape: a raw capture. Kept so the hazard it models is visible
// and so the fixed form can be compared against something.
struct RawCaptureWindow {
	Widget *w;
	long showCount = 0;

	void Show()
	{
		if (++showCount == 1) {
			Widget *self = w; // [this]
			QtPostTask([self]() { self->open(); });
		}
	}

	void Hide()
	{
		if (--showCount == 0) {
			Widget *self = w; // [this]
			QtPostTask([self]() { self->hide(); });
		}
	}
};

// Mirror of the fixed SEPleaseWaitWindow::Show/Hide.
struct TrackedCaptureWindow {
	Widget *w;
	long showCount = 0;

	void Show()
	{
		if (++showCount == 1) {
			Tracked self = MakeTracked(w);

			QtPostTask([self]() {
				if (self)
					self->open();
			});
		}
	}

	void Hide()
	{
		if (--showCount == 0) {
			Tracked self = MakeTracked(w);

			QtPostTask([self]() {
				if (self)
					self->hide();
			});
		}
	}
};

static void Reset()
{
	g_virtualCallsOnDestroyedObject = 0;
	g_opens = 0;
	g_hides = 0;
	g_queue.clear();
	g_trackedSlots.clear();
}

/* ================================================================= */

// --- The ordinary path still works -----------------------------------------
//
// A fix that stopped the window ever showing would pass every safety check
// here and be useless, so this comes first.
static void check_the_window_still_opens_and_hides()
{
	Reset();

	Widget w;
	TrackedCaptureWindow win{&w};

	win.Show();
	check(g_opens == 0,
	      "Show() must post rather than open inline; the caller may not be on the Qt thread");

	DrainQueue();
	check(g_opens == 1,
	      "a drained Show() must open the window exactly once");

	win.Hide();
	DrainQueue();
	check(g_hides == 1,
	      "a drained Hide() must hide the window exactly once");
	check(g_virtualCallsOnDestroyedObject == 0,
	      "nothing was destroyed, so nothing may be reported as a call on a destroyed object");
}

// --- The regression --------------------------------------------------------
//
// Qt destroys the window between the post and the drain. This is the case that
// crashes with a raw capture.
static void check_a_task_that_outlives_the_window_does_not_call_into_it()
{
	Reset();

	Widget w;
	TrackedCaptureWindow win{&w};

	win.Show();
	DestroyWidget(&w); // the main window goes down, taking its child
	DrainQueue();

	check(g_virtualCallsOnDestroyedObject == 0,
	      "a queued Show() landing after Qt destroyed the window must be a no-op; calling open() on it dispatches a virtual through a rewound vtable and reaches _purecall");
	check(g_opens == 0, "a destroyed window must not be opened");

	// And the same on the way down, which is the commoner order: Hide() is
	// what runs while things are being torn down.
	Reset();

	Widget w2;
	TrackedCaptureWindow win2{&w2};
	win2.Show();
	DrainQueue();

	win2.Hide();
	DestroyWidget(&w2);
	DrainQueue();

	check(g_virtualCallsOnDestroyedObject == 0,
	      "a queued Hide() landing after destruction must be a no-op");
	check(g_hides == 0, "a destroyed window must not be hidden");
}

// --- The raw capture really is the hazard ----------------------------------
//
// Non-vacuity. If this passed, the test above would be proving nothing: it
// would mean the scenario cannot reach a destroyed object at all.
static void check_the_raw_capture_is_what_reaches_a_destroyed_window()
{
	Reset();

	Widget w;
	RawCaptureWindow win{&w};

	win.Show();
	DestroyWidget(&w);
	DrainQueue();

	check(g_virtualCallsOnDestroyedObject == 1,
	      "non-vacuity: the raw-`this` capture MUST reach the destroyed window, or the scenario is not modelling the bug");
}

// --- The counter decides when to post, and must survive the change ---------
static void check_only_the_edges_post_a_task()
{
	Reset();

	Widget w;
	TrackedCaptureWindow win{&w};

	win.Show();
	win.Show();
	win.Show();
	DrainQueue();

	check(g_opens == 1,
	      "three Show() calls must open once; the counter is what makes the window refcounted");

	win.Hide();
	win.Hide();
	DrainQueue();

	check(g_hides == 0,
	      "the window must stay up while a caller still holds it");

	win.Hide();
	DrainQueue();

	check(g_hides == 1,
	      "the last Hide() must take the window down exactly once");
}

// --- A handle taken before destruction is still safe to copy ---------------
//
// The lambda captures the handle BY VALUE, so the copy has to be zeroed too.
// A model where only the original is cleared would pass the tests above while
// the real QPointer semantics are what make the fix work.
static void check_every_copy_of_the_handle_is_cleared()
{
	Reset();

	Widget w;
	Tracked a = MakeTracked(&w);

	check(static_cast<bool>(a), "a handle to a live widget must be truthy");

	DestroyWidget(&w);

	check(!a,
	      "Qt zeroes every QPointer to a destroyed object, including copies held by queued lambdas; a model that cleared only one would not be modelling QPointer");
}

/* ================================================================= */

int main()
{
	check_the_window_still_opens_and_hides();
	check_a_task_that_outlives_the_window_does_not_call_into_it();
	check_the_raw_capture_is_what_reaches_a_destroyed_window();
	check_only_the_edges_post_a_task();
	check_every_copy_of_the_handle_is_cleared();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::printf("all please-wait-window lifetime checks passed\n");
	return 0;
}
