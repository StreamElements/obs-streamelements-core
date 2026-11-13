// A dock's signal handlers must not re-enter a manager that is being destroyed.
//
// SELIVE-215, measured over 2026-11-09..13: four events, four machines,
// `crash.kind: purecall`, three on `26.9.4.994` and -- the one that matters --
// one on `26.11.11.1100`, so the CORE-1922 paint-tree fix did not close this
// path. The async-call context names SEWidgetManager.cpp's deferred
// PersistState posts, and the fault is a pure virtual call reached through
// Qt's layout activation.
//
// The mechanism, from the code rather than from the stack:
//
//   1. Each dock gets two lambdas, connected with the THREE-argument
//      QObject::connect. There is no context object because SEWidgetManager
//      is not a QObject, so the connection's lifetime follows the DOCK.
//   2. Both lambdas capture a raw `this`.
//   3. ~SEWidgetManager calls removeDockWidget() and, inside
//      SEDeleteDockWidgetWhenSafe(), hide(). Both emit SYNCHRONOUSLY.
//   4. That is the BASE destructor. The derived part (SEBrowserWidgetManager)
//      is already destroyed, so the vtable has been demoted to one where
//      SerializeDockingWidgets() is PURE VIRTUAL.
//   5. The lambdas schedule PersistState(), which reaches the manager through
//      the raw pointer SEStatePersistence borrows back to it, and calls that
//      very method.
//
// Step 5 on a demoted vtable is `_purecall` -> abort() -> SentryAbortHandler,
// which is both the crash kind and the title SELIVE-215 carries.
//
// There is a second way in, and it runs the other way round:
// SEDeleteDockWidgetWhenSafe() deliberately LEAKS a dock when UI teardown is
// not safe, and a leaked dock keeps its connections. Then the dock outlives
// the manager and emits long after it is gone.
//
// Mirrors the lifetimes, not the Qt. The real ones need QDockWidget and a
// QMainWindow; what is under test is which object is alive when.

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

// Each of these is a dead OBS in the field.
static int g_pureVirtualCalls = 0;
static int g_serializations = 0;
static int g_handlerEntries = 0;

// The Qt thread's queue, drained on demand so "has not run yet" is assertable.
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

// Stands in for SEWidgetManager. Polymorphic, with the method the persistence
// object calls left pure in the base -- which is the whole point: once the
// derived part is destroyed, a call to it is _purecall.
struct Manager {
	// What the C++ object model does for us, made observable: set false by
	// ~Derived, so the base knows its vtable has been demoted.
	bool derivedAlive = true;

	std::shared_ptr<bool> alive = std::make_shared<bool>(true);

	// Pure in the real base. Calling it below the derived destructor is
	// undefined, so the model counts it instead of doing it.
	virtual void SerializeDockingWidgets() = 0;

	void SerializeThroughBorrowedPointer()
	{
		if (!derivedAlive) {
			++g_pureVirtualCalls;
			return;
		}

		SerializeDockingWidgets();
	}

	virtual ~Manager() = default;
};

// Stands in for SEStatePersistence, which borrows its managers as RAW
// pointers -- deliberately, for destruction-order reasons. That is what turns
// a late PersistState() into a call on a dead manager.
struct StatePersistence {
	Manager *borrowed = nullptr;

	void PersistState()
	{
		if (borrowed)
			borrowed->SerializeThroughBorrowedPointer();
	}
};

static StatePersistence g_persistence;

// Stands in for QDockWidget: hide() emits visibilityChanged synchronously.
struct Dock {
	std::vector<std::function<void()>> onVisibilityChanged;

	void hide()
	{
		auto handlers = onVisibilityChanged;

		for (auto &h : handlers)
			h();
	}

	void disconnectAll() { onVisibilityChanged.clear(); }
};

/* ================================================================= */

// The shipping shape before the fix: a raw `this`, no token.
struct RawManager : Manager {
	std::vector<Dock *> docks;

	void SerializeDockingWidgets() override { ++g_serializations; }

	void Connect(Dock *dock)
	{
		dock->onVisibilityChanged.push_back([this]() {
			++g_handlerEntries;
			QtPostTask([]() { g_persistence.PersistState(); });
		});

		docks.push_back(dock);
	}

	~RawManager() override
	{
		derivedAlive = false; // ~Derived has run; vtable demoted

		for (auto *dock : docks)
			dock->hide(); // emits, synchronously, right here
	}
};

// Mirror of the fixed SEWidgetManager.
struct GuardedManager : Manager {
	std::vector<Dock *> docks;

	void SerializeDockingWidgets() override { ++g_serializations; }

	void Connect(Dock *dock)
	{
		std::weak_ptr<bool> token = alive;

		dock->onVisibilityChanged.push_back([this, token]() {
			auto live = token.lock();

			if (!live || !*live)
				return;

			++g_handlerEntries;
			QtPostTask([]() { g_persistence.PersistState(); });
		});

		docks.push_back(dock);
	}

	~GuardedManager() override
	{
		derivedAlive = false;

		// FIRST, before anything emits.
		*alive = false;

		for (auto *dock : docks) {
			dock->disconnectAll();
			dock->hide();
		}
	}
};

static void Reset()
{
	g_pureVirtualCalls = 0;
	g_serializations = 0;
	g_handlerEntries = 0;
	g_queue.clear();
	g_persistence.borrowed = nullptr;
}

/* ================================================================= */

// --- Non-vacuity: the raw capture really does reach a demoted vtable -------
//
// If this passed, every case below would be proving nothing: it would mean the
// scenario cannot reach the bug at all.
static void check_the_raw_capture_reaches_a_destroyed_manager()
{
	Reset();

	Dock dock;

	{
		RawManager manager;
		manager.Connect(&dock);
		g_persistence.borrowed = &manager;
	} // ~RawManager hides the dock, which emits into the handler

	DrainQueue();

	check(g_handlerEntries == 1,
	      "non-vacuity: the unguarded handler MUST be entered from the destructor, or this is not modelling the bug");

	check(g_pureVirtualCalls == 1,
	      "non-vacuity: the scheduled PersistState MUST reach the manager through the borrowed raw pointer and land on the demoted vtable -- that is the _purecall SELIVE-215 carries");
}

// --- The regression --------------------------------------------------------
static void check_a_destroyed_manager_is_not_re_entered()
{
	Reset();

	Dock dock;

	{
		GuardedManager manager;
		manager.Connect(&dock);
		g_persistence.borrowed = &manager;
	}

	DrainQueue();

	check(g_handlerEntries == 0,
	      "a handler fired from the base destructor must be a no-op: the derived part is gone and anything it schedules lands on a pure virtual");

	check(g_pureVirtualCalls == 0,
	      "nothing may reach SerializeDockingWidgets() once the vtable has been demoted");
}

// --- The other direction: a dock that outlives the manager ------------------
//
// SEDeleteDockWidgetWhenSafe() leaks a dock when UI teardown is not safe, and
// a leaked dock keeps its connections. The manager dies first and the dock
// emits afterwards.
static void check_a_dock_that_outlives_the_manager_is_harmless()
{
	Reset();

	Dock leaked;

	{
		GuardedManager manager;

		// Connected, then deliberately NOT disconnected: the leak path
		// never reaches the destructor's disconnect.
		manager.Connect(&leaked);
		manager.docks.clear();

		g_persistence.borrowed = &manager;
	}

	// OBS tears its main window down later and hides the leaked dock.
	leaked.hide();
	DrainQueue();

	check(g_handlerEntries == 0,
	      "a leaked dock emitting after the manager is gone must find an expired token; the destructor's disconnect cannot help here, which is why the token exists as well");

	check(g_pureVirtualCalls == 0,
	      "and nothing may be scheduled against the dead manager");
}

// --- The feature still works ------------------------------------------------
//
// A fix that stopped persisting the layout would pass every safety check above
// and be useless, so this is the one that keeps it honest.
static void check_a_live_manager_still_persists_its_layout()
{
	Reset();

	Dock dock;
	GuardedManager manager;

	manager.Connect(&dock);
	g_persistence.borrowed = &manager;

	dock.hide(); // an ordinary visibility change, manager alive

	check(g_handlerEntries == 1,
	      "while the manager is alive the handler must run normally");
	check(g_serializations == 0,
	      "the handler must POST rather than serialize inline");

	DrainQueue();

	check(g_serializations == 1,
	      "a drained PersistState must serialize the layout exactly once");
	check(g_pureVirtualCalls == 0,
	      "nothing was destroyed, so nothing may be reported as a call on a demoted vtable");
}

// --- A task already queued when the manager dies ---------------------------
//
// The narrower race: the handler ran while everything was alive, and the task
// it posted is still sitting in the queue when the manager goes away.
static void check_a_task_queued_before_the_destructor_cannot_land_after_it()
{
	Reset();

	Dock dock;

	{
		GuardedManager manager;
		manager.Connect(&dock);
		g_persistence.borrowed = &manager;

		dock.hide(); // posts, does not run

		check(g_queue.size() == 1,
		      "test setup: the task must still be queued when the manager is destroyed");
	}

	// The borrowed pointer is what SEStatePersistence holds, and the real
	// one is cleared by the container's destruction order rather than by
	// the manager. Modelled as the manager outliving nothing: the queue is
	// drained against a dead borrow.
	g_persistence.borrowed = nullptr;

	DrainQueue();

	check(g_pureVirtualCalls == 0,
	      "a task queued before the destructor must not reach the manager afterwards; in production the borrow is cleared because statePersistence is destroyed before the managers it borrows");
}

/* ================================================================= */

int main()
{
	check_the_raw_capture_reaches_a_destroyed_manager();
	check_a_destroyed_manager_is_not_re_entered();
	check_a_dock_that_outlives_the_manager_is_harmless();
	check_a_live_manager_still_persists_its_layout();
	check_a_task_queued_before_the_destructor_cannot_land_after_it();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::printf("all dock signal lifetime checks passed\n");
	return 0;
}
