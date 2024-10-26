// myCrasherDlg.cpp : This test application is an example of how to integrate the BugSplat library. 
// This application has serveral dialog buttons all hooked up to different ways to crash a native application.
// Select a button, get a crash, send it to BugSplat, then look at the BugSplat website to see the record of your activity.
//

#include "StdAfx.h"
#include "shlwapi.h"
#include "MyCrasher.h"
#include "MyCrasherDlg.h"
#include <fpieee.h>
#include <excpt.h>
#include <float.h>
#include <eh.h>

// BugSplat SDK Includes
#include "BugSplat.h"

MiniDmpSender* globalMpSender = NULL;

// BugSplat exception callback
bool ExceptionCallback(UINT nCode, LPVOID lpVal1, LPVOID lpVal2)
{
	wchar_t path[256];
	globalMpSender->getMinidumpPath(path, sizeof(path));

	switch (nCode)
	{
	case MDSCB_EXCEPTIONCODE:

		EXCEPTION_RECORD* p = (EXCEPTION_RECORD*)lpVal1;
		DWORD code = p ? p->ExceptionCode : 0;

		// create some files in the %temp% directory and attach them
		wchar_t cmdString[2 * MAX_PATH];
		wchar_t filePath[MAX_PATH];
		wchar_t tempPath[MAX_PATH];
		GetTempPathW(MAX_PATH, tempPath);

		swprintf(filePath, sizeof(filePath), L"%sfile1.txt", tempPath);
		swprintf(cmdString, sizeof(cmdString), L"echo Exception Code = 0x%08x > %s", code, filePath);
		_wsystem(cmdString);
		globalMpSender->sendAdditionalFile(filePath);

		swprintf(filePath, sizeof(filePath), L"%sfile2.txt", tempPath);
		wchar_t buf[_MAX_PATH];
		globalMpSender->getMinidumpPath(buf, _MAX_PATH);

		swprintf(cmdString, sizeof(cmdString), L"echo Crash reporting is so clutch!  minidump path = %s > %s", buf, filePath);
		_wsystem(cmdString);
		globalMpSender->sendAdditionalFile(filePath);
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////
// CMyCrasherDlg dialog

CMyCrasherDlg::CMyCrasherDlg(MiniDmpSender* pSender, CWnd* pParent /*=NULL*/)
	: CDialog(CMyCrasherDlg::IDD, pParent)
	, mpSender(pSender)
{
	//{{AFX_DATA_INIT(CMyCrasherDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	globalMpSender = mpSender;
}

void CMyCrasherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMyCrasherDlg)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_PROBLEMCOMBO, m_cbProblem);
}

BEGIN_MESSAGE_MAP(CMyCrasherDlg, CDialog)
	//{{AFX_MSG_MAP(CMyCrasherDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_SIMILATE_PROBLEM, OnSimulateProblem)
	ON_BN_CLICKED(IDC_CREATE_REPORT, OnCreateReport)
	//}}AFX_MSG_MAP
	ON_CBN_SELCHANGE(IDC_PROBLEMCOMBO, OnCbnSelchangeProblemcombo)
	ON_BN_CLICKED(IDC_SENDADDITIONALFILES, OnBnClickedSendadditionalfiles)
	ON_BN_CLICKED(IDC_CHK_ENABLEHANGDETECT, OnBnClickedChkEnableHangDetect)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMyCrasherDlg message handlers

BOOL CMyCrasherDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// lets populate the dialog
	CString str;
	str.LoadString(IDS_SELECTPROBLEM);
	m_cbProblem.AddString(str);

	str.LoadString(IDS_PROBLEM_MEM_EXCEPTION);  m_cbProblem.AddString(str);
	str.LoadString(IDS_PROBLEM_STACK_OVERFLOW); m_cbProblem.AddString(str);
	str.LoadString(IDS_PROBLEM_DIVBYZERO);      m_cbProblem.AddString(str);
	str.LoadString(IDS_PROBLEM_ITERATION_LOCK); m_cbProblem.AddString(str);
	str.LoadString(IDS_PROBLEM_ABORT);          m_cbProblem.AddString(str);

	m_cbProblem.SetCurSel(0);
	OnCbnSelchangeProblemcombo();

	if (mpSender->getFlags() | MDSF_DETECTHANGS)
		CheckDlgButton(IDC_CHK_ENABLEHANGDETECT, BST_CHECKED);
	else
		CheckDlgButton(IDC_CHK_ENABLEHANGDETECT, BST_UNCHECKED);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMyCrasherDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM)dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMyCrasherDlg::OnQueryDragIcon()
{
	return (HCURSOR)m_hIcon;
}


void CMyCrasherDlg::OnCreateReport()
{
	mpSender->createReport((EXCEPTION_POINTERS*)NULL);
}

// ******************************************************************************
// When compiling in release the contents of some of our sample 'crashing' 
// methods  get optimized out... this pragma disables the optimization
// so that we can get a crash. 
// ******************************************************************************
#pragma optimize( "", off)

#pragma warning (disable : 4717)
#pragma warning (disable : 4748)
void StackOverflow(int depth)
{
	char blockdata[10000];
#if _MSC_VER < 1400
	// VC 7.1 or earlier...
	sprintf(blockdata, "Overflow: %d\n", depth);
#else
	// VC 8 or later
	sprintf_s(blockdata, 10000, "Overflow: %d\n", depth);
#endif

	StackOverflow(depth + 1);
}
#pragma warning (default : 4717)
#pragma warning (default: 4748)


void CMyCrasherDlg::OnSimulateProblem()
{
	int nIndex = m_cbProblem.GetCurSel();

	switch (nIndex)
	{
	case 1:// generate Memory Exception			
	{
		*(int*)0 = 0;
	}
	break;

	case 2:// generate Stack Overflow
	{
		StackOverflow(0);
	}
	break;

	case 3:// generate Int DivByZero error
	{
		int x, y;
		x = 5;
		y = 0;
		int nRes = x / y;
	}
	break;

	case 4://generate an application hang
	{
		for (int i = 0; i < 1000; i++)
		{
			if (i == 10)
				i = 0;
			Sleep(10);
		}
	}
	break;
	case 5: //generate pure virtual function call
	{
		abort();
	}
	break;
	}
}

#pragma optimize( "", on)

void CMyCrasherDlg::OnBnClickedSendadditionalfiles()
{
	CButton* pBtn = (CButton*)GetDlgItem(IDC_SENDADDITIONALFILES);
	if (pBtn->GetCheck() == 0)
		mpSender->setCallback(NULL);
	else
		mpSender->setCallback(ExceptionCallback);
}


void CMyCrasherDlg::OnCbnSelchangeProblemcombo()
{
	if (!::IsWindow(m_hWnd))
		return;

	int nIndex = m_cbProblem.GetCurSel();
	UINT nResId = -1;
	CString str;
	switch (nIndex)
	{
	case 1:
		str.LoadString(IDS_PROBLEM_MEM_EXCEPTION_DESC);
		break;
	case 2:
		str.LoadString(IDS_PROBLEM_STACK_OVERFLOW_DESC);
		break;
	case 3:
		str.LoadString(IDS_PROBLEM_DIVBYZERO_DESC);
		break;
	case 4:
		str.LoadString(IDS_PROBLEM_ITERATION_LOCK_DESC);
		break;
	default:
		str.Empty();
		break;

	}

	GetDlgItem(IDC_STATIC_DESCRIPTION)->SetWindowText(str);
	GetDlgItem(IDC_SIMILATE_PROBLEM)->EnableWindow(nIndex != 0);
}


void CMyCrasherDlg::OnBnClickedChkEnableHangDetect()
{
	CButton* pBtn = (CButton*)GetDlgItem(IDC_CHK_ENABLEHANGDETECT);
	if (pBtn->GetCheck() == 0)
		mpSender->setFlags(mpSender->getFlags() & ~MDSF_DETECTHANGS);
	else
		mpSender->setFlags(mpSender->getFlags() | MDSF_DETECTHANGS);
}
