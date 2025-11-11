#pragma once

#define ENABLE_SETRACE 0

#if ENABLE_SETRACE
	void *__SETrace_Trace_AddRef(const char *file, const int line,
				   const char *statement, void *ptr);

	void *__SETrace_Trace_DecRef(const char *file, const int line,
				     const char *statement, void *ptr);

	template<typename T> T* __SETrace_Trace_AddRef_Template(const char* file,
		const int line,
		const char* statement,
		T* ptr)
	{
		if (!ptr)
			return ptr;

		return static_cast<T *>(__SETrace_Trace_AddRef(file, line, statement, ptr));
	}

	template<typename T>
	T *__SETrace_Trace_DecRef_Template(const char *file, const int line,
					   const char *statement, T *ptr)
	{
		if (!ptr)
			return ptr;

		return static_cast<T *>(
			__SETrace_Trace_DecRef(file, line, statement, ptr));
	}

	void __SETrace_Dump(const char* file, int line);

	#define SETRACE_ADDREF(a) \
		__SETrace_Trace_AddRef_Template(__FILE__, __LINE__, #a, a)
	#define SETRACE_DECREF(a) \
		__SETrace_Trace_DecRef_Template(__FILE__, __LINE__, #a, a)
	#define SETRACE_AUTODECREF(a) SETRACE_DECREF(a)
	#define SETRACE_SCOPEREF(a) a
	#define SETRACE_NOREF(a) a
	#define SETRACE_IMPLICITDECREF(a) a

	#define SETRACE_DUMP() __SETrace_Dump(__FILE__, __LINE__)
#else
	#define SETRACE_ADDREF(a) a
	#define SETRACE_DECREF(a) a
	#define SETRACE_AUTODECREF(a) SETRACE_DECREF(a)
	#define SETRACE_SCOPEREF(a) a
	#define SETRACE_NOREF(a) a
	#define SETRACE_IMPLICITDECREF(a) a

	#define SETRACE_DUMP()
#endif
