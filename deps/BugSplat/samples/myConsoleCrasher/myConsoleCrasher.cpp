//
//        This sample project illustrates how to capture crashes (unhandled exceptions) in native Windows applications using BugSplat.
//
//		  To build this sample:
//		  1. Edit myConsoleCrasher.h and provide your own value for BUGSPLAT_DATABASE.  (The database can be managed by logging into
//           the BugSplat web application.)
//	      2. Create a Client ID and Client Secret pair for your BugSplat database at https://app.bugsplat.com/v2/database/integrations#oauth
//        3. Create a file MyConsoleCrasher\Scripts\env.ps1 and populate it with the following (being sure to subsitute your {{id}} and {{secret}} 
//           values from the previous step):
//                                             $BUGSPLAT_CLIENT_ID = "{{id}}"
//                                             $BUGSPLAT_CLIENT_SECRET = "{{secret}}"
//		  4. Build the project
//    
//        In order to assure that crashes sent to the BugSplat website yield exception stack traces with file/line # information, 
//        a Visual Studio post build event is configured to send the resulting .exe and .pdb files to BugSplat using the SendPdbs utility. 
//        If you do not care about file/line # info or for any reason you do not want to send these files, 
//        simply disable the post build event.
//
//        More information is available online at https://www.bugsplat.com

#pragma optimize( "", off) // prevent optimizer from interfering with our crash-producing code

#include "stdafx.h"
#include <vector>
#include <new.h>
#include <windows.h>
#include <iostream>
#include <chrono>
#include <thread>

#ifdef ASAN
// Enabling Asan error reports requires changes to the build settings, including the compiler option /fsanitize=address
#include "sanitizer/asan_interface.h"
#endif

#include "myConsoleCrasher.h"
#include "BugSplat.h"

void MemoryException();
void StackOverflow(void *p);
void DivideByZero();
void ExhaustMemory(); 
void ThrowByUser();
void ThreadException(int nthreads);
void CallAbort();
void InvalidParameters();
void OutOfBoundsVectorCrash();
void VirtualFunctionCallCrash();
void CustomSEHException();
void HeapCorruption();
void CreateReport();
bool ExceptionCallback(UINT nCode, LPVOID lpVal1, LPVOID lpVal2);
MiniDmpSender *mpSender;

int wmain(int argc, wchar_t **argv)
{
	if (IsDebuggerPresent())
	{
		wprintf(L"Run this application without the debugger attached to enable BugSplat exception handling.\n");
		DebugBreak();
		exit(0);
	}

	// BugSplat initialization.  Post crash reports to the "Fred" database for application "myConsoleCrasher" version "1.0"
	mpSender = new MiniDmpSender(BUGSPLAT_DATABASE, APPLICATION_NAME, APPLICATION_VERSION, NULL, MDSF_USEGUARDMEMORY | MDSF_LOGFILE | MDSF_LOG_VERBOSE );

	// The following calls add support for collecting crashes for abort(), vectored exceptions, out of memory,
	// pure virtual function calls, and for invalid parameters for OS functions.
	// These calls should be used for each module that links with a separate copy of the CRT.
	SetGlobalCRTExceptionBehavior();
	SetPerThreadCRTExceptionBehavior();  // This call needed in each thread of your app

	// A guard buffer of 20mb is needed to catch OutOfMemory crashes
	mpSender->setGuardByteBufferSize(20*1024*1024);

	// Set optional default values for user, email, and user description of the crash.
	mpSender->setDefaultUserName(L"Fred");
	mpSender->setDefaultUserEmail(L"fred@bugsplat.com");
	mpSender->setDefaultUserDescription(L"This is the default user crash description.");

	// Set optional notes field
	mpSender->setNotes(L"Additional 'notes' data supplied through API");

	// Set optional custom crash attributes
	mpSender->setAttribute(L"GPU", L"GeForce '{}!@#45(じみー です。)678()<>,./?[] RTX 4060 Ti");
	mpSender->setAttribute(L"Region", L"Europe");

	// Process command line args that we need prior to crashing
	for (int i = 1; i < argc; i++) {

		if (!_wcsicmp(argv[i], L"/AttachFiles")) {
			mpSender->setCallback(ExceptionCallback); // files are attached in the callback after the exception occurs
		}
	}

	// Force a crash, in a variety of ways
	for (int i = 1; i < argc; i++) {

		if (!_wcsicmp(argv[i], L"/Crash")) {
			// Don't let the BugSplat dialog appear
			mpSender->setFlags(MDSF_NONINTERACTIVE | mpSender->getFlags());
			MemoryException();
		}

		if (!_wcsicmp(argv[i], L"/MemoryException")) {
			MemoryException();
		}

		else if (!_wcsicmp(argv[i], L"/StackOverflow")) {
			StackOverflow(NULL);
		}

		else if (!_wcsicmp(argv[i], L"/DivByZero")) {
			DivideByZero();
		}

		else if (!_wcsicmp(argv[i], L"/OutOfMemory")) {
			ExhaustMemory();
		}

		else if (!_wcsicmp(argv[i], L"/Throw")) {
			ThrowByUser();
		}

		else if (!_wcsicmp(argv[i], L"/Thread")) {
			ThreadException(1);
		}

		else if (!_wcsicmp(argv[i], L"/MultipleThreads")) {
			ThreadException(10);
		}

		else if (!_wcsicmp(argv[i], L"/Abort")) {
			CallAbort();
		}

		else if (!_wcsicmp(argv[i], L"/Asan")) {
			HeapCorruption();	// / Generally this error goes undetected if Asan is not enabled
		}

		else if (!_wcsicmp(argv[i], L"/VectorOutOfBounds")) {
			OutOfBoundsVectorCrash();
		}

		else if (!_wcsicmp(argv[i], L"/InvalidParameters")) {
			InvalidParameters();
		}

		else if (!_wcsicmp(argv[i], L"/PureVirtual")) {
			VirtualFunctionCallCrash();
		}

		else if (!_wcsicmp(argv[i], L"/SEH")) {
			mpSender->setFlags(MDSF_NONINTERACTIVE | mpSender->getFlags());
			mpSender->setDefaultUserDescription(_T("BugSplat ALERT - execution continues!"));
			CustomSEHException();
			wprintf(L"Application normal exit.\n");
			exit(0);
		}

		else if (!_wcsicmp(argv[i], L"/CreateReport")) {
			CreateReport();
			wprintf(L"Application normal exit.\n");
			exit(0);
		}
	}

	// Default if no crash resulted from command line args
	MemoryException();

	return 0;
}


void MemoryException()
{
	// Dereferencing a null pointer results in a memory exception
	wprintf(L"MemoryException!\n");
	*(volatile int *)0 = 0;
}

void DivideByZero()
{
	// Calling a recursive function with no exit results in a stack overflow
	wprintf(L"DivideByZero!\n");
	volatile int x, y, z;
	x = 1;
	y = 0;
	z = x / y;
}

void StackOverflow(void *p)
{
	// Calling a recursive function with no exit results in a stack overflow
	wprintf(L"StackOverflow!\n");
	volatile char q[10000];
	while (true) {
		StackOverflow((void *)q);
	}
}

void ExhaustMemory()
{
	wprintf(L"ExhaustMemory!\n");

	// Loop until memory exhausted
	while (true)
	{
		char* a = new char[1024 * 1024];
		a[0] = 'X';
	}
}

void ThrowByUser()
{
	wprintf(L"Throw user generated exception!\n");
	throw("User generated exception!");
}


DWORD WINAPI MyThreadCrasher( LPVOID )
{
	int msec = 2000;
	Sleep(msec);

	wprintf(L"MyThreadCrasher creating memory exception after %d milliseconds!\n", msec);
	MemoryException();
	return 0;
}

void ThreadException( int max_threads)
{
	DWORD   dwThreadIdArray[100];
	HANDLE  hThreadArray[100];
	if (max_threads > 100) max_threads = 100;

	// Create worker threads.
	for (int i = 0; i<max_threads; i++)
	{
		// Create the thread to begin execution on its own.
		hThreadArray[i] = CreateThread(
			NULL,                   // default security attributes
			0,                      // use default stack size  
			MyThreadCrasher,        // thread function name
			NULL,					// argument to thread function 
			0,                      // use default creation flags 
			&dwThreadIdArray[i]);   // returns the thread identifier 

		if (hThreadArray[i] == NULL)
		{
			wprintf(L"CreateThread failed");
			ExitProcess(3);
		}
	}

	// Wait until all threads have terminated.
	WaitForMultipleObjects(max_threads, hThreadArray, TRUE, INFINITE);
}

void CallAbort()
{
	wprintf(L"abort()!\n");
	abort();
}

void OutOfBoundsVectorCrash()
{
	wprintf(L"std::vector out of bounds!\n");
	std::vector<int> v;
	v[0] = 5;
}

void InvalidParameters()
{
	wprintf(L"Invalid parameters!\n");
	char *fmt = NULL;
	printf(fmt);
}

void VirtualFunctionCallCrash()
{
	struct Base {
		Base()
		{
			wprintf(L"Pure Virtual Function Call crash!");
			BaseFunc();
		}

		virtual void DerivedFunc() = 0;

		void BaseFunc()
		{
			DerivedFunc();
		}
	};

	struct Derived : public Base
	{
		void DerivedFunc() {}
	};

	Base* instance = new Derived;
	instance->DerivedFunc();
}

DWORD SEHFilterFunction(EXCEPTION_POINTERS *exp)
{
	mpSender->createReport(exp);
	return EXCEPTION_EXECUTE_HANDLER;
}

void CustomSEHException()
{
	__try
	{
		// Use to create a BugSplat report without exiting.
		RaiseException( 
			0x123,         // exception code 
			0,             // continuable exception 
			0, NULL);      // no arguments ;
	}
	__except (SEHFilterFunction(GetExceptionInformation()))
	{
		return;
	}
}


// Address sanitizer test
void asanCallback(const char* message)
{
	mpSender->createAsanReport(message);
}


// Generally this error goes undetected if Asan is not enabled
void HeapCorruption() 
{
	void* pointerArray[20];
	struct simple_struct {
		double b;
		double c;
		char d;
	};

#ifdef ASAN
	__asan_set_error_report_callback(asanCallback);
#endif

	{
		auto a = new simple_struct;
		auto b = new simple_struct;

		pointerArray[0] = a;
		pointerArray[1] = b;
	}

	// Function attempts to use pointer from array.
	// Encounters an error and deletes the pointer without removing it from array
	{
		auto item = reinterpret_cast<simple_struct*>(pointerArray[1]);
		delete item;
	}

	// Program continues, address is reused
	constexpr auto numInts = 40;
	auto intArray = reinterpret_cast<int*>(pointerArray[1]);
	for (int i = 0; i < numInts; i++) {
		intArray[i] = 10;
	}

	// Function called again, encounters the same error and tries to delete the same memory again
	{
		auto item = reinterpret_cast<simple_struct*>(pointerArray[1]);

		__try {
			delete item;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			wprintf(L"exception handling test successful.\n");

		}
	}
}

void CreateReport()
{
	const __wchar_t  *xml = L"<report><process>"
		"<exception>"
			"<code>FATAL ERROR</code>"
			"<explanation>This is an error code explanation</explanation>"
			"<func><![CDATA[myConsoleCrasher!MemoryException]]></func>"
			"<file>/www/bugsplatAutomation/myConsoleCrasher/myConsoleCrasher.cpp</file>"
			"<line>143</line>"
			"<registers>"
				"<cs>0023</cs>"
				"<ds>002b</ds>"
				"<eax>00000011</eax>"
				"<ebp>00affb58</ebp>"
				"<ebx>00858000</ebx>"
				"<ecx>43bf1e0e</ecx>"
				"<edi>00affb58</edi>"
				"<edx>014480b4</edx>"
				"<efl>00010202</efl>"
			"</registers>"
		"</exception>"
		"<modules numloaded=\"2\">"
		"<module>"
			"<name>myConsoleCrasher</name>"
			"<order>1</order>"
			"<address>01320000-01457000</address>"
			"<path>C:/www/BugsplatAutomation/BugsplatAutomation/bin/x64/Release/temp/BugSplat/bin/myConsoleCrasher.exe</path>"
			"<symbolsloaded>deferred</symbolsloaded>"
			"<fileversion/>"
			"<productversion/>"
			"<checksum>00000000</checksum>"
			"<timedatestamp>SatJun1501:18:092019</timedatestamp>"
		"</module>"
		"<module>"
		"<name>BugSplatRc</name>"
		"<order>2</order>"
		"<address>01320000-01457000</address>"
		"<path>C:/www/BugsplatAutomation/BugsplatAutomation/bin/x64/Release/BugSplatRc.dll</path>"
		"<symbolsloaded>deferred</symbolsloaded>"
		"<fileversion/>"
		"<productversion/>"
		"<checksum>00000000</checksum>"
		"<timedatestamp>SatJun1501:18:092019</timedatestamp>"
		"</module>"
		"</modules>"
		"<threads count=\"2\">"
		"<thread id=\"0\" current=\"yes\" event=\"yes\" framecount=\"3\">"
			"<frame>"
				"<symbol><![CDATA[myConsoleCrasher!MemoryException]]></symbol>"
				"<file>/www/bugsplatAutomation/myConsoleCrasher/myConsoleCrasher.cpp</file>"
				"<line>143</line>"
				"<offset>0x35</offset>"
			"</frame>"
			"<frame>"
				"<symbol><![CDATA[myConsoleCrasher!wmain]]></symbol>"
				"<file>C:/www/BugsplatAutomation/BugsplatAutomation/BugSplat/samples/myConsoleCrasher/myConsoleCrasher.cpp</file>"
				"<line>83</line>"
				"<offset>0x239</offset>"
			"</frame>"
			"<frame>"
				"<symbol><![CDATA[myConsoleCrasher!__scrt_wide_environment_policy::initialize_environment]]></symbol>"
				"<file>d:/agent/_work/4/s/src/vctools/crt/vcstartup/src/startup/exe_common.inl</file>"
				"<line>90</line>"
				"<offset>0x43</offset>"
			"</frame>"
		"</thread>"
		"<thread id=\"1\" current=\"no\" event=\"no\" framecount=\"3\">"
			"<frame>"
				"<symbol><![CDATA[my2ConsoleCrasher!MemoryException]]></symbol>"
				"<file>/www/bugsplatAutomation/myConsoleCrasher/myConsoleCrasher.cpp</file>"
				"<line>143</line>"
				"<offset>0x35</offset>"
			"</frame>"
			"<frame>"
				"<symbol><![CDATA[my2ConsoleCrasher!wmain]]></symbol>"
				"<file>C:/www/BugsplatAutomation/BugsplatAutomation/BugSplat/samples/myConsoleCrasher/myConsoleCrasher.cpp</file>"
				"<line>83</line>"
				"<offset>0x239</offset>"
			"</frame>"
			"<frame>"
				"<symbol><![CDATA[my2ConsoleCrasher!__scrt_wide_environment_policy::initialize_environment]]></symbol>"
				"<file>d:/agent/_work/4/s/src/vctools/crt/vcstartup/src/startup/exe_common.inl</file>"
				"<line>90</line>"
				"<offset>0x43</offset>"
			"</frame>"
		"</thread>"
		"</threads></process></report>";

	mpSender->createReport(xml);
}


// BugSplat exception callback
bool ExceptionCallback(UINT nCode, LPVOID lpVal1, LPVOID lpVal2)
{

	switch (nCode)
	{
	case MDSCB_EXCEPTIONCODE:
	{
		EXCEPTION_RECORD *p = (EXCEPTION_RECORD *)lpVal1;
		DWORD code = p ? p->ExceptionCode : 0;

		// Create some files in the %temp% directory and attach them
		wchar_t cmdString[2 * MAX_PATH];
		wchar_t filePath[MAX_PATH];
		wchar_t tempPath[MAX_PATH];
		GetTempPathW(MAX_PATH, tempPath);

		wsprintf(filePath, L"%sfile1.txt", tempPath);
		wsprintf(cmdString, L"echo Exception Code = 0x%08x > %s", code, filePath);
		_wsystem(cmdString);
		mpSender->sendAdditionalFile(filePath);

		wsprintf(filePath, L"%sfile2.txt", tempPath);

		wchar_t buf[_MAX_PATH];
		mpSender->getMinidumpPath(buf, _MAX_PATH);

		wsprintf(cmdString, L"echo Crash reporting is so clutch!  minidump path = %s > %s", buf, filePath);
		_wsystem(cmdString);
		mpSender->sendAdditionalFile(filePath);
	}
	break;
	}

	return false;
}
