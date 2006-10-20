/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_SETTINGS_H__01979F21_0FB8_11D4_A374_00105A6BCA62__INCLUDED_)
#define AFX_SETTINGS_H__01979F21_0FB8_11D4_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Settings.h : header file
//

class CDatalog;
class CWinAfsLoadDlg;

/////////////////////////////////////////////////////////////////////////////
// CSettings dialog

class CSettings : public CDialog
{
// Construction
public:
	CSettings(CWinAfsLoadDlg* pParent);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CSettings)
	enum { IDD = IDD_MENUSETTINGS };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSettings)
	//}}AFX_VIRTUAL

// Implementation
protected:
	CWinAfsLoadDlg *m_pParent;
	// Generated message map functions
	//{{AFX_MSG(CSettings)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SETTINGS_H__01979F21_0FB8_11D4_A374_00105A6BCA62__INCLUDED_)
