#pragma once

//
// A counter that makes a function refuse to run inside itself.
//

#include <util/threading.h>

class RecursiveNestingLevelCounter {
public:
	RecursiveNestingLevelCounter(long *counter) : m_counter(counter)
	{
		m_level = os_atomic_inc_long(m_counter);
	}

	~RecursiveNestingLevelCounter() { os_atomic_dec_long(m_counter); }

	long level() { return m_level; }

private:
	long *m_counter;
	long m_level;
};

#define PREVENT_RECURSIVE_REENTRY()                                   \
	static long __recursive_nesting_level = 0L;                   \
	RecursiveNestingLevelCounter __recursive_nesting_level_guard( \
		&__recursive_nesting_level);                          \
	if (__recursive_nesting_level_guard.level() > 1)              \
		return;
