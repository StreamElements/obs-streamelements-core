#include "StdAfx.h"
#include "MyCrasher.h"
#include "MyCrasherDlg.h"

// The complete BugSplat getting started guide is available at https://www.bugsplat.com
//
// To integrate BugSplat, declare an instance of MiniDmpSender that survives the life of your native application.
// You need to include the information provided to www.bugsplat.com for database identifier
// application name, and version. (These must also be provided when you upload the symbol files 
// to the BugSplat website.
//
// In order to assure that crashes sent to the BugSplat website yield exception stack traces with file/line # information, 
// just rebuild this project.  A Visual Studio post build event is configured to send the resulting .exe and .pdb files
// to BugSplat via the SendPdbs utility.  If you wish to use your own account and database, you will need to modify the post build
// event accordingly.  If you do not care about file/line# info or for any reason you do not want  to send these files, 
// simply disable the post build event.
//


BEGIN_MESSAGE_MAP(CMyCrasherApp, CWinApp)
    ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

// CMyCrasherApp construction
CMyCrasherApp::CMyCrasherApp()
{
    // The VS debugger takes precedence over BugSplat's exception handling
    if (IsDebuggerPresent()) 
    {
        MessageBox(NULL, L"Run this application without the debugger to allow BugSplat exception handling", L"Information", MB_OK);
        exit(0);
    }
    
    // BUGSPLAT INITIALIZATION
    DWORD flags = MDSF_PREVENTHIJACKING;
    mpSender = new MiniDmpSender(L"Fred", L"MyCrasher", L"1.0", NULL, flags);

	// Call each of the exposed methods
	mpSender->enableExceptionFilter(false);
	ASSERT(FALSE == mpSender->isExceptionFilterEnabled());

	mpSender->enableExceptionFilter();
	ASSERT(TRUE == mpSender->isExceptionFilterEnabled());

	mpSender->enableExceptionFilter(true);
	ASSERT(TRUE == mpSender->isExceptionFilterEnabled());

	mpSender->resetVersionString(L"1.0");

	mpSender->resetAppIdentifier(L"xyzzy");

	//mpSender->setUserZipPath(L"\\www\\tmp");

	//! Use to set full path for BsSndRpt's resource DLL (allows dialog customizations, e.g. language); default is ./BugSplatRc.dll (or ./BugSplatRc64.dll).
#ifdef _WIN64	
	wchar_t *path = L"./BugSplatRc64.dll";
#else
    wchar_t* path = L"./BugSplatRc.dll";
#endif // _WIN64
	mpSender->setResourceDllPath(path);

	//! Use to set the default user name.  Useful for quiet-mode applications that don't prompt for user/email/description at crash time.
	mpSender->setDefaultUserName(L"Fred");

	//! Use to set the default user email.  Useful for quiet-mode applications that don't prompt for user/email/description at crash time.
	mpSender->setDefaultUserEmail(L"fred@bugsplat.com");

	//! Use to set the default user description.  Useful for quiet-mode applications that don't prompt for user/email/description at crash time.
	mpSender->setDefaultUserDescription(L"I was stabbing the Orc in the right eye with my enchanted lance");

	//! Use to send an XML stack trace to BugSplat, bypassing minidump creation.
	//mpSender->createReport(L"a;b;c;d;e;f;g");

    //! Use to increase the timeout in ms used to determine if a process is hung. Default is 5000.
    mpSender->setHangDetectionTimeout(1000);

    //! Must call setFlags with MDSF_DETECTHANGS and other flags after calling setHangDetectionTimeout
    mpSender->setFlags(MDSF_DETECTHANGS | flags);
}

// The one and only CMyCrasherApp object
CMyCrasherApp theApp;

// CMyCrasherApp initialization
BOOL CMyCrasherApp::InitInstance()
{
    CCommandLineInfoEx cmdInfo;
    ParseCommandLine(cmdInfo); 
    
    // Force a crash if the crash option is specified
    if (cmdInfo.GetOption((CString)"crash")) {
        mpSender->setFlags(MDSF_NONINTERACTIVE | mpSender->getFlags());  // Don't let the BugSplat dialog appear
        *(int *) 0 = 0; // Generate a Memory Exception
    } 
    else if (cmdInfo.GetOption((CString)"crash2")) {
        *(int *) 0 = 0; // Generate a Memory Exception
    }

    // display main dialog
    CMyCrasherDlg dlg(mpSender);
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();

    // Since the dialog has been closed, return FALSE so that we exit the
    // application, rather than start the application's message pump.
    return FALSE;
}
