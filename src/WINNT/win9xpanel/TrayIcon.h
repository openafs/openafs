/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_TRAYICON_H__60C86242_1890_11D5_A375_00105A6BCA62__INCLUDED_)
#define AFX_TRAYICON_H__60C86242_1890_11D5_A375_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// TrayIcon.h : header file
//
class MENUBLOCK
{									//used to pass info to the Tray icon
public:
	MENUBLOCK(){mID=-1;}
	MENUBLOCK(UINT id,const char *msg){
		mID=id;
		title=msg;
	}
	~MENUBLOCK(){};
	MENUBLOCK& operator=(MENUBLOCK &other){
		mID=other.mID;
		title=other.title;
		return *this;
	}
	friend BOOL operator==(const MENUBLOCK &first, const MENUBLOCK &second );
	int mID;
	CString title;
};

typedef MENUBLOCK * LPMENUBLOCK;

/////////////////////////////////////////////////////////////////////////////
// CTrayIcon window

class CTrayIcon : public CWnd
{
// Construction
public:
	CTrayIcon(UINT uCallbackMessage, UINT uIcon, UINT uID);

// Attributes
public:
	static const UINT m_uMsgTaskbarCreated;		/*RegisterWindowMessage(_T("TaskbarCreated"))*/
    static CWnd  m_wndInvisible;
	static CWnd * m_pwTrayIcon;

// Operations
public:
	static CWnd * FindTrayWnd();
	void SetConnectState(int istate);
	void AddDrive(MENUBLOCK &menu);
	void RemoveDrive(MENUBLOCK &menu);
    BOOL  SetIcon(HICON hIcon);
    BOOL  SetIcon(LPCTSTR lpszIconName);
    BOOL  SetIcon(UINT nIDResource);
    BOOL  SetStandardIcon(LPCTSTR lpIconName);
    BOOL  SetStandardIcon(UINT nIDResource);
    HICON GetIcon() const;

    void  SetFocus();
    BOOL  HideIcon();
    BOOL  ShowIcon();
    BOOL  AddIcon(CWnd *pParent);
    BOOL  RemoveIcon();
	BOOL IsIconOnTray(){return (m_hParent!=NULL);}

	void MinimiseToTray(CWnd* pWnd);
	void MaximiseFromTray(CWnd* pWnd);
	BOOL RemoveTaskbarIcon(CWnd* pWnd);

    // Default handler for tray notification message
    virtual LRESULT OnTrayNotification(WPARAM uID, LPARAM lEvent);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTrayIcon)
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTrayIcon();

    DECLARE_DYNAMIC(CTrayIcon)

	// Generated message map functions
protected:
	void OnTaskbarCreated(WPARAM, LPARAM);
	HWND m_hParent;				/* Parent window to send messages too e.g. WinAFsLoadDlg*/
	CList<MENUBLOCK,MENUBLOCK&> m_MountList;
	int m_iConnectState;
	NOTIFYICONDATA  m_Notify;
	static const int m_uMaxTooltipLength;
	UINT m_DefaultMenuItemID;
	BOOL m_DefaultMenuItemByPos;

	//{{AFX_MSG(CTrayIcon)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TRAYICON_H__60C86242_1890_11D5_A375_00105A6BCA62__INCLUDED_)
