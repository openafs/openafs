/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// TrayIcon.cpp : implementation file
//

#include "stdafx.h"
#include "TrayIcon.h"
#include "share.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CTrayIcon, CWnd)

// this is part of MENUBLOCK routine
//need this type of overload for FIND to work, requires CONST on both sides
// CList::Find requires a == compare between const MEMBLOCK, by reference defined
BOOL operator==(const MENUBLOCK &first,const MENUBLOCK &second )	
{
return (first.mID==second.mID );
};
/////////////////////////////////////////////////////////////////////////////
// CTrayIcon

const UINT CTrayIcon::m_uMsgTaskbarCreated = ::RegisterWindowMessage(_T("TaskbarCreated"));
const int CTrayIcon::m_uMaxTooltipLength = 64;
CWnd  CTrayIcon::m_wndInvisible;


CTrayIcon::CTrayIcon(UINT uCallbackMessage, UINT uIcon, UINT uID)
{
	memset(&m_Notify,0,sizeof(m_Notify));
    ASSERT(uCallbackMessage >= WM_APP);
	CString sTooltip;
	sTooltip.LoadString(uID);
    ASSERT(sTooltip.GetLength()<= m_uMaxTooltipLength);
    CWnd::CreateEx(0, AfxRegisterWndClass(0), _T(""), WS_POPUP, 0,0,0,0, NULL, 0);	/* create an invisable window for this Class*/
    m_Notify.cbSize = sizeof(NOTIFYICONDATA);
	m_Notify.uCallbackMessage=uCallbackMessage;
	m_Notify.hIcon=AfxGetApp()->LoadIcon(uIcon);
	m_Notify.uID=uID;
    m_Notify.hWnd   =  m_hWnd;
    m_Notify.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    _tcsncpy(m_Notify.szTip, sTooltip.GetBuffer(sTooltip.GetLength()), m_uMaxTooltipLength-1);
	m_iConnectState=1;				//set current menu option to Connect
	m_hParent=NULL;
}



CTrayIcon::~CTrayIcon()
{
    RemoveIcon();
    DestroyWindow();
}


BEGIN_MESSAGE_MAP(CTrayIcon, CWnd)
	//{{AFX_MSG_MAP(CTrayIcon)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
    ON_REGISTERED_MESSAGE(CTrayIcon::m_uMsgTaskbarCreated, OnTaskbarCreated)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTrayIcon message handlers

/*
typedef struct _NOTIFYICONDATA {  
	DWORD cbSize;           // sizeofstruct,youmustset  
	HWND hWnd;              // HWND sending notification  
	UINT uID;               // IDoficon(callbackWPARAM)  
	UINT uFlags;            // see below  
	UINT uCallbackMessage;  // sent to your wndproc  
	HICON hIcon;            // handle of icon  C
	CHAR szTip[64];         // tip text
} NOTIFYICONDATA;
*/

BOOL CTrayIcon::AddIcon(CWnd *pParent)
{
    if (m_hParent==NULL)
        RemoveIcon();
	m_hParent=pParent->m_hWnd;
	ASSERT(m_hParent);
	m_Notify.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	if (!Shell_NotifyIcon(NIM_ADD, &m_Notify))
            m_hParent=NULL;
    return (m_hParent!=NULL);
}

BOOL CTrayIcon::RemoveIcon()
{
    if (m_hParent==NULL)
        return TRUE;
    m_Notify.uFlags = 0;
    if (Shell_NotifyIcon(NIM_DELETE, &m_Notify))
        m_hWnd=NULL;
    return (m_hWnd==NULL);
}

void CTrayIcon::SetConnectState(int istate)
{
	m_iConnectState=istate;
}

void CTrayIcon::AddDrive(MENUBLOCK &menu)
{
	m_MountList.AddTail(menu);
}

void CTrayIcon::RemoveDrive(MENUBLOCK &menu)
{
	POSITION pos=m_MountList.Find(menu);
	if (pos)
		m_MountList.RemoveAt(pos);
}

LRESULT CTrayIcon::OnTrayNotification(UINT wParam, LONG lParam) 
{
    //Return quickly if its not for this tray icon
    if (wParam != m_Notify.uID)
        return 0L;
    CWnd *pTargetWnd = AfxGetMainWnd();
    if (!pTargetWnd)
        return 0L;

	if ((ULONG)wParam!=m_Notify.uID ||
		(lParam!=WM_RBUTTONUP && lParam!=WM_LBUTTONDBLCLK))
		return 0;

	// If there's a resource menu with the same ID as the icon, use it as 
	// the right-button popup menu. CTrayIcon will interprets the first
	// item in the menu as the default command for WM_LBUTTONDBLCLK
	// 
	CMenu menu;
	if (!menu.LoadMenu(m_Notify.uID))
		return 0;
	CMenu* pSubMenu = menu.GetSubMenu(0);
	if (!pSubMenu) 
		return 0;

	CString msg;
	switch (m_iConnectState)
	{
	default:
		msg="&DisConnect";
		pSubMenu->EnableMenuItem(3,MF_BYPOSITION|MF_GRAYED|MF_DISABLED);
		break;
	case 1:
		msg="&Connect";
		pSubMenu->EnableMenuItem(3,MF_BYPOSITION|MF_ENABLED);
		break;
	case 2:	//error state
		pSubMenu->EnableMenuItem(1,MF_BYPOSITION|MF_GRAYED|MF_DISABLED);
		pSubMenu->EnableMenuItem(3,MF_BYPOSITION|MF_ENABLED);
		break;
	}
	pSubMenu->ModifyMenu(1,MF_BYPOSITION|MF_STRING,pSubMenu->GetMenuItemID(1),msg);
	if (m_MountList.GetCount())
	{
		pSubMenu->InsertMenu(4,MF_BYPOSITION,MF_SEPARATOR);
		POSITION pos=m_MountList.GetHeadPosition();
		LPMENUBLOCK pmenu;//=&m_MountList.GetAt(pos);
		while (pos)
		{
			pmenu=&m_MountList.GetNext(pos);
			pSubMenu->InsertMenu(5,MF_BYPOSITION,pmenu->mID,pmenu->title);
		}
	}

	if (lParam==WM_RBUTTONUP) {

		// Make first menu item the default (bold font)
		::SetMenuDefaultItem(pSubMenu->m_hMenu, 0, TRUE);

		// SetForegroundWindow. see Q135788 in MSDN.
		CPoint mouse;
		GetCursorPos(&mouse);
		::SetForegroundWindow(m_Notify.hWnd);	
		::TrackPopupMenu(pSubMenu->m_hMenu, 0, mouse.x, mouse.y, 0,
			pTargetWnd->GetSafeHwnd(), NULL);
        pTargetWnd->PostMessage(WM_NULL, 0, 0);
        menu.DestroyMenu();

	} else  // double click: execute first menu item
		::SendMessage(m_hParent, WM_COMMAND, pSubMenu->GetMenuItemID(0), 0);

	return 1; // handled
}

LRESULT CTrayIcon::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
    if (message == m_Notify.uCallbackMessage)
        return OnTrayNotification(wParam, lParam);
	
	return CWnd::WindowProc(message, wParam, lParam);
}

BOOL CTrayIcon::RemoveTaskbarIcon(CWnd* pWnd)
{
    LPCTSTR pstrOwnerClass = AfxRegisterWndClass(0);

    // Create static invisible window
    if (!::IsWindow(m_wndInvisible.m_hWnd))
    {
		if (!m_wndInvisible.CreateEx(0, pstrOwnerClass, _T(""), WS_POPUP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, 0))
			return FALSE;
    }

    pWnd->SetParent(&m_wndInvisible);

    return TRUE;
}

void CTrayIcon::MinimiseToTray(CWnd* pWnd)
{

    RemoveTaskbarIcon(pWnd);
    pWnd->ModifyStyle(WS_VISIBLE, 0);
}

void CTrayIcon::MaximiseFromTray(CWnd* pWnd)
{
    pWnd->SetParent(NULL);

    pWnd->ModifyStyle(0, WS_VISIBLE);
    pWnd->RedrawWindow(NULL, NULL, RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME |
                                   RDW_INVALIDATE | RDW_ERASE);

    // Move focus away and back again to ensure taskbar icon is recreated
    if (::IsWindow(m_wndInvisible.m_hWnd))
        m_wndInvisible.SetActiveWindow();
    pWnd->SetActiveWindow();
    pWnd->SetForegroundWindow();
}

void CTrayIcon::OnTaskbarCreated(WPARAM , LPARAM ) 
{
    if (m_hParent==NULL)
		return;
	m_Notify.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    Shell_NotifyIcon(NIM_ADD, &m_Notify);
}

CWnd * CTrayIcon::m_pwTrayIcon=NULL;

BOOL CALLBACK EnumChildProc(
		HWND hWnd,      // handle to child window
		LPARAM lParam   // application-defined value
		)
{
	CWnd *wnd=CWnd::FromHandle(hWnd);
	CString msg;
	wnd->GetWindowText(msg);
	CTrayIcon::m_pwTrayIcon=wnd;
	HICON ic=wnd->GetIcon(FALSE);
	return TRUE;
}

BOOL CALLBACK EFindTrayWnd(HWND hwnd, LPARAM lParam)
{
    TCHAR szClassName[256];
    GetClassName(hwnd, szClassName, 255);

    // Did we find the Main System Tray? If so, then get its size and keep going
    if (_tcscmp(szClassName, _T("TrayNotifyWnd")) == 0)
    {
		CTrayIcon::m_pwTrayIcon=CWnd::FromHandle(hwnd);
        return FALSE;
    }
	return TRUE;

}

CWnd * CTrayIcon::FindTrayWnd()
{
    HWND hShellTrayWnd = ::FindWindow(_T("Shell_TrayWnd"), NULL);
    if (!hShellTrayWnd) return NULL;
    EnumChildWindows(hShellTrayWnd, EFindTrayWnd, (LPARAM)0);

	if (!EnumChildWindows(
		m_pwTrayIcon->m_hWnd,         // handle to parent window
		EnumChildProc,  // callback function
		0))
		return NULL;
	return m_pwTrayIcon;
}

