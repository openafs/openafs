// WinAfsLoadDlg.cpp : implementation file
//
/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#include "stdafx.h"
#include "WinAfsLoad.h"
#include "WinAfsLoadDlg.h"
#include "modver.h"
#include "encript.h"
#include <winreg.h>
#include "change.h"
#include "cregkey.h"
#include "force.h"
#include "retry.h"
#include <Lmcons.h>
#include "settings.h"
#include "afsmsg95.h"
#include "ProgBarDlg.h"
#include "MyFrame.h"
#include "commandsettings.h"
//#include <process.h>

#define PASSWORDSIZE 32
#define USERNAMESIZE 32
#define SHARENAMESIZE 12
#define MAXSHARES 10
#define DRIVESIZE 3
#define AUTOSIZE 1
#define ITEMCHECKED 0x2000	//CONTROL List defined for checked and not checked condition
#define ITEMNOTCHECKED 0x1000
#define TIMEINTERVAL 200        
#define CHECKDIR 2500	//NUMBER CYCLE before checking directory for changes (2.5 seconds)
#define MAXWORLDINDEX 25	//Number of frames (-1) in the whirling world

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	CString	m_sVersion;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	m_sVersion = _T("");
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	DDX_Text(pDX, IDC_STATICABOUT, m_sVersion);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


class CCancel
{
	BOOL *m_pPtr;
	BOOL m_bVal;
public:
	CCancel(BOOL &sav,BOOL val){m_pPtr=&sav;m_bVal=val;}
	~CCancel(){
		*m_pPtr=m_bVal;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CWinAfsLoadDlg dialog

CWinAfsLoadDlg::CWinAfsLoadDlg(const char *user,const char *pass,CWnd* pParent /*=NULL*/)
	: CDialog(CWinAfsLoadDlg::IDD, pParent), m_trayIcon(WM_MY_TRAY_NOTIFICATION,IDR_MAINFRAME,IDR_TRAYICON)
{
	//{{AFX_DATA_INIT(CWinAfsLoadDlg)
	m_sPassword = _T("");
	m_sUsername = _T("");
	m_sMountDisplay = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	if (user)
		m_sUsername=user;
	if (pass)
		m_sPassword=pass;
	m_bConnect=CWINAFSLOADAPP->m_bConnect;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pImagelist=NULL;
	m_iActiveItem=-1;		//used in MountList to indicate which item is clicked on
	m_nAutTimer=m_nDirTimer=0;
	m_pEncript=NULL;
	m_PowerResumeDelay=-1;
//	m_PowerDelay=0;
}

void CWinAfsLoadDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWinAfsLoadDlg)
	DDX_Control(pDX, IDC_WORLD, m_cWorld);
	DDX_Control(pDX, IDC_AUTHWARN, m_cAuthWarn);
	DDX_Control(pDX, IDC_AUTH, m_cAuthenicate);
	DDX_Control(pDX, IDC_SAVEUSERNAME, m_cSaveUsername);
	DDX_Control(pDX, IDC_DRIVEMOUNTLIST, m_cMountlist);
	DDX_Control(pDX, IDC_CHANGE, m_cChange);
	DDX_Control(pDX, IDC_REMOVE, m_cRemove);
	DDX_Control(pDX, IDC_OPTIONLINE, m_cOptionLine);
	DDX_Control(pDX, IDC_CHECKADVANCED, m_cCheckAdvanceDisplay);
	DDX_Control(pDX, IDC_CONNECT, m_cConnect);
	DDX_Control(pDX, IDC_CANCEL, m_cCancel);
	DDX_Control(pDX, IDC_PASSWORD, m_cPassword);
	DDX_Control(pDX, IDC_USERNAME, m_cUsername);
	DDX_Text(pDX, IDC_PASSWORD, m_sPassword);
	DDV_MaxChars(pDX, m_sPassword, 32);
	DDX_Text(pDX, IDC_USERNAME, m_sUsername);
	DDV_MaxChars(pDX, m_sUsername, 32);
	DDX_Text(pDX, IDC_MOUNTDISPLAY, m_sMountDisplay);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CWinAfsLoadDlg, CDialog)
	//{{AFX_MSG_MAP(CWinAfsLoadDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_CONNECT, OnConnect)
	ON_BN_CLICKED(IDC_CHECKADVANCED, OnCheckadvanced)
	ON_COMMAND(IDM_APP_OPEN, OnAppOpen)
	ON_BN_CLICKED(IDC_CHANGE, OnChange)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
	ON_NOTIFY(NM_CLICK, IDC_DRIVEMOUNTLIST, OnClickDrivemountlist)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_DRIVEMOUNTLIST, OnItemchangedDrivemountlist)
	ON_WM_QUERYENDSESSION()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_AUTH, OnAuthenicate)
	ON_WM_DESTROY()
	ON_WM_CTLCOLOR()
	ON_COMMAND(IDC_SHOWBACK, OnShow)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_HELPMAIN, OnHelpmain)
	ON_BN_CLICKED(IDC_SETTINGS, OnSettings)
	ON_BN_CLICKED(IDC_CANCEL, OnCancel)
	ON_WM_SYSCOMMAND()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDM_EXPLORERAFS, OnTrayButton0)
	ON_BN_CLICKED(IDM_EXPLORERAFS+16, OnTrayButton1)
	ON_BN_CLICKED(IDM_EXPLORERAFS+32, OnTrayButton2)
	ON_BN_CLICKED(IDM_EXPLORERAFS+48, OnTrayButton3)
	ON_BN_CLICKED(IDM_EXPLORERAFS+64, OnTrayButton4)
	ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
	ON_MESSAGE(WM_POWERBROADCAST,OnPowerBroadcast)
	ON_MESSAGE(WM_ERRORMSG,OnErrorMessage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinAfsLoadDlg message handlers


BOOL CWinAfsLoadDlg::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
    UINT nID =pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND)
    {
        // idFrom is actually the HWND of the tool
        nID = ::GetDlgCtrlID((HWND)nID);
		switch (nID)
		{
		case IDC_CHECKADVANCED:
			if (m_cCheckAdvanceDisplay.GetCheck())
	            pTTT->lpszText = MAKEINTRESOURCE(IDS_HIDEADVANCE);
			else
	            pTTT->lpszText = MAKEINTRESOURCE(nID);
            pTTT->hinst = AfxGetResourceHandle();
            return(TRUE);

		case IDC_SAVEUSERNAME:
			if (m_cSaveUsername.GetCheck())
	            pTTT->lpszText = MAKEINTRESOURCE(IDS_FORGETUSERNAME);
			else
	            pTTT->lpszText = MAKEINTRESOURCE(nID);
            pTTT->hinst = AfxGetResourceHandle();
            return(TRUE);
		case IDC_CANCEL:
			if (m_bServiceIsActive)
	            pTTT->lpszText = MAKEINTRESOURCE(IDS_HIDE);
			else
	            pTTT->lpszText = MAKEINTRESOURCE(nID);
            pTTT->hinst = AfxGetResourceHandle();
            return(TRUE);
		case IDC_CONNECT:
			if (m_bServiceIsActive)
	            pTTT->lpszText = MAKEINTRESOURCE(IDS_DISCONNECT);
			else
	            pTTT->lpszText = MAKEINTRESOURCE(nID);
            pTTT->hinst = AfxGetResourceHandle();
            return(TRUE);
		default:
            pTTT->lpszText = MAKEINTRESOURCE(nID);
            pTTT->hinst = AfxGetResourceHandle();
            return(TRUE);
		case 0:
			break;
        }
    }
    return(FALSE);
}

BOOL CWinAfsLoadDlg::PreTranslateMessage(MSG* pMsg) 
{
	// TODO: Add your specialized code here and/or call the base class
	// get the accelerator table to work, i.e. press <enter> is same as IDC_CONNECT
	if (m_hAccelTable) 
	{
		if (::TranslateAccelerator(m_hWnd, m_hAccelTable, pMsg))
			return(TRUE);
	}
   	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CWinAfsLoadDlg::OnInitDialog()
{
	DWORD rc;
	char compName[129];
	CString msg;
	CDialog::OnInitDialog();
	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strMenu;
		strMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(!strMenu.IsEmpty());
		pSysMenu->AppendMenu(MF_SEPARATOR);
		pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strMenu);
	}
	if (CWINAFSLOADAPP->m_bShowAfs)
		OnShowAddMenus();
	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	// TODO: Add extra initialization here

	m_hAccelTable = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR1)); 

// bind to socket's DLL

	DWORD uStatus;
	WSADATA WSAData;
	if ((uStatus = WSAStartup(MAKEWORD(1,1), &WSAData)) == 0) {
		LOG("AFS:Startup%s",WSAData.szDescription);
	} else {
		msg.Format("AFS Error - Cannot find socket.dll status=0x%0x", uStatus);
		AfxMessageBox(msg,MB_OK);
		SendMessage(WM_CLOSE,0,0);
		LOG("WM_CLOSE3");
		return FALSE;
	}

	// We need to make sure the DLL's support the features provided by the CListCtrl common control
	// That is they must be later than 4.71 
    
//	ver.GetFileVersionInfo("comctl32.dll",vernum);	//use this form if Win95???

	DWORD vernum;
    DLLVERSIONINFO dvi;
	CModuleVersion ver;
	if (!ver.DllGetVersion("comctl32.dll", dvi,vernum))
		ver.GetFileVersionInfo("comctl32.dll",vernum);
	LOG("comctl32 %d.%d",(vernum>>16) & 0xff,vernum & 0xff);
	if(vernum< PACKVERSION(4,70))
	{
		msg.Format("Current Version %d.%d, Recommend at least version 4.70 for COMCTL32.dll, (Install, 401COMUPD.EXE or Internet Explorer 4.01 SP2 or better)",HIWORD(vernum),LOWORD(vernum));
		MessageBox(msg, "AFS DLL Version Warning", MB_OK);
		m_cCheckAdvanceDisplay.ModifyStyle(0,WS_DISABLED);
	} else {
	if (!ver.DllGetVersion("Shell32.dll", dvi,vernum))
		ver.GetFileVersionInfo("Shell32.dll",vernum);
	LOG("Shell32 %d.%d",(vernum>>16) & 0xff,vernum & 0xff);
	if(vernum< PACKVERSION(4,00))
	{
		msg.Format("Current Version %d.%d, Recommend at least version 4.00 for SHELL32.dll, (Install Internet Explorer 4.01 SP2 or better)",HIWORD(vernum),LOWORD(vernum));
		MessageBox(msg, "AFS DLL Version Warning", MB_OK);
		m_cCheckAdvanceDisplay.ModifyStyle(0,WS_DISABLED);
	}
	}
	if (!ver.DllGetVersion("ADVAPI32.dll", dvi,vernum))
		ver.GetFileVersionInfo("ADVAPI32.dll",vernum);
	LOG("ADVAPI32 %d.%d",(vernum>>16) & 0xff,vernum & 0xff);
	m_pEncript = (CEncript *)new CEncript(this);
	

	// initiallze AFS and setup Progress display
	if (!m_cAfs.Init(this,msg))
		HandleError(msg);

	m_trayIcon.AddIcon(this);
	EnableToolTips(TRUE);
	
	m_bServiceIsActive=FALSE;
	// Obtain Version String
	CRegkey key(HKEY_LOCAL_MACHINE,"Software\\Open AFS\\AFS Control Center","CurrentVersion");
	key.GetString("VersionString",m_VersionString,128);
	msg.Format("%s %s",AFSTITLE,m_VersionString);
	SetWindowText(msg);

	UpdateData(TRUE);

	rc = 128; GetComputerName(compName, &rc);
	m_sComputername=compName;
	m_sMountDisplay.Format("Connected Drives on Computer:%s",compName);
	m_sUsername="";
	CRect rect;
	GetWindowRect(&m_OriginalRect);
	m_cOptionLine.GetWindowRect(&rect);
	m_DialogShrink=rect.top-m_OriginalRect.top+5;	//make it above the edit box
	SetWindowPos(&wndTop,0,0,m_OriginalRect.right-m_OriginalRect.left,m_DialogShrink,SWP_NOMOVE|SWP_NOZORDER);
	// Pull registration information out
	CRegkey regkey("AFS\\Window");
	UINT logintime=0;
	DWORD size=sizeof(m_PowerResumeDelay);
	regkey.GetBinary("PowerResumeDelay",(LPBYTE)&m_PowerResumeDelay,size);
	if (m_PowerResumeDelay<=0) {
		m_PowerResumeDelay=20;	//seconds
		regkey.PutBinary("PowerResumeDelay",(LPBYTE)&m_PowerResumeDelay,size);
	}
	CHAR pLoginName[UNLEN + 1];
	size=UNLEN;
	if (CWINAFSLOADAPP->m_bNoID)
	{
		CWnd *w=GetDlgItem(IDC_USERNAME);
		w->EnableWindow(FALSE);
		w=GetDlgItem(IDC_PASSWORD);
		w->EnableWindow(FALSE);
		w=GetDlgItem(IDC_SAVEUSERNAME);
		w->EnableWindow(FALSE);
	} else {
		if (GetUserName(pLoginName,&size))
		{
			m_sLoginName=pLoginName;
		} else
			m_sLoginName="Bozo";
		CString sUser,sPass;
		RegPassword(sUser,sPass,TRUE);
		if (m_sUsername=="")
		{
			m_sUsername=sUser;
		}
		if ((m_sUsername=="") && (m_sLoginName!="Bozo"))
		{
			m_sUsername=m_sLoginName;
		}
		if ((m_sPassword=="") || (m_sPassword.IsEmpty()))
		{
			m_sPassword=sPass;
		}
		if (m_sPassword.IsEmpty())
		{
			m_cSaveUsername.SetCheck(FALSE);
			RegLastUser(m_sUsername,TRUE);
		} else
			m_cSaveUsername.SetCheck(TRUE);
	}
	// Initialize mount control list

	m_pImagelist = new CImageList();
	ASSERT(m_pImagelist != NULL);    // serious allocation failure checking
	m_pImagelist->Create(16, 16, 0, 2, 2);
	m_cMountlist.SetImageList(m_pImagelist, LVSIL_SMALL);

	m_cMountlist.GetWindowRect(&rect);
	{// don't particullary want to keep the DC around
	CClientDC dc(this);
	CSize cSize=dc.GetTextExtent("*",1);
	CSize dSize=dc.GetTextExtent("XXXXXXXXXXX",12);
	cSize.cx+=10;
	int r=rect.Width()-cSize.cx-dSize.cx;
#define COLDRIVE 0
	m_cMountlist.InsertColumn(COLDRIVE, "Drive", LVCFMT_LEFT,
		r * 1/5, 0);
#define COLPATH 1
	m_cMountlist.InsertColumn(COLPATH, "Afs Path", LVCFMT_LEFT,
		r * 4/5, 1);
#define COLAUTO 2
	m_cMountlist.InsertColumn(COLAUTO, "*", LVCFMT_CENTER,
		cSize.cx, 1);
#define COLSHARE 3
#define NUMCOL 4
	m_cMountlist.InsertColumn(COLSHARE, "Share Name", LVCFMT_LEFT,
		dSize.cx, 1);
	m_cMountlist.SetExtendedStyle(m_cMountlist.GetExtendedStyle() | LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES );
	}
	ProfileData(FALSE);
	UpdateData(FALSE);
	DismountAll(msg,3);
	BuildDriveList(TRUE);
	m_seqIndex=0;
	m_seqCount=0;			//set up sequence timer for moving world
	m_nShown=0;
	m_cAuthWarn.ShowWindow(SW_HIDE);
	m_bkBrush.CreateSolidBrush(RGB(255,255,0));
   
// WHIRLING world display
	m_cWorld.SetWindowPos(NULL,0,0,42,42,SWP_NOMOVE|SWP_NOZORDER);
	m_cWorld.GetWindowRect(&m_WorldRect);
	ScreenToClient(&m_WorldRect);
    VERIFY( m_bmpWorld.LoadBitmap( IDB_WORLD ) );

	m_seqCount=0;
	m_seqIndex=0;
	m_dirCount=0;
	m_nDirTimer=SetTimer(WM_DIRTIMER,TIMEINTERVAL,0);
#ifdef _DEBUG
	m_nAutTimer=SetTimer(WM_AUTTIMER,10000,0);
#else
	m_nAutTimer=SetTimer(WM_AUTTIMER,60000,0);
#endif
	m_bRestartAFSD=FALSE;
	m_OSVersion.dwOSVersionInfoSize=sizeof(m_OSVersion);
	GetVersionEx(&m_OSVersion);			//get OS type & version
	if (m_bConnect)
		PostMessage(WM_COMMAND,IDC_CONNECT,0);
	else 
		GotoDlgCtrl(&m_cConnect);
	return FALSE;
	// return TRUE  unless you set the focus to a control
}	// end OnInitDialog

BOOL CWinAfsLoadDlg::RegPassword(CString &user,CString &pass,BOOL fetch)
{
	DWORD len;
	char *pBuffer;
	CString sKey;
	sKey.Format("AFS\\Security\\%s",m_sLoginName);
	CRegkey regkey(sKey);
	if (regkey.Getkey()==0) return FALSE;
	if (fetch)
	{
		regkey.GetString("UserName",user,USERNAMESIZE);
		if (user.IsEmpty()||(user==""))
		{
			return TRUE;
		}
		len=PASSWORDSIZE;
		pBuffer=new char[(int)len+1];
		regkey.GetBinary("Password",(LPBYTE)pBuffer,len);
		if (len==0)
			return TRUE;
		if (m_pEncript)
			m_pEncript->Encript(m_sComputername,m_sLoginName,user
			,(UCHAR *)pBuffer,len,FALSE);
		pBuffer[(int)len]=0;
		pass=pBuffer;
		delete(pBuffer);
	} else {
		if (user.IsEmpty()||pass.IsEmpty()) {
			regkey.PutString("UserName",user);
			regkey.PutBinary("Password",NULL,0);
			return TRUE;
		}
		if (regkey.PutString("UserName",user))
		{
			len=pass.GetLength();
			pBuffer=new char[(int)len+1];
			strcpy(pBuffer,pass);
			if (m_pEncript)
				m_pEncript->Encript(m_sComputername,m_sLoginName,user
				,(UCHAR *)pBuffer,len,TRUE);
			else
				len=strlen(pBuffer);
			regkey.PutBinary("Password",(LPBYTE)pBuffer,len);
			delete(pBuffer);
		}
	}
	return TRUE;
}

BOOL CWinAfsLoadDlg::RegLastUser(CString &user,BOOL fetch)
{
	CString sKey;
	sKey.Format("AFS\\Security\\%s",m_sLoginName);
	CRegkey regkey(sKey);
	if (regkey.Getkey()==0) return FALSE;
	if (fetch)
	{
		regkey.GetString("LastUser",user,USERNAMESIZE);
		return TRUE;
	}
	return (regkey.PutString("LastUser",user));
}

// MOdify the system menu
/// need to add items to Icon Menu also
/// check out sc_close for close postion rather than depend on fixed position
/// Need to add Menu Item aphbetical to drive letter

#define NUMOFMENUDRIVES 5
#define INSERTPOSITION 6

void CWinAfsLoadDlg::AddMenu(const char *sDrive,const char *share)
{
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu == NULL) return;
	CString msg;
	msg.Format("%s  %s",sDrive,share);
	UINT id=pSysMenu->GetMenuItemID(INSERTPOSITION);	//insert before About Menu Item
	if (id==IDM_EXPLORERAFS) 
	{
		char lpString[3];
// limit number of drives added to is NUMOFDRIVES
		int i=INSERTPOSITION;
		do {
			int len=pSysMenu->GetMenuString(i,lpString,2,MF_BYPOSITION);
			if (*sDrive==*lpString) return;
			UINT nID=pSysMenu->GetMenuItemID(++i);
			if  ((nID<IDM_EXPLORERAFS) || (nID>IDM_EXPLORERAFS+16*(NUMOFMENUDRIVES-1)))//NOT a Drive ITEM
			{
				pSysMenu->InsertMenu(i,MF_BYPOSITION,IDM_EXPLORERAFS+16*(i-INSERTPOSITION),msg);
				MENUBLOCK blk(IDM_EXPLORERAFS+16*(i-INSERTPOSITION),msg);
				m_trayIcon.AddDrive(blk);
				return;
			}
		} while (i<INSERTPOSITION+NUMOFMENUDRIVES-1);
	} else {
		id=IDM_ABOUTBOX;
		MENUBLOCK blk(IDM_EXPLORERAFS,msg);
		m_trayIcon.AddDrive(blk);
		if (!pSysMenu->InsertMenu(INSERTPOSITION,MF_BYPOSITION,MF_SEPARATOR)) return;
		if (!pSysMenu->InsertMenu(INSERTPOSITION,MF_BYPOSITION,IDM_EXPLORERAFS,msg)) return;
	}
	return;
}

void CWinAfsLoadDlg::RemoveMenu(const char *sDrive)
{
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu == NULL) return;
	UINT nID=pSysMenu->GetMenuItemID(INSERTPOSITION);
	if ((nID<IDM_EXPLORERAFS)||(nID>IDM_EXPLORERAFS+16*NUMOFMENUDRIVES)) return;
	char lpString[3];
	for (int i=INSERTPOSITION;i<=INSERTPOSITION+NUMOFMENUDRIVES-1;i++)
	{
		int len=pSysMenu->GetMenuString(i,lpString,2,MF_BYPOSITION);
		if (*sDrive==*lpString)
		{
			MENUBLOCK blk;
			blk.title=sDrive;
			blk.mID=pSysMenu->GetMenuItemID(i);
			m_trayIcon.RemoveDrive(blk);
			if (!pSysMenu->RemoveMenu(i,MF_BYPOSITION )) return;
			if  ((i==INSERTPOSITION)&& (pSysMenu->GetMenuItemID(i)==MF_SEPARATOR))
				pSysMenu->RemoveMenu(i,MF_BYPOSITION);
			return;
		}
	}
/// scan rest of list to see if you need to add a drive
	return;
}
void CWinAfsLoadDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	switch (nID & 0xFFF0)
	{
	case IDM_VIEWAFS:
		{
		HWND hwnd=m_cAfs.GetLoadWindowHandle();
		if (hwnd==NULL) break;
		UINT show=(::IsWindowVisible(hwnd))?SW_HIDE:SW_SHOWNORMAL;
		::ShowWindow(m_cAfs.GetLoadWindowHandle(),show);
		}
		break;
	case IDM_CAPTUREWINDOW:
		CWINAFSLOADAPP->m_bLogWindow=TRUE;
		CWINAFSLOADAPP->ShowLog(TRUE);
		break;
	case IDM_CAPTUREFILE:
		CWINAFSLOADAPP->m_bLog=TRUE;
		CWINAFSLOADAPP->ShowPrint(TRUE);
		break;
	case IDM_ABOUTBOX:
		{
		CAboutDlg dlgAbout;
		// Lets update About box to include version information 
		char wdir[MAX_PATH];
#ifdef _DEBUG 
		wsprintf(wdir,"%s.exe",((CWinAfsLoadApp *)AfxGetApp())->m_pszExeName);
#else
		wsprintf(wdir,"%s.exe",CWINAFSLOADAPP->m_pszExeName);
#endif
		DWORD wHandle;
		DWORD rc;
		rc =GetFileVersionInfoSize(wdir,&wHandle);
		if (rc>0)
		{
			BYTE *lpData=(BYTE *)new BYTE[rc];
			if (GetFileVersionInfo(wdir,wHandle,rc,lpData))
			{
				UINT len;
				struct TRANSLATION {
					WORD langID;         // language ID
					WORD charset;        // character set (code page)
				};
				CString msg;
				TRANSLATION mTrans,* pTrans;
				if (VerQueryValue(lpData,
					"\\VarFileInfo\\Translation", (PVOID *)&pTrans, &len) && len >= 4) {
					mTrans = *pTrans;
					TRACE("code page = %d\n", mTrans.charset);
					}
				LPCTSTR pVal;
				UINT iLenVal;
				CString query;
				query.Format(_T("\\StringFileInfo\\%04x%04x\\%s"),
					mTrans.langID,
					mTrans.charset,
					_T("ProductVersion"));
				if (VerQueryValue(lpData, (LPTSTR)(LPCTSTR)query,
					(LPVOID*)&pVal, &iLenVal)) {
//					dlgAbout.m_sVersion.Format("WinAFSload Version %s",pVal);
				}
   			}
			delete lpData;
		}
		dlgAbout.DoModal();
		}
	case SC_ICON:
		if (GetFocus())
			m_trayIcon.MinimiseToTray(this);	//only if click on miminization botton i
		else 
			CDialog::OnSysCommand(nID, lParam);	//do this to mimic the normal windows behavor
		break;

	default:
		if ((nID>=IDM_EXPLORERAFS)&&(nID<=IDM_EXPLORERAFS+16*NUMOFMENUDRIVES))
		{
			CMenu* pSysMenu = GetSystemMenu(FALSE);
			if (pSysMenu == NULL) return;
			CString msg;
			char sDrive[3];
			pSysMenu->GetMenuString(nID,sDrive,3,MF_BYCOMMAND);
			if (!m_cAfs.StartExployer(msg,sDrive))
				HandleError(msg);
			break;
		}
		CDialog::OnSysCommand(nID, lParam);
		break;
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWinAfsLoadDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	if (IsIconic())
	{
		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

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
		m_bmpWorld.DrawTrans(dc,m_WorldRect,(m_WorldRect.Width()-1)*m_seqIndex);
	}
}


void CWinAfsLoadDlg::HandleError(const char *s,BOOL display)
{
	LOG(s);
	UpdateData(FALSE);
	if (display)
	{	
		m_cAfs.FinishProgress();
		CWait wait(IDC_ARROW);
		MessageBox(s,"AFS Client Warning",MB_ICONWARNING | MB_OK);
	}
}


void CWinAfsLoadDlg::OnConnect() 
{
// This is required to keep a process from getting inbehind the error display dialog box and hitting the Connect Button again.
// the best way to avoid this would have been to separate GUI task from processes; that way
// the Connect button would have bee disabled while the process is busy or if waiting for an error response!  It is possible 
// to send a ClickMessage to the Connect button even with the a modal dialog box overlaying the main dialog!

	if (CWait::IsBusy()) return;
	CString msg;
	CWait wait;
	m_iActiveItem=-1;		//necessary to indicate to MountList that items being changed are done by program not by mouse input
	UpdateData(TRUE);
	if (m_bServiceIsActive) // DOS is active so lets terminate
	{
		// disconnect any connected AFS mounts, unless there are files open!
		if (!DismountAll(msg,0))
			return;
		TerminateBackground();
		m_nShown=0;
		m_cAuthWarn.ShowWindow(SW_HIDE);
		return;
	}
	CCancel cancel(m_bConnect,FALSE);		//clear m_bConnect when connection done
	if (									// if username is present then must have password
		IsGetTokens() 
		&& (m_sPassword=="")
		) 
	{
		HandleError("You must enter a password!");
		m_cPassword.SetFocus();
		return;
	}
	CProgress progress(this,7);
	progress.Next();
	if (!m_cAfs.Create(msg,m_sComputername,m_procInfo)) 
	{
		progress.Finish();
		HandleError(msg);
		m_procInfo.hThread=0;
		return;
	}
	// CWINAFSLOADAPP->m_sMsg contains the host name used for login
	LOG("AFS Client Console started successfully [%s].",(const char *)m_sComputername);
	CString sDrive;

	CString sKey;
	int iMounted=-1;
	for (int iItem=0;iItem<m_cMountlist.GetItemCount();iItem++)
	{
		sKey = m_cMountlist.GetItemText(iItem,COLSHARE);
		if (stricmp(sKey,"all")!=0) continue;		// lets find the All first
		CString sAuto(m_cMountlist.GetItemText(iItem,COLAUTO));
		if (sAuto!="*")
			continue;
		sDrive=m_cMountlist.GetItemText(iItem,COLDRIVE);
		LOG("Connect %s %s",sDrive,sKey);
		if (m_cAfs.Mount(msg,sDrive,sKey))
		{
			m_cMountlist.SetCheck(iItem,TRUE);
			AddMenu(sDrive,sKey);
			iMounted=iItem;
		} else {
			CString msg2;
			m_cAfs.Shutdown(msg2);
			m_procInfo.hThread=0;
			msg2.Format("Connect can't continue: %s",msg);
			progress.Finish();
			HandleError(msg2);
			return;
		}
		break;
	}
	if (iMounted<0){
		// Scan for any other connection
		for (int iItem=0;iItem<m_cMountlist.GetItemCount();iItem++)
		{
			CString sAuto(m_cMountlist.GetItemText(iItem,COLAUTO));
			if (sAuto!="*")
				continue;
			sDrive=m_cMountlist.GetItemText(iItem,COLDRIVE);
			LOG("Connect %s %s",sDrive,sKey);
			if (m_cAfs.Mount(msg,sDrive,sKey))
			{
				m_cMountlist.SetCheck(iItem,TRUE);
				AddMenu(sDrive,sKey);
				iMounted=iItem;
			} else {
				CString msg2;
				m_cAfs.Shutdown(msg2);
				m_procInfo.hThread=0;
				msg2.Format("Connect can't continue: %s",msg);
				progress.Finish();
				HandleError(msg2);
				return;
			}
			break;
		}
	}
	progress.Next();
	if  (IsGetTokens())
	{
#if 0
		if (iMounted<0)		//have we mounted at least one drive
		{
			m_cAfs.Shutdown(msg);
			m_procInfo.hThread=0;
			msg="Connect can't continue, mountable drive not defined";
			progress.Finish();
			HandleError(msg);
			return;
		}
#endif
		if (!m_cAfs.Authencate(msg,m_sUsername,m_sPassword))
		{
			CString msg2;
			m_cAfs.Dismount(msg2,sDrive,TRUE);
			m_cMountlist.SetCheck(iItem,FALSE);
			RemoveMenu(sDrive);
			m_cAfs.Shutdown(msg2);
			m_procInfo.hThread=0;
			progress.Finish();
			HandleError(msg);
			return;
		}
	}
	progress.Next();	
	// scan through the list for any additional items to connect
	BuildDriveList();
	for (iItem=0;iItem<m_cMountlist.GetItemCount();iItem++)
	{
		CString sKey(m_cMountlist.GetItemText(iItem,COLSHARE));
		if (iItem==iMounted) continue;
		CString sAuto(m_cMountlist.GetItemText(iItem,COLAUTO));
		if (sAuto!="*")
			continue;
		sDrive=m_cMountlist.GetItemText(iItem,COLDRIVE);
		LOG("Connect %s %s",sDrive,sKey);
		if (m_cAfs.Mount(msg,sDrive,sKey))
		{
			m_cMountlist.SetCheck(iItem,TRUE);
			AddMenu(sDrive,sKey);
		} else
			HandleError(msg);
	}
	if (!m_cAfs.StartExployer(msg,sDrive))
		HandleError(msg);
	if (!m_cSaveUsername.GetCheck())
	{
		m_sPassword.Empty();
		UpdateData(FALSE);
	}
//	Sleep(5000)
	m_bServiceIsActive=TRUE;	//DON'T allow shutdown until DOS is terminated
	UpdateConnect();
	if (m_bConnect)
		PostMessage(WM_COMMAND,IDC_CANCEL,0);
}

void CWinAfsLoadDlg::PostNcDestroy() 
{
	// TODO: Add your specialized code here and/or call the base class
	WSACleanup();
	if( m_pImagelist != NULL)
		delete m_pImagelist;
	if (m_pEncript)
		delete m_pEncript;
    m_bmpWorld.DeleteObject();
	CDialog::PostNcDestroy();
}


void CWinAfsLoadDlg::OnCancel() 
{
	// TODO: Add your control notification handler code here
	
	if (m_bServiceIsActive)
		ShowWindow(SW_HIDE);
	else
	{
		CString msg,warn="";
		ProfileData(TRUE);
		CString user,pass;
		UpdateData(TRUE);
		RegPassword(user,pass,TRUE);
		if (m_cSaveUsername.GetCheck())
		{
			if (!m_pEncript->IsValid())
				warn="\n\nAt this time if you choose to save your password on your harddrive, it will not be in encripted form.\n\n\
To encript your password you must have version 4.71 of ADVAPI32.dll (or better).\n Note: To upgrade this system file, install Internet Explorer 4.01 SP2 (or better).\n\n\
HINT:To intstall Internet Explorer 4.01 Service Pack 2\n\
1. Insert the CD that contains Service Pack 2\n\
2. Use the Explorer to navigate to the CDROM sub-Directory \\IE4SP2\\I386\n\
3. Double Click on Setup \n\
4. Follow the instructions for standard Installation\n\
5. If asked, select option to install Windows Desktop Update\n\
6. You will have to restart your computer when installation is completed\n\
7. You may remove all other components except SP2 (approximatly 90MB)";
			if ((user==m_sUsername) && (m_sPassword!=pass))
			{
				msg.Format("Your password is different from what was previous saved on disk.\n Do you wish to update? %s",warn);
				if (MessageBox(msg
					,"AFS - Security Warning"
					, MB_ICONWARNING | MB_YESNO)==IDYES) 
				RegPassword(m_sUsername,m_sPassword,FALSE);
			} else if (user==""){
				if (m_pEncript)
					msg.Format("Your user name and password (encrypted) will be saved on your disk.\n Do you wish to save?");
				else
					msg.Format("Your user name and password will be saved on your disk.\n Do you wish to save?%s",warn);
				if (MessageBox(msg,"AFS - Security Warning"
					, MB_ICONWARNING | MB_YESNO)==IDYES) 
				RegPassword(m_sUsername,m_sPassword,FALSE);
			} else if (user!=m_sUsername){
				msg.Format("Previous saved on the disk a user name (%s) and password will be overwritten by new user name (%s) and password(encrypted). Do you wish to update?%s",user,m_sUsername,warn);
				if (MessageBox(msg,"AFS - Security Warning"
					, MB_ICONWARNING | MB_YESNO)==IDYES) 
				RegPassword(m_sUsername,m_sPassword,FALSE);
			}
		}
		else 
		{
			if ((pass!="")&&(user==m_sUsername))
			{
				msg.Format("Previous user name (%s) and password will be removed from the disk\nDo you wish to clear?",user);
				if (MessageBox(msg,"AFS - Security Warning",  
					MB_ICONWARNING | MB_YESNO)==IDYES) 
				{
					user="";
					RegPassword(user,user,FALSE);
				}
			}
		}
		RegLastUser(m_sUsername,FALSE);
		CDialog::OnCancel();
	}
}

void CWinAfsLoadDlg::OnCheckadvanced() 
{
	// TODO: Add your control notification handler code here
	if (m_cCheckAdvanceDisplay.GetCheck()) 
	{
		CWait wait;
		if (m_bServiceIsActive)
			UpdateMountDisplay();
		SetWindowPos(&wndTop,0,0,m_OriginalRect.right-m_OriginalRect.left,m_OriginalRect.bottom-m_OriginalRect.top,SWP_NOMOVE|SWP_NOZORDER);
	} else {
		SetWindowPos(&wndTop,0,0,m_OriginalRect.right-m_OriginalRect.left,m_DialogShrink,SWP_NOMOVE|SWP_NOZORDER);
	}
}

BOOL  CWinAfsLoadDlg::DismountAll(CString &msg,INT mode) 
{// mode =0 , give user option to retry or cancel
	//mode =1, give user option to force
	//mode =3 , force it anyway
	TCHAR szDrive[] = TEXT("*:");
	TCHAR szMapping[ MAX_PATH ] = TEXT("");
 	LPTSTR pszSubmount = szMapping;
    DWORD dwSize = MAX_PATH;
	TCHAR pBuffer[MAX_PATH];
	if (strlen(m_cAfs.MountName())==0)
		return TRUE;
	for (TCHAR chDrive = 'D'; chDrive <= 'Z'; ++chDrive)
	{
		szDrive[0]=chDrive;
		if (WNetGetConnection (szDrive, szMapping, &dwSize) == NO_ERROR)
		{
			CHAR *p=strstr(szMapping,m_cAfs.MountName());
			if (p)
			{
				LOG("Disconnect %s %s",szDrive,szMapping);
				if (!m_cAfs.Dismount(msg,szDrive,(mode==3)))
				{
					CWait wait;
					switch (mode)
					{
					case 1:	//Option to force
						{
						CForce dlg;
						dlg.m_sMsg=msg;
						switch (dlg.DoModal())
						{
						case IDOK:
							LOG("Force Disconnect %s %s",szDrive,szMapping);
							if (!m_cAfs.Dismount(msg,szDrive,TRUE))
								return FALSE;
							break;
						default:
							return FALSE;
						}
						}
						break;
					default:
						{//option to return
						CRetry dlg(TRUE);
						dlg.m_sMsg=msg;
						switch (dlg.DoModal())
						{
						case IDC_FORCE:
							LOG("Force Disconnect %s %s",szDrive,szMapping);
							if (!m_cAfs.Dismount(msg,szDrive,TRUE))
								return FALSE;
							break;
						case IDOK:
							chDrive--;
							continue;

						default:
							return FALSE;
						}
						}
					}
				}
				p=strrchr(szMapping,'\\');
				if (p)
				for (int iItem=m_cMountlist.GetItemCount()-1;iItem>=0;iItem--)
				{
					m_cMountlist.GetItemText(iItem,COLSHARE,pBuffer,MAX_PATH);
					if (stricmp(p+1,pBuffer)!=0) continue;
					m_cMountlist.GetItemText(iItem,COLDRIVE,pBuffer,MAX_PATH);
					if (stricmp(szDrive,pBuffer)!=0) continue;
					m_cMountlist.SetCheck(iItem,FALSE);
					RemoveMenu(szDrive);
					break;
				}
			}
		}
	}
	return TRUE;
}

BOOL CWinAfsLoadDlg::TerminateBackground(BOOL bDisplay,CString *emsg) 
{// if bDisplay==FALSE then skip error message and don't update connect buttons (power down needs this option)
// IF error and emsg!=NULL then return error messaage
	CString msg;
	CWait wait;
	if (!m_cAfs.Shutdown(msg))
	{
		HandleError(msg,bDisplay);
		m_cConnect.ModifyStyle(0,WS_DISABLED,0);
		m_cConnect.Invalidate();
		m_cCancel.SetWindowText("Exit");
		m_cCancel.Invalidate();
		m_trayIcon.SetConnectState(2);
		m_bServiceIsActive=FALSE;
		if (emsg) 
			*emsg=msg;
		return FALSE;
	}
	m_bServiceIsActive=FALSE;
	if (bDisplay)
		UpdateConnect();
	LOG("AFS Client Console stopped");
	return TRUE;
}

void CWinAfsLoadDlg::UpdateConnect()
{
	if (m_bRestartAFSD)
	{
		m_cConnect.ModifyStyle(WS_DISABLED,0);
		m_cAuthenicate.ModifyStyle(WS_DISABLED,0);
		return;
	}
	if (m_bServiceIsActive)
	{
		m_cConnect.SetWindowText("DisConnect");
		m_cConnect.Invalidate();
		m_cCancel.SetWindowText("Cancel");
		m_cCancel.Invalidate();
		if (!CWINAFSLOADAPP->m_bNoID)
		{	
			m_cAuthenicate.ModifyStyle(WS_DISABLED,0);
			if (IsGetTokens())
				m_cAuthenicate.SetWindowText("ReAuthenicate");
			 else 	//tokens are not gotten; allow authenication
				m_cAuthenicate.SetWindowText("Authenicate");
			m_cAuthenicate.Invalidate();
		}
		m_trayIcon.SetConnectState(0);
		return;
	}
	m_cConnect.SetWindowText("Connect");
	m_cConnect.Invalidate();
	m_cCancel.SetWindowText("Exit");
	m_cCancel.Invalidate();
	m_cAuthenicate.ModifyStyle(0,WS_DISABLED);
	m_cAuthenicate.Invalidate();
	m_trayIcon.SetConnectState(1);
}


void CWinAfsLoadDlg::OnAppOpen() 
{
	// TODO: Add your command handler code here
	//required so when icon menu is "Open" it will show the hidden application
	m_trayIcon.MaximiseFromTray(this);
//	ShowWindow(SW_SHOW);
}

// Up to 4 drives can be listed on the menu for the user to click on and start up explorer
void CWinAfsLoadDlg::OnTrayButton0() 
{
	OnSysCommand(IDM_EXPLORERAFS, 0);
}
void CWinAfsLoadDlg::OnTrayButton1() 
{
	OnSysCommand(IDM_EXPLORERAFS+16, 0);
}
void CWinAfsLoadDlg::OnTrayButton2() 
{
	OnSysCommand(IDM_EXPLORERAFS+32, 0);
}
void CWinAfsLoadDlg::OnTrayButton3() 
{
	OnSysCommand(IDM_EXPLORERAFS+48, 0);
}
void CWinAfsLoadDlg::OnTrayButton4() 
{
	OnSysCommand(IDM_EXPLORERAFS+64, 0);
}

#define MAXDRIVESIZE (SHARENAMESIZE+5)*MAXSHARES

BOOL CWinAfsLoadDlg::ProfileData(BOOL put)
{
	CString dINI;
	CString tINI;
	dINI.Format("%safsdsbmd.ini",CWINAFSLOADAPP->m_sTargetDir);
	tINI.Format("%safsdsbmt.ini",CWINAFSLOADAPP->m_sTargetDir);
	BOOL ret=TRUE;
	if (put)
	{
#define BLOCKSIZE MAX_PATH+SHARENAMESIZE+3
		int scur=BLOCKSIZE;
		char *sblock=(char *)malloc(scur);
		int sused=0;
		char *sptr=sblock;
		int dcur=128;
		char *dblock=(char *)malloc(dcur);
		int dused=0;
		char *dptr=dblock;
		for (int iItem=m_cMountlist.GetItemCount()-1;iItem>=0;iItem--)
		{
			CString sKey(m_cMountlist.GetItemText(iItem,COLSHARE));
			CString sPath(m_cMountlist.GetItemText(iItem,COLPATH));
			CString sDrive(m_cMountlist.GetItemText(iItem,COLDRIVE));
			CString sAuto(m_cMountlist.GetItemText(iItem,COLAUTO));
			if (!sKey.IsEmpty() && !sPath.IsEmpty())
			{// going a lot of trouble to handle any size buffer with out puting a limit on it
				int len=sKey.GetLength()+sDrive.GetLength()+sAuto.GetLength()+2;
				if (dused+len>=dcur)
					dblock=(char *)realloc(dblock,(dcur+=128));
				dptr=dblock+dused;
				wsprintf(dptr,"%s=%s%s",sKey,sAuto,sDrive);
				dused+=len;
				len=sKey.GetLength()+sPath.GetLength()+2;
				if (sused+len>=scur)
					sblock=(char *)realloc(sblock,(scur+=BLOCKSIZE));
				sptr=sblock+sused;
				wsprintf(sptr,"%s=%s",sKey,sPath);
				sused+=len;
			}
		}
		*(sblock+sused)=0;	//put extra null at the end
		*(dblock+dused)=0;
		WritePrivateProfileSection("AFS Submounts",sblock,tINI);
		WritePrivateProfileSection("AFS Drivemounts",dblock,dINI);
		delete dblock;
		delete sblock;
	} else {	//get
		char sShare[SHARENAMESIZE+1];
		CHAR sPath[MAX_PATH+1];
		CHAR sDriveMount[MAXDRIVESIZE+2];
		CHAR sDrive[DRIVESIZE+1];
		CHAR sAuto[AUTOSIZE+1];
		strcpy(sAuto," ");
		CString path;
		// lets scan for all and home first, we want to place them first
		//mode: 0=look for all, 1=look for home, 2=finish the rest		
		for(int mode=0;mode<3;mode++)
		{
			int drivelen=GetPrivateProfileString("AFS Drivemounts", NULL, "", sDriveMount, MAXDRIVESIZE,dINI);
			if (drivelen>=MAXDRIVESIZE)
			{
				CString msg;
				msg.Format("Profile String Error - Too many entries (%d)",MAXSHARES);
				HandleError(msg,TRUE);
				return FALSE;
			}
			PCHAR pDrivekey=sDriveMount;
			while (drivelen>1)
			{
				if ((strlen(pDrivekey)==0) || (strlen(pDrivekey)>SHARENAMESIZE))
				{
					HandleError("Profile String Error - AFS Drivemounts - Share Name",FALSE);
					ret=FALSE;
					break;
				}
				strcpy(sShare,pDrivekey);
				if (GetPrivateProfileString("AFS Drivemounts", sShare, "", sDrive, DRIVESIZE,dINI)==0)
				{
					HandleError("Profile String Error - AFS Drivemounts - Drive Name",FALSE);
					ret=FALSE;
					break;
				}
				if (sDrive[0]=='*')	//test for leading *
				{
					strcpy(sAuto,"*");
					strrev(sDrive);
					sDrive[strlen(sDrive)-1]=0;
				} else
					strcpy(sAuto," ");
				sDrive[1]=0;	//force to be single character
				strupr(sDrive);
				if (strspn(sDrive,"DEFGHIJKLMNOPQRSTUVWXYZ")==0)
				{
					HandleError("Profile String Error - AFS Drivemounts - Drive Letter",FALSE);
					ret=FALSE;
					break;
				}
				strcat(sDrive,":");
				switch (mode)
				{
				case 0:
					if (stricmp("all",sShare)!=0) break;
					if (GetPrivateProfileString("AFS Submounts", sShare, "", sPath, MAX_PATH,tINI)==0)
					{
						strcpy(sPath,"/");	//none defined
					}
					AddToList(sDrive,sPath,sShare,sAuto);
					drivelen=0;
					continue;

				case 1:
					if (stricmp("home",sShare)!=0) break;
					if (GetPrivateProfileString("AFS Submounts", sShare, "", sPath, MAX_PATH,tINI)==0)
					{
						HandleError("Profile String Error - AFS Drivemounts - Share Name",FALSE);
						ret=FALSE;
						break;
					}
					AddToList(sDrive,sPath,sShare,sAuto);
					drivelen=0;
					continue;
				default:
					if ((stricmp("all",sShare)==0)|| (stricmp("home",sShare)==0)) break;
					if (GetPrivateProfileString("AFS Submounts", sShare, "", sPath, MAX_PATH,tINI)==0)
					{
						HandleError("Profile String Error - AFS Drivemounts - Share Name",FALSE);
						ret=FALSE;
						break;
					}
					AddToList(sDrive,sPath,sShare,sAuto);
					break;
				}
				drivelen-=(strlen(pDrivekey)+1);
				pDrivekey+=(strlen(pDrivekey)+1);
			}
		}

	}
	return ret;
}

void CWinAfsLoadDlg::OnChange() 
{
	// TODO: Add your control notification handler code here

	POSITION pos = m_cMountlist.GetFirstSelectedItemPosition();
	if (pos == NULL) return;
	int nItem = m_cMountlist.GetNextSelectedItem(pos);
	UINT state=m_cMountlist.GetCheck(nItem);
	if (state)
	{
		MessageBox("You cannot change Item from the list while connected!","AFS - Warning",MB_OK|MB_ICONWARNING);
		return;
	}
	CChange dlg(TRUE,this);
	dlg.m_sDrive=m_cMountlist.GetItemText(nItem,COLDRIVE);
	dlg.m_sPath=m_cMountlist.GetItemText(nItem,COLPATH);
	dlg.m_sDescription=m_cMountlist.GetItemText(nItem,COLSHARE);
	dlg.m_bAuto=(strcmp(m_cMountlist.GetItemText(nItem,COLAUTO),"*")==0);
	if (dlg.DoModal()==IDCANCEL) return;
	m_cMountlist.SetItemText(nItem,COLDRIVE,dlg.m_sDrive);
	m_cMountlist.SetItemText(nItem,COLPATH,dlg.m_sPath);
	m_cMountlist.SetItemText(nItem,COLSHARE,dlg.m_sDescription);
	m_cMountlist.SetItemText(nItem,COLAUTO,(dlg.m_bAuto)?"*":" ");
	UpdateData(FALSE);
	ProfileData(TRUE);
}

VOID CWinAfsLoadDlg::AddToList(const char *sDrive,const char *sPath,const char *sShare,const char *sAuto)
{
	LV_ITEM         lvitem;
	memset(&lvitem,0,sizeof(lvitem));
	int iSubItem,iActualItem;
	int iItem;
	if (stricmp(sShare,"home")==0) 
		iItem=1;
	else if (stricmp(sShare,"all")==0)
		iItem=0;
	else {
		iItem=m_cMountlist.GetItemCount();
		if (iItem<1) iItem=2;
	}

	for (iSubItem = 0; iSubItem < NUMCOL; iSubItem++)
	{
		lvitem.mask = LVIF_TEXT ;//| (iSubItem == 0? LVIF_IMAGE : 0);
		lvitem.iItem = (iSubItem == 0)? iItem : iActualItem;
		lvitem.iSubItem = iSubItem;
		lvitem.iImage = 0;
		switch(iSubItem)
		{
		case COLDRIVE:
			lvitem.pszText=(char *)sDrive;
			break;
		case COLPATH:
			lvitem.pszText=(char *)sPath;
			break;
		case COLSHARE:
			lvitem.pszText=(char *)sShare;
			break;
		case COLAUTO:
			lvitem.pszText=(char *)sAuto;
			break;		
		default:
			break;
		}
		if (iSubItem == 0)
			iActualItem = m_cMountlist.InsertItem(&lvitem); // insert new item
		else
			m_cMountlist.SetItem(&lvitem); // modify existing item (the sub-item text)
	}

}

void CWinAfsLoadDlg::OnAdd() 
{
	// TODO: Add your control notification handler code here
	CChange dlg(FALSE,this);
	if (dlg.DoModal()==IDCANCEL) return;
	//return m_sDescription, m_sPath, m_sDrive
	AddToList(dlg.m_sDrive,dlg.m_sPath,dlg.m_sDescription,(dlg.m_bAuto)?"*":" ");
	UpdateData(FALSE);
	ProfileData(TRUE);									//update INI list if changed
}


void CWinAfsLoadDlg::OnRemove() 
{
	// TODO: Add your control notification handler code here
	
	POSITION pos = m_cMountlist.GetFirstSelectedItemPosition();
	if (pos == NULL) return;
	while (pos)
	{
		int nItem = m_cMountlist.GetNextSelectedItem(pos);
		UINT state=m_cMountlist.GetCheck(nItem);
		if (state)
		{
			HandleError("You cannot remove Item from the list while connected!");
			continue;
		}
#if 0
		if (stricmp(m_cMountlist.GetItemText(nItem,COLSHARE),"all")==0)
		{
			HandleError("You cannot remove 'All' Item from the list!");
			continue;
		}
#endif
		m_cMountlist.DeleteItem(nItem);
	}
	ProfileData(TRUE);
}

// FOLLOWING ROUITNE IS required so OnItemchangedDrivemountlist can tell the difference
// between a user changing an item to mount/dismount or the program is just updating the display
// unfortunately any changes go thourgh OnItemchangedDrivemountlist
void CWinAfsLoadDlg::OnClickDrivemountlist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	
	*pResult = 0;
	NM_LISTVIEW* pListview = (NM_LISTVIEW*)pNMHDR;
	CString msg;
	*pResult=0;
	m_iActiveItem=pListview->iItem;	//this is the only place where items changed are done by user click
}

void CWinAfsLoadDlg::UpdateMountDisplay() 
{
	TCHAR szDrive[] = TEXT("*:");
	TCHAR szMapping[ MAX_PATH ] = TEXT("");
 	LPTSTR pszSubmount = szMapping;
    DWORD dwSize = MAX_PATH;
	TCHAR pDrive[DRIVESIZE+1];
	TCHAR pShare[SHARENAMESIZE+1];
	ASSERT(strlen(m_cAfs.MountName())!=0);
	BOOL update=FALSE;
	for (int iItem=m_cMountlist.GetItemCount()-1;iItem>=0;iItem--)
	{
		m_cMountlist.GetItemText(iItem,COLDRIVE,pDrive,DRIVESIZE);
		m_cMountlist.GetItemText(iItem,COLSHARE,pShare,SHARENAMESIZE);
		if ((WNetGetConnection(pDrive, szMapping, &dwSize) == NO_ERROR) 
			&& (strstr(szMapping,m_cAfs.MountName())!=NULL))
		{
			if ((stricmp((strrchr(szMapping,'\\')+1),pShare)==0) 
				&& (!m_cMountlist.GetCheck(iItem)))
			{
				m_cMountlist.SetCheck(iItem,TRUE);
				AddMenu(m_cMountlist.GetItemText(iItem,COLDRIVE),m_cMountlist.GetItemText(iItem,COLSHARE));
				update=TRUE;
			}
		}
		else {
			if (m_cMountlist.GetCheck(iItem))
			{
				m_cMountlist.SetCheck(iItem,FALSE);
				RemoveMenu(m_cMountlist.GetItemText(iItem,COLDRIVE));
				update=TRUE;
			}
		}
	}
	if (update)
		m_cMountlist.Invalidate();
}

void CWinAfsLoadDlg::OnItemchangedDrivemountlist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pListview = (NM_LISTVIEW*)pNMHDR;
	CString msg;
	*pResult=0;
	if ((pListview->uNewState & (ITEMCHECKED | ITEMNOTCHECKED))!=(pListview->uOldState & 0x3000))
	{
		if (m_iActiveItem<0) return;	//no processing needed if not initiated by a mouse click
		m_iActiveItem=-1;
		switch (pListview->uNewState & (ITEMCHECKED | ITEMNOTCHECKED))
		{
		case ITEMCHECKED:	//mount a drive
			if (!m_bServiceIsActive)
			{// can't allow mounting if connection not active
				HandleError("You must connect first");
				m_cMountlist.SetCheck(pListview->iItem,FALSE);
				return;
			}
			LOG("Connect %s %s",m_cMountlist.GetItemText(pListview->iItem,COLDRIVE),m_cMountlist.GetItemText(pListview->iItem,COLSHARE));
			{
			CProgress progress(this,4);
			if (!m_cAfs.Mount(msg
				,m_cMountlist.GetItemText(pListview->iItem,COLDRIVE)
				,m_cMountlist.GetItemText(pListview->iItem,COLSHARE)))
			{
				m_cMountlist.SetCheck(pListview->iItem,FALSE);
				UpdateData(FALSE);
				HandleError(msg);
				return;
			}
			AddMenu(m_cMountlist.GetItemText(pListview->iItem,COLDRIVE),m_cMountlist.GetItemText(pListview->iItem,COLSHARE));
			}
			break;
		case ITEMNOTCHECKED:	//dismount a drive
			{
			BOOL force=FALSE;
			LOG("Disconnect %s",m_cMountlist.GetItemText(pListview->iItem,COLDRIVE));
			do
			{
				if (!m_cAfs.Dismount(msg,m_cMountlist.GetItemText(pListview->iItem,COLDRIVE),force))
				{// there was an error, allow the user to force closing the drive anyway
					CRetry dlg(TRUE);
					dlg.m_sMsg=msg;
					switch (dlg.DoModal())
					{
					case IDOK:
						continue;
					case IDC_FORCE:
						force=TRUE;
						continue;
					default:
						m_cMountlist.SetCheck(pListview->iItem,TRUE);
						return;
					}
				}
				RemoveMenu(m_cMountlist.GetItemText(pListview->iItem,COLDRIVE));
				break;
			} while (TRUE);
			}
			break;
		default:
			break;
		}
		return;
	}
	POSITION pos = m_cMountlist.GetFirstSelectedItemPosition();
	if (pos == NULL)
	{
		m_cRemove.ModifyStyle(0,WS_DISABLED,0);
		m_cChange.ModifyStyle(0,WS_DISABLED,0);
	} else {
		m_cRemove.ModifyStyle(WS_DISABLED,0,0);
		m_cMountlist.GetNextSelectedItem(pos);
		if (pos==NULL)
			m_cChange.ModifyStyle(WS_DISABLED,0,0);
		else
			m_cChange.ModifyStyle(0,WS_DISABLED,0);
	}
	m_cChange.Invalidate(TRUE);
	m_cRemove.Invalidate(TRUE);
	m_iActiveItem=-1;
}

void CWinAfsLoadDlg::BuildDriveList(BOOL newone)
{//if force then build a newone
	
	DWORD mapDrive=GetLogicalDrives();
	TCHAR szDrive[] = TEXT("*:  ");
	int iItem;
	if (newone)
	{
		m_Drivelist.RemoveAll();
		TCHAR szMapping[ MAX_PATH ] = TEXT("");
		LPTSTR pszSubmount = szMapping;
	    DWORD dwSize = MAX_PATH;
		TCHAR stDrive[]=TEXT("*:");
		for (iItem = 2;iItem <= 25; ++iItem)
		{
			szDrive[0]=stDrive[0]=iItem+'A';
			if (((mapDrive & 1<<iItem)==0)	// if drive is not in use or it is a network assigned drive
											//		then place it on the available drive list
				|| (WNetGetConnection (stDrive, szMapping, &dwSize) == NO_ERROR))
			{
				m_Drivelist.AddTail(CString(szDrive));
			}
		}
		return;
	}
	//update drive list seting * for thoes drives already in use
	for (iItem=m_Drivelist.GetCount()-1;iItem>=0;iItem--)
	{
		strcpy(szDrive,m_Drivelist.GetAt(m_Drivelist.FindIndex(iItem)));
		szDrive[3]= ((1<<(szDrive[0]-'A')) & mapDrive)?'*' :' ';
		m_Drivelist.SetAt(m_Drivelist.FindIndex(iItem),CString(szDrive));
	}
}

void CWinAfsLoadDlg::ExtractDrive(CString &zdrive,const char *request)
{// extract the cloest drive to the requested letter
	POSITION pos = m_Drivelist.Find(CString(request));
	if (pos)
	{
		zdrive=m_Drivelist.GetAt(pos);
		m_Drivelist.RemoveAt(pos);
		return;
	}
	int count=m_Drivelist.GetCount()-4;
	if (count<0) count=0;
	pos=m_Drivelist.FindIndex(count);
	zdrive=m_Drivelist.GetAt(pos);
	m_Drivelist.RemoveAt(pos);
}

HCURSOR CWinAfsLoadDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}


// Main menu close statement
BOOL CWinAfsLoadDlg::OnQueryEndSession( )
{
	ShowWindow(SW_SHOW);
	CString msg;
	if (!DismountAll(msg,1))// disconnect any connected AFS mounts, unless there are files open!
		return FALSE;
	TerminateBackground();
	if (m_nDirTimer)				//Destroy Window is not being called 
		KillTimer(m_nDirTimer);
	if (m_nAutTimer)
		KillTimer(m_nAutTimer);
	m_nAutTimer=m_nDirTimer=0;
	return !m_bServiceIsActive;
}

void CWinAfsLoadDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	char pBuffer[64];
	int len;
	char *p=NULL;
	CString msg;
	switch (nIDEvent)
	{
	case WM_DIRTIMER:
		switch (m_seqIndex)
		{
		case 22:
		case 25://1.0
			if ((m_seqCount+=TIMEINTERVAL)<1000) 
				break;
		case 23: //0.75
			if ((m_seqCount+=TIMEINTERVAL)<500)
				break;
		case 20:
		case 21:
		case 24:
		case 26://0.5
			if ((m_seqCount+=TIMEINTERVAL)<500)
				break;
		default://0.2
			m_seqCount=0;
			if (++m_seqIndex>MAXWORLDINDEX) m_seqIndex=0;
			m_bmpWorld.UpdateMask(this,m_WorldRect);//build new mask and force repaint of world area 
			break;
		}
		if ((!m_bServiceIsActive) || (m_bRestartAFSD)) break;
		if ((m_dirCount+=TIMEINTERVAL)<CHECKDIR) break;
		m_dirCount=0;
		len=::GetWindowText(m_cAfs.GetLoadWindowHandle(),pBuffer,64);
		if ((len==0) || ((p=strstr(pBuffer,"Finish"))!=NULL))
		{// if p!=NULL then application is finished but not totally shutdown
			if (p)
			{
				::SendMessage(m_cAfs.GetLoadWindowHandle(),WM_CLOSE,0,0);
				LOG("WM_CLOSE4");
			}
			CString msg;
			DismountAll(msg,3);		//unfortunately all drive references are invalid also!
			m_bServiceIsActive=FALSE;
			UpdateConnect();
			break;
		}
		if (!m_cCheckAdvanceDisplay.GetCheck())	break;
		if ((WS_VISIBLE & GetStyle()) )
			UpdateMountDisplay();
		break;
	case WM_AUTTIMER:
		if ((!m_bServiceIsActive) || m_bRestartAFSD) break;
		switch (m_cAfs.TestTokenTime(msg))
		{
		case 0:
			if (m_nShown==0) break;
			m_cAuthWarn.ShowWindow(SW_HIDE);
			m_cAuthWarn.Invalidate(TRUE);
			m_nShown=0;
			break;
		case -1:
			if (m_nShown==-1) break;
			HandleError(msg);
			ShowWindow(SW_SHOWDEFAULT);
			m_nShown=-1;
			break;
		case 1:
			if (m_nShown==1) break;
			m_bkBrush.DeleteObject();
			m_bkBrush.CreateSolidBrush(RGB(255,255,0));
			m_cAuthWarn.SetWindowText(msg);
			m_cAuthWarn.Invalidate(TRUE);
			m_cAuthWarn.ShowWindow(SW_SHOW);
			ShowWindow(SW_SHOWDEFAULT);
			m_nShown=1;
			LOG(msg);
			break;
		case 2:
			if (m_nShown==2) break;
			m_bkBrush.DeleteObject();
			m_bkBrush.CreateSolidBrush(RGB(255,0,0));
			m_cAuthWarn.ShowWindow(SW_SHOW);
			m_cAuthWarn.SetWindowText(msg);
			m_cAuthWarn.Invalidate(TRUE);
			ShowWindow(SW_SHOWDEFAULT);
			LOG(msg);
			m_nShown=2;
			break;
		default:
			break;
		}

		break;
	default:
		break;
	}
	CDialog::OnTimer(nIDEvent);
}

void CWinAfsLoadDlg::OnAuthenicate() 
{
	// TODO: Add your control notification handler code here
	CString msg;
	CWait wait;
	UpdateData(TRUE);
	m_nShown=0;
	m_cAuthWarn.ShowWindow(SW_HIDE);
	LOG("Re-Authenication");
	if (m_sUsername=="")
	{
		HandleError("You must enter a user name!");
		m_cUsername.SetFocus();
		return;
	}
	if (m_sPassword=="")
	{
		HandleError("You must enter a password!");
		m_cPassword.SetFocus();
		return;
	}
	if (!m_cAfs.Authencate(msg,m_sUsername,m_sPassword))
	{
		HandleError(msg);
		return;
	}
	m_cAuthenicate.SetWindowText("ReAuthenicate");
	m_cAuthenicate.Invalidate();
	if (!m_cSaveUsername.GetCheck())
	{
		m_sPassword.Empty();
		UpdateData(FALSE);
	}

}


void CWinAfsLoadDlg::OnDestroy() 
{

//    ASSERT( m_hBmpOld );
//    VERIFY( m_dcMem.SelectObject( CBitmap::FromHandle(m_hBmpOld) ) );
    // Need to DeleteObject() the bitmap that was loaded
    m_bmpWorld.DeleteObject();

    // m_dcMem destructor will handle rest of cleanup    

	if (m_nDirTimer)				//it is better to kill the timer here than PostNcDestroy
		KillTimer(m_nDirTimer);
	if (m_nAutTimer)
		KillTimer(m_nAutTimer);
	m_nAutTimer=m_nDirTimer=0;
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here
	
}

// Here is how we change the Background color for the yellow warning
HBRUSH CWinAfsLoadDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	
	// TODO: Change any attributes of the DC here
	if (pWnd->GetDlgCtrlID() == IDC_AUTHWARN)
	{
		pDC->SetBkMode(TRANSPARENT);	// we have to make text background transparent 
		return (HBRUSH)m_bkBrush;		// Return a different brush (YELLOW)
	}
	
	return hbr;
}

void CWinAfsLoadDlg::OnShow() 
{
	// TODO: Add your command handler code here
	CMenu* pSysMenu = GetSystemMenu(FALSE);

	if (pSysMenu != NULL)
	{
		if (0xFFFFFFFF!=pSysMenu->GetMenuState(IDM_CAPTUREWINDOW, MF_BYCOMMAND)) 
			return;
		CSettings dlg(this);
		dlg.DoModal();
		OnShowAddMenus();
	}
}

void CWinAfsLoadDlg::OnShowAddMenus() 
{
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu == NULL) return;
	CString strMenu;
	pSysMenu->AppendMenu(MF_SEPARATOR);
	strMenu.LoadString(IDM_CAPTUREWINDOW);
	pSysMenu->AppendMenu(MF_STRING, IDM_CAPTUREWINDOW, strMenu);
	strMenu.LoadString(IDM_CAPTUREFILE);
	pSysMenu->AppendMenu(MF_STRING, IDM_CAPTUREFILE, strMenu);
	strMenu.LoadString(IDM_VIEWAFS);
	pSysMenu->AppendMenu(MF_STRING, IDM_VIEWAFS, strMenu);
}

// Signaled when AFSD sends a socket message
LRESULT CWinAfsLoadDlg::OnPowerBroadcast(WPARAM wParam, LPARAM lParam)
{
	CString msg;
	CString sDrive;
	CString sKey;
	BOOL  result=TRUE;
	switch(wParam)
	{
	case PBT_APMSUSPEND:
		LOG("PBT_APMSUSPEND");
		break;
	case PBT_APMQUERYSUSPEND:
		// if lParam & 1 ==1 then you can prompt user for info
		LOG("PBT_APMQUERYSUSPEND");
#if WIN95TEST
		if (IsWin95())
		{// Windows 95
			CString *emsg=new CString;
			emsg->Format("Warning: Attempt to Suspend was Denied\nPower Suspend on Windows 95 is not compatable with Ufiler. \nYou may be required to reboot before using Ufilier again!");
			PostMessage(WM_ERRORMSG,AFS_EXITCODE_GENERAL_FAILURE,(LPARAM)emsg);

			return BROADCAST_QUERY_DENY;	//deny suspension
		}
#endif
		if (m_bRestartAFSD)
			break;
		if (m_bServiceIsActive)
		{

			if (
				(!IsWin95())	// WIN95 will suspend anyway
				&& (!m_cAfs.CheckNet(msg)) 
				
				)

			{
				if (lParam &1) 
				{
					CString *emsg=new CString;
					emsg->Format("Attempt to Suspend Denied\n%s",msg);
					PostMessage(WM_ERRORMSG,AFS_EXITCODE_GENERAL_FAILURE,(LPARAM)emsg);
				}
				LOG("AFS Client Console stopped, powerdown denied.");
				return BROADCAST_QUERY_DENY;	//files open suspend denied
			}
			CString *emsg=new CString;
			// Allow power suspension even though termination incomplete (no files open anyway)
			if (!TerminateBackground(FALSE,emsg))
			{
				if (lParam &1) 
				{
					m_cConnect.ModifyStyle(0,WS_DISABLED,0);
					m_cConnect.Invalidate();
					m_cCancel.SetWindowText("Exit");
					m_cCancel.Invalidate();
					m_trayIcon.SetConnectState(2);
					m_bServiceIsActive=FALSE;
					PostMessage(WM_ERRORMSG,AFS_EXITCODE_GENERAL_FAILURE,(LPARAM)emsg);
					result=FALSE;
				}
			} else
				delete emsg;
			m_bServiceIsActive=FALSE;
		}
		// terminate socket connection (even if terminatebackground failed
		REQUESTUISUSPENDEVENT(ONPARMDISCONNECT);
		{
		WPARAM wp;
		CString msg;
		CWINAFSLOADAPP->WaitForEvent(SOCKETIO+2000,&wp,&msg);	//wait 2 second longer than any socket IO
		switch (wp)
		{
		case AFS_EXITCODE_NORMAL:
			break;
		default:
			// have to post message to myself so I can return Querry deny immediately
			{
			CString *emsg=new CString(msg);
			m_cConnect.ModifyStyle(0,WS_DISABLED,0);
			m_cConnect.Invalidate();
			m_cCancel.SetWindowText("Exit");
			m_cCancel.Invalidate();
			PostMessage(WM_ERRORMSG,AFS_EXITCODE_GENERAL_FAILURE,(LPARAM)emsg);
			m_trayIcon.SetConnectState(2);
			result=FALSE;
			}
		}
		}
		m_bRestartAFSD=result;
		break;
	case PBT_APMRESUMESUSPEND:
		LOG("PBT_APMRESUMESUSPEND");
		REQUESTUIPOSTEVENT(WM_UICONNECT,WM_CONNECTRETURN,0);
		break;
	case PBT_APMRESUMECRITICAL:
		LOG("PBT_APMRESUMECRITICAL");
		REQUESTUIPOSTEVENT(WM_UICONNECT,WM_CONNECTRETURN,0);
		break;
	default:
		break;
	}
	UpdateConnect();
	return true;
}


// lParam will contain CString, they must be deleted when done!
LRESULT CWinAfsLoadDlg::OnErrorMessage(WPARAM wParam, LPARAM lParam)
{
	CString *msg=(CString *)lParam;
	MessageBox(*msg,"AFS Control Panel Warning");
	delete msg;
	return TRUE;
}

// actual message comes from AfsLoad (CWinApp)
LRESULT CWinAfsLoadDlg::OnAfsEvent(WPARAM wParam, LPARAM lParam)
{
	ASSERT(lParam);
	CString * msg=(CString *)lParam;
	CString emsg;
	emsg.Format("OnAfsEvent %x [%s]",wParam,(const char *)(*msg));
	LOG((const char *)emsg);
	switch (wParam)
	{
		case AFS_EXITCODE_NORMAL:
			break;
		default:
			MessageBox((const char*)(*msg),"AFS Client Console Failure");
			break;
	}
	if (msg)
		delete msg;
	return 0;
}


void CWinAfsLoadDlg::ErrorDisplayState()
{
	m_cConnect.ModifyStyle(0,WS_DISABLED,0);
	m_cConnect.Invalidate();
	m_cCancel.SetWindowText("Exit");
	m_cCancel.Invalidate();
	m_cAuthenicate.ModifyStyle(0,WS_DISABLED,0);
	m_cAuthenicate.Invalidate();
	UpdateConnect();
}

static CProgress *m_progress=NULL;

// all calls must have set lParam null or contain a message to release
// actual message comes from AfsLoad (CWinApp)
LRESULT CWinAfsLoadDlg::OnNotifyReturn(WPARAM wParam, LPARAM lParam)
{// fields messages from a NotifyFromUI, e.g. ONCONNECT ONPING
// lParam contains the message that must be released
	CString msg;
	int iItem;
	CString sDrive;
	CString sKey;
	switch(wParam)
	{
	case WM_CONNECTRETURN:
		if (lParam)
		{// we had a failure
			HandleError(*(CString *)lParam);
			delete (CString *)lParam;
			m_bRestartAFSD=FALSE;
			ErrorDisplayState();
			break;
		}
		if (!m_bRestartAFSD)
			break;
		m_progress = new CProgress(this,7);
		if (m_progress)
		{
			m_progress->SetTitle("Power Restart","Enable AFS Client Console","Authenication");
			m_progress->Next();
		}
		if (IsWin95())
		{
			LOG("Wait %d seconds to Bring up AFSD",m_PowerResumeDelay);
			return TRUE;		// do a return instead of break so Delete won't be called
		}
	case WM_RESUMEDELAY:
	case WM_PINGRETURN:	//we will return here after CONNECT(ONPING) finds the server connected
		LOG("Done with delay, load AFSD");
		if (m_progress)
			m_progress->Next();
		{
		m_bRestartAFSD=FALSE;
		if (lParam)
		{// we had a failure
			HandleError(*(CString *)lParam);
			delete (CString *)lParam;
			ErrorDisplayState();
			break;
		}
		if (m_progress)
			m_progress->Next();
		if (!m_cAfs.Create(msg,m_sComputername,m_procInfo)) 
		{
			HandleError(msg);
			m_procInfo.hThread=0;
			ErrorDisplayState();
			break;
		} else
			LOG("AFS Client Powerup started successfully.");
		for (iItem=0;iItem<m_cMountlist.GetItemCount();iItem++)
		{
				sKey = m_cMountlist.GetItemText(iItem,COLSHARE);
			if (stricmp(sKey,"all")!=0) continue;
			CString sAuto(m_cMountlist.GetItemText(iItem,COLAUTO));
			if (sAuto!="*")
				continue;
			sDrive=m_cMountlist.GetItemText(iItem,COLDRIVE);
			LOG("Connect %s %s",sDrive,sKey);
			m_cAfs.Dismount(msg,sDrive,TRUE);
			if (!m_cAfs.Mount(msg,sDrive,sKey))
			{
				CString msg2;
				m_cAfs.Shutdown(msg2);
				m_procInfo.hThread=0;
				msg2.Format("Connect can't continue: %s",msg);
				HandleError(msg2);
				ErrorDisplayState();
				return true;
			}
			AddMenu(sDrive,sKey);
			break;
		}
		if (m_progress)
			m_progress->Next();
		if (!m_cAfs.Authencate(msg,m_sUsername,m_sPassword))
		{
			CString msg2;
			DismountAll(msg,3);		//unfortunately all drive references are invalid also!
			m_cAfs.Shutdown(msg2);
			m_procInfo.hThread=0;
			HandleError(msg);
			ErrorDisplayState();
			if (m_progress)
				delete m_progress;
			return true;
		}
		m_bServiceIsActive=TRUE;
		}
		break;
	default:
		if (lParam)
			delete (CString *)lParam;
		break;
	}
	UpdateConnect();
	if (m_progress)
		delete m_progress;
	return true;
}


BOOL CWinAfsLoadDlg::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	// TODO: Add your message handler code here and/or call default
	::WinHelp(m_hWnd,CWINAFSLOADAPP->m_pszHelpFilePath,HELP_CONTEXT,IDH_MAIN);
	return TRUE;
}

void CWinAfsLoadDlg::OnHelpmain() 
{
	// TODO: Add your control notification handler code here
	::WinHelp(m_hWnd,CWINAFSLOADAPP->m_pszHelpFilePath,HELP_CONTEXT,IDH_MAIN);
	
}

void CWinAfsLoadDlg::OnSettings() 
{
	// TODO: Add your control notification handler code here
	CRegkey regkey("AFS\\Window");
	CCommandSettings dlg;
	dlg.m_ConnectOnStart=	CWINAFSLOADAPP->m_bConnect;
	dlg.m_LogToWindow=CWINAFSLOADAPP->m_bLogWindow;
	dlg.m_LogToFile=CWINAFSLOADAPP->m_bLog;
	dlg.m_UserName=m_sUsername;
	UINT tvar;
	DWORD size=sizeof(tvar);
	regkey.GetBinary("LoginTime",(LPBYTE)&tvar,size);
	dlg.m_uMaxLoginTime=tvar;
	dlg.m_uMaxPowerRestartDelay=m_PowerResumeDelay;
	if (dlg.DoModal()==IDOK)
	{
		if (dlg.m_LogToWindow ^ CWINAFSLOADAPP->m_bLogWindow) 
			CWINAFSLOADAPP->ShowLog(dlg.m_LogToWindow);
		if (dlg.m_LogToFile ^ CWINAFSLOADAPP->m_bLog)
			CWINAFSLOADAPP->ShowPrint(dlg.m_LogToFile);
		CWINAFSLOADAPP->m_bLogWindow=dlg.m_LogToWindow;
		CWINAFSLOADAPP->m_bLog=dlg.m_LogToFile;
		CWINAFSLOADAPP->m_bConnect=dlg.m_ConnectOnStart;
		CWINAFSLOADAPP->RegOptions(FALSE);
		tvar=dlg.m_uMaxLoginTime;
		regkey.PutBinary("LoginTime",(LPBYTE)&tvar,size);
		m_PowerResumeDelay=dlg.m_uMaxPowerRestartDelay;
		regkey.PutBinary("PowerResumeDelay",(LPBYTE)&m_PowerResumeDelay,size);
	}
}

