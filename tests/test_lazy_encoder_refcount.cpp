// CORE-865: SELazyOBSVideoEncoderProvider must acquire and release its encoder
// exactly once, whatever the consumer count.
//
// The reported crash was an EXCEPTION_ACCESS_VIOLATION_WRITE inside
// obs_encoder_release during Stop Streaming -- 6 distinct users, four releases.
//
// The provider is a refcounted wrapper: consumers come and go, and the
// underlying obs_encoder_t is allocated on the first consumer and released on
// the last. But the two halves disagreed:
//
//   AddConsumer:    ++m_refCount; if (!m_object) m_object = AllocRef();
//   RemoveConsumer: ReleaseRef(m_object); --m_refCount; ...
//
// The acquire is guarded by `if (!m_object)` -- once per OBJECT. The release
// was unguarded -- once per CONSUMER. One acquire, N releases.
//
// That matters because several providers share a single encoder: an allocator's
// AllocRef() resolves to another provider's object and takes its own reference
// on it. The user's OBS log from the crashing session shows one encoder pointer
// wrapped by three providers and another by four. So the SECOND consumer to
// leave took the encoder's count to zero, libobs destroyed it, and every
// provider still holding it was left with a dangling m_object. The next release
// wrote through freed memory.
//
// This mirrors the provider's reference accounting against a counted stand-in
// for obs_encoder_t. The real class needs libobs, which the suite deliberately
// does not link.

#include <cstdio>
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

// Stand-in for obs_encoder_t, with libobs's actual release semantics:
// destroyed at zero, and touching it afterwards is the bug we are hunting.
struct Encoder {
	int refCount = 0;
	bool destroyed = false;
	int useAfterFree = 0;
};

static Encoder g_encoder;

static void EncoderAddRef()
{
	++g_encoder.refCount;
}

// Mirrors obs_encoder_release: null-guarded, destroys at zero.
static void EncoderRelease()
{
	if (g_encoder.destroyed) {
		// This is the access violation. libobs would read
		// encoder->control out of the freed allocation and do an
		// interlocked decrement through it.
		++g_encoder.useAfterFree;
		return;
	}

	if (--g_encoder.refCount == 0)
		g_encoder.destroyed = true;
}

/* ================================================================= */

// Mirror of SELazyOBSVideoEncoderProvider's consumer accounting.
struct Provider {
	int refCount = 0;
	bool holdsObject = false;

	// Set false to reproduce the pre-fix behaviour.
	bool releaseOnlyOnLastConsumer = true;

	void AddConsumer()
	{
		++refCount;

		if (!holdsObject) {
			EncoderAddRef(); // AllocRef()
			holdsObject = true;
		}
	}

	void RemoveConsumer()
	{
		if (!releaseOnlyOnLastConsumer)
			EncoderRelease(); // the bug: once per consumer

		--refCount;

		if (refCount <= 0 && holdsObject) {
			if (releaseOnlyOnLastConsumer)
				EncoderRelease(); // once per object

			holdsObject = false;
		}
	}
};

static void Reset()
{
	g_encoder = Encoder();
}

/* ================================================================= */

// --- One consumer: the case that always worked --------------------------
static void check_single_consumer_balances()
{
	Reset();

	Provider p;
	p.AddConsumer();

	check(g_encoder.refCount == 1,
	      "CORE-865: the first consumer must take exactly one reference");

	p.RemoveConsumer();

	check(g_encoder.refCount == 0 && g_encoder.destroyed,
	      "CORE-865: the last consumer must release exactly one reference");
	check(g_encoder.useAfterFree == 0,
	      "CORE-865: a single consumer must never touch a destroyed encoder");
}

// --- Several consumers on one provider ----------------------------------
//
// The acquire is per-object, so the release must be too.
static void check_many_consumers_take_one_reference()
{
	Reset();

	Provider p;
	p.AddConsumer();
	p.AddConsumer();
	p.AddConsumer();

	check(g_encoder.refCount == 1,
	      "CORE-865: consumers after the first must NOT take further references");

	p.RemoveConsumer();

	check(!g_encoder.destroyed,
	      "CORE-865: an intermediate consumer leaving must NOT destroy the encoder -- others still hold it");
	check(g_encoder.refCount == 1,
	      "CORE-865: an intermediate consumer leaving must not change the reference count");

	p.RemoveConsumer();
	p.RemoveConsumer();

	check(g_encoder.destroyed && g_encoder.refCount == 0,
	      "CORE-865: the encoder must be released once the last consumer leaves");
	check(g_encoder.useAfterFree == 0,
	      "CORE-865: no access to the encoder after destruction");
}

// --- The negative control -----------------------------------------------
//
// The same sequence with the pre-fix behaviour must reproduce the crash. If
// this ever stops failing, the test above has stopped proving anything.
static void check_prefix_behaviour_reproduces_the_crash()
{
	Reset();

	Provider p;
	p.releaseOnlyOnLastConsumer = false; // as shipped

	p.AddConsumer();
	p.AddConsumer();
	p.AddConsumer();

	p.RemoveConsumer(); // 1 -> 0: destroyed, with two consumers still holding

	check(g_encoder.destroyed,
	      "CORE-865: negative control -- releasing per consumer must destroy the encoder early");

	p.RemoveConsumer();
	p.RemoveConsumer();

	check(g_encoder.useAfterFree == 2,
	      "CORE-865: negative control -- the remaining consumers must hit the freed encoder, which is the reported access violation");
}

// --- Providers sharing one encoder --------------------------------------
//
// The shape from the user's log: one obs_encoder_t wrapped by several
// providers, each holding its own reference via its allocator's AllocRef().
static void check_shared_encoder_across_providers()
{
	Reset();

	Provider a, b, c;
	a.AddConsumer();
	b.AddConsumer();
	c.AddConsumer();

	check(g_encoder.refCount == 3,
	      "CORE-865: each provider wrapping the encoder holds its own reference");

	a.RemoveConsumer();
	check(!g_encoder.destroyed,
	      "CORE-865: one provider releasing must not destroy an encoder two others still hold");

	b.RemoveConsumer();
	check(!g_encoder.destroyed,
	      "CORE-865: two providers releasing must not destroy an encoder one other still holds");

	c.RemoveConsumer();
	check(g_encoder.destroyed && g_encoder.useAfterFree == 0,
	      "CORE-865: the encoder is destroyed only when the last provider lets go");
}

// --- Add/remove churn ----------------------------------------------------
//
// A provider that drops to zero and is used again must re-acquire, not reuse a
// destroyed object.
static void check_reacquire_after_last_consumer_leaves()
{
	Reset();

	Provider p;
	p.AddConsumer();
	p.RemoveConsumer();

	check(g_encoder.destroyed,
	      "CORE-865: dropping to zero consumers releases the encoder");
	check(!p.holdsObject,
	      "CORE-865: the provider must not keep a pointer to a released encoder");
}

int main()
{
	check_single_consumer_balances();
	check_many_consumers_take_one_reference();
	check_prefix_behaviour_reproduces_the_crash();
	check_shared_encoder_across_providers();
	check_reacquire_after_last_consumer_leaves();

	if (failures) {
		std::fprintf(stderr, "%d encoder-refcount check(s) failed\n",
			     failures);
		return 1;
	}

	std::puts("test_lazy_encoder_refcount: all checks passed");
	return 0;
}
