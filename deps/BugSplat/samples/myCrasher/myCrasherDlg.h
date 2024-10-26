// myCrasherDlg.h : header file
//

#include "afxwin.h"
#if !defined(AFX_MYCRASHERDLG_H__818FBAE5_7E56_4B48_94DE_9C88D20C993F__INCLUDED_)
#define AFX_MYCRASHERDLG_H__818FBAE5_7E56_4B48_94DE_9C88D20C993F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

void setNonInteractive( );


/////////////////////////////////////////////////////////////////////////////
// CMyCrasherDlg dialog

class CMyCrasherDlg : public CDialog
{
// Construction
public:
	CMyCrasherDlg(MiniDmpSender* pSender, CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CMyCrasherDlg)
	enum { IDD = IDD_MYCRASHER_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMyCrasherDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CMyCrasherDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSimulateProblem();
	afx_msg void OnCreateReport();
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
protected:
	CComboBox m_cbProblem;
    MiniDmpSender *mpSender;
public:
	afx_msg void OnCbnSelchangeProblemcombo();
	afx_msg void OnBnClickedSendadditionalfiles();
	afx_msg void OnBnClickedChkEnableHangDetect();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MYCRASHERDLG_H__818FBAE5_7E56_4B48_94DE_9C88D20C993F__INCLUDED_)
