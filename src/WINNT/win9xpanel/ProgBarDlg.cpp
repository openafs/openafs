/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// ProgBarDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MyFrame.h"
#include "WinAfsLoad.h"
#include "ProgBarDlg.h"
#include "share.h"
//#include <process.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CProgBarDlg dialog


CProgBarDlg::CProgBarDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CProgBarDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CProgBarDlg)
	m_sAuthenicateTime = _T("");
	m_sBackgroundTime = _T("");
	m_sMountTime = _T("");
	//}}AFX_DATA_INIT
	m_pParent=pParent;
}

CProgBarDlg::~CProgBarDlg()
{
	// nitwit do not expect any local variables to hang around
}

void CProgBarDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CProgBarDlg)
	DDX_Control(pDX, IDC_CHECKAUTHENICATION, m_cAuthenicate);
	DDX_Control(pDX, IDC_CHECKMOUNT, m_cMount);
	DDX_Control(pDX, IDC_CHECKBACKGROUND, m_cBackground);
	DDX_Control(pDX, IDC_STATUSREGION, m_cStatusRegion);
	DDX_Control(pDX, IDC_COMPLETE_MOUNT, m_cCompleteMount);
	DDX_Control(pDX, IDC_COMPLETE_ENABLE, m_cCompleteEnable);
	DDX_Control(pDX, IDC_COMPLETE_AUTH, m_cCompleteAuth);
	DDX_Text(pDX, IDC_TIMEAUTHENICATION, m_sAuthenicateTime);
	DDX_Text(pDX, IDC_TIMEBACKGROUND, m_sBackgroundTime);
	DDX_Text(pDX, IDC_TIMEMOUNT, m_sMountTime);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CProgBarDlg, CDialog)
	//{{AFX_MSG_MAP(CProgBarDlg)
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_MESSAGE( WM_PROGRESSPARM,OnParm)
	ON_WM_DESTROY()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WSA_EVENT, OnWSAEvent)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProgBarDlg message handlers

BOOL CProgBarDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	
	m_Option=0;
	SetFocus();
	m_uStatus=0;
	m_Socket = INVALID_SOCKET;
//	m_sLog="";
	m_cBackground.GetWindowText(m_sDefaultBackground.GetBuffer(32),31);
	m_cAuthenicate.GetWindowText(m_sDefaultAuthenicate.GetBuffer(32),32);
	m_cMount.GetWindowText(m_sDefaultMount.GetBuffer(32),32);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.


HCURSOR CProgBarDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CProgBarDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default

	CString msg;
	m_iOrientation=++m_iOrientation % 2;
	CRect s1;
	m_cStatusRegion.GetWindowRect(&s1);
	ScreenToClient(&s1);
	InvalidateRect(&s1,TRUE);
	switch (nIDEvent)
	{
	case WM_PROGRESSTIMER:
		{
		CTimeSpan elapsed=::CTime::GetCurrentTime()-m_StartTime;
		switch (m_Current)
		{
		case 1:
			m_sBackgroundTime.Format("%02d:%02d",elapsed.GetTotalMinutes()
				,elapsed.GetTotalSeconds()-60*elapsed.GetTotalMinutes());
			UpdateData(FALSE);
			break;
		case 2:
			m_sAuthenicateTime.Format("%02d:%02d",elapsed.GetTotalMinutes()
				,elapsed.GetTotalSeconds()-60*elapsed.GetTotalMinutes());
			UpdateData(FALSE);
			break;
		case 4:
			m_sMountTime.Format("%02d:%02d",elapsed.GetTotalMinutes()
				,elapsed.GetTotalSeconds()-60*elapsed.GetTotalMinutes());
			UpdateData(FALSE);
			break;
		default:
			break;
		}
		}
		break;
	default:
		break;
	}
	CDialog::OnTimer(nIDEvent);
}

LRESULT CProgBarDlg::OnParm(WPARAM wp, LPARAM lp)
{
	switch (wp)
	{
	case ProgFSetTitle:
		if (lp==0) break;
		{
		char *msg=(char *)lp;
		if (strlen(msg))
			m_cBackground.SetWindowText(msg);
		msg+=strlen(msg)+1;
		if (strlen(msg))
			m_cAuthenicate.SetWindowText(msg);
		msg+=strlen(msg)+1;
		if (strlen(msg))
			m_cMount.SetWindowText(msg);
		free((char *)lp);
		}
		break;
	case ProgFShow:
		ShowWindow(SW_SHOW);
		m_nTimer=SetTimer(WM_PROGRESSTIMER,1000,0);
		break;
	case ProgFHide:
		ShowWindow(SW_HIDE);
		KillTimer(m_nTimer);
		m_cBackground.SetWindowText(m_sDefaultBackground);
		m_cAuthenicate.SetWindowText(m_sDefaultAuthenicate);
		m_cMount.SetWindowText(m_sDefaultMount);
		break;
	case ProgFOpt:
		ShowWindow(SW_SHOW);
		m_Option=(UINT) lp;
		m_sBackgroundTime="";
		m_sAuthenicateTime="";
		m_sMountTime="";
		UpdateData(FALSE);
		m_iOrientation=0;
		if (m_Option & 1)
		{
			m_cBackground.ModifyStyle(WS_DISABLED,0,0);
		} else {
			m_cBackground.ModifyStyle(0,WS_DISABLED,0);
		}
		if (m_Option & 2)
			m_cAuthenicate.ModifyStyle(WS_DISABLED,0,0);
		else
			m_cAuthenicate.ModifyStyle(0,WS_DISABLED,0);
		if (m_Option & 4)
			m_cMount.ModifyStyle(WS_DISABLED,0,0);
		else
			m_cMount.ModifyStyle(0,WS_DISABLED,0);
		m_Current=0;
		m_nTimer=SetTimer(WM_PROGRESSTIMER,1000,0);
		ShowWindow(SW_SHOW);
		break;
	case ProgFNext:
		m_iOrientation=0;
		switch(m_Current)
		{
		case 0:
			m_Current=1;
			if (m_Option & m_Current)
			{
				CWnd* pWnd=GetDlgItem(IDC_TIMEBACKGROUND);
				if (pWnd)
					pWnd->SetFocus();
				m_StartTime=CTime::GetCurrentTime();
				OnTimer(WM_PROGRESSTIMER);
				break;
			}
		case 1:
			m_Current=2;
			if (m_Option & m_Current)
			{
				OnTimer(WM_PROGRESSTIMER);
				CWnd* pWnd=GetDlgItem(IDC_TIMEAUTHENICATION);
				if (pWnd)
					pWnd->SetFocus();
				m_StartTime=CTime::GetCurrentTime();
				break;
			}
		case 2:
			m_Current=4;
			if (m_Option & m_Current)
			{
				OnTimer(WM_PROGRESSTIMER);
				CWnd* pWnd=GetDlgItem(IDC_TIMEMOUNT);
				if (pWnd)
					pWnd->SetFocus();
				m_StartTime=CTime::GetCurrentTime();
				break;
			}
		default:
			m_Current=0;
			break;
		}
		break;
	default:
		break;
	}
	CRect s1;
	m_cStatusRegion.GetWindowRect(&s1);
	ScreenToClient(&s1);
	InvalidateRect(&s1,TRUE);
	return 0;
}

BOOL CProgBarDlg::Create()
{
	return CDialog::Create(IDD, m_pParent);
}

void CProgBarDlg::PostNcDestroy() 
{
	// TODO: Add your specialized code here and/or call the base class
	if (m_Socket != INVALID_SOCKET)
		WSAAsyncSelect(m_Socket, m_hWnd, 0, 0);	//cancel out messages
	if (m_Socket != INVALID_SOCKET)
		closesocket(m_Socket);
	delete this;
}

void CProgBarDlg::OnDestroy() 
{
	KillTimer(m_nTimer);
	Sleep(50);
	CDialog::OnDestroy();
	// TODO: Add your message handler code here
}

VOID CProgBarDlg::DisConnect()
{
	if (m_Socket != INVALID_SOCKET)
	{
		shutdown(m_Socket,3);
		char cBuffer[AFS_MAX_MSG_LEN];
		DWORD uStatus=1;
		int loop=SOCKETIO/250;
		while ((uStatus !=SOCKET_ERROR)&&(uStatus!=0) &&(loop-- >0))
		{
			Sleep(250);
			uStatus = recv(m_Socket, (char FAR *)&cBuffer, AFS_MAX_MSG_LEN, 0 );
		}
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
	WSAAsyncSelect(m_Socket, m_hWnd, 0, 0);	//cancel out messages
	LOG("Socket Disconnect");
}

typedef struct SoCkErR {
	int			soc_err;
	LPSTR		szSockErr;
} SOC_ERR;

SOC_ERR socket_error[] = {
	{ WSANOTINITIALISED	, "A successful WSAStartup must occur before using this function."},
	{ WSAENETDOWN		, 	"The Windows Sockets implementation has detected that the network "\
							"subsystem has failed."},
	{ WSAENOTCONN		, 	"The socket is not connected."},
	{ WSAEINTR			, 	"The (blocking) call was canceled using WSACancelBlockingCall."},
	{ WSAEINPROGRESS	, 	"A blocking Windows Sockets operation is in progress."},
	{ WSAENOTSOCK		, 	"The descriptor is not a socket."},
	{ WSAEOPNOTSUPP		, 	"MSG_OOB was specified, but the socket is not of type SOCK_STREAM."},
	{ WSAESHUTDOWN		, 	"The socket has been shut down; it is not possible to recv on a socket "\
							"after shutdown has been invoked with how set to 0 or 2."},
	{ WSAEWOULDBLOCK	,	"The socket is marked as nonblocking and the receive operation would block."},
	{ WSAEMSGSIZE		,	"The datagram was too large to fit into the specified buffer and was truncated."},
	{ WSAEINVAL			,	"The socket has not been bound with bind."},
	{ WSAECONNABORTED	,	"The virtual circuit was aborted due to timeout or other failure."},
	{ WSAECONNRESET		,	"The virtual circuit was reset by the remote side."},
	{ WSAEACCES			,	"The requested address is a broadcast address, but the appropriate flag was not set."},
	{ WSAEFAULT			,	"The buf argument is not in a valid part of the user address space."},
	{ WSAENETRESET		,	"The connection must be reset because the Windows Sockets implementation dropped it."},
	{ WSAENOBUFS		,	"The Windows Sockets implementation reports a buffer deadlock."},
	{ WSAENOTCONN		,	"The socket is not connected."},
	{ WSAEWOULDBLOCK	,	" The socket is marked as nonblocking and the requested operation would block."},
	{ -1				,	"Unknown Socket Error"}
};

LPCSTR show_socket_error()
{
	INT soc_err=WSAGetLastError();
	SOC_ERR	*lperr = &socket_error[0];
	while (1 && lperr->soc_err != -1)
	{
		if (lperr->soc_err == soc_err)
		{
			return lperr->szSockErr;
		}
		lperr++;
	}
	return "Unknown Socket Error";
}


BOOL CProgBarDlg::Connect(CString &sStatus)
{
	CHAR szBuff[80];
	UINT dwSize = sizeof(szBuff);
	SOCKADDR_IN socketAddress;
	m_Socket = socket( AF_INET, SOCK_DGRAM, 0);
	if (m_Socket == INVALID_SOCKET) {
		sStatus="Socket() failed";
		return FALSE;
	}
	if (gethostname(szBuff, dwSize)!=0)
	{
		sStatus.Format("Get Host Name - %s", show_socket_error());
		return FALSE;
	}
	socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
	socketAddress.sin_port = htons(AFS_MSG_PORT);        /* Convert to network ordering */
	if (bind(m_Socket, (struct sockaddr FAR *) &socketAddress, sizeof(socketAddress)) == SOCKET_ERROR) {
		sStatus.Format("Bind :%s", show_socket_error());
		return FALSE;
	}
	if ((m_uStatus = WSAAsyncSelect( m_Socket,GetSafeHwnd(), WSA_EVENT, FD_READ | FD_CLOSE )) > 0) {
		WSAAsyncSelect( m_Socket, GetSafeHwnd(), 0, 0);
		sStatus.Format("Get Host Name: ", show_socket_error());
		return FALSE;
	}
	m_bReadyToSend=TRUE;
	sStatus="Success Connect";
	LOG("Socket Connect");
	return TRUE;
}

LRESULT CProgBarDlg::OnWSAEvent(WPARAM wParam, LPARAM lParam)
{
	CString sLog;
	if (WSAGETSELECTERROR( lParam ) != 0) { 
//		m_sLog.Format("Read Failure status=%d\r\n", WSAGETSELECTERROR(lParam));
		WSAAsyncSelect( m_Socket,GetSafeHwnd(), 0, 0);
		m_uStatus=WSAGETSELECTERROR( lParam );
		LOG("WSA Select error");
		return 0;
	}
	switch (WSAGETSELECTEVENT(lParam))
	{
	case FD_READ:
		wParam=Decode(wParam,sLog);
		LOG("OnWSAEvent FD_READ(%d)",wParam);
		CWINAFSLOADAPP->WSANotifyFromUI(wParam,(const char *)sLog);
		break;
	case FD_CLOSE:
		LOG("OnWSAEvent FD_CLOSE(%d)",wParam);
		break;
	case FD_WRITE:
		LOG("OnWSAEvent FD_WRITE(%d)",wParam);
		break;
	default:
		LOG("OnWSAEvent Unknown Response");
		break;
	}
	return 0;
}

// this routine will set the notfication return (wParam) for WinAfsLoadDlg to receive
// if problem with decoding result return AFS_EXITCODE_GENERAL_FAILURE
// Other returns are:
//		AFS_EXITCODE_NORMAL(0) ,AFS_EXITCODE_PANIC(2) ,AFS_EXITCODE_NETWORK_FAILURE(3)
UINT CProgBarDlg::Decode(WPARAM wParam,CString &sLog)
{
	m_bReadyToSend=FALSE;
	m_uStatus = recv((SOCKET)wParam, (char FAR *)&m_buffer, AFS_MAX_MSG_LEN, 0 );
	if (m_uStatus==0){
		sLog.Format("Receive Error: %s", show_socket_error());
		return AFS_EXITCODE_GENERAL_FAILURE;
	}
	SOCKETTYPE *p=(SOCKETTYPE *)m_buffer;
	switch (p->header.msgtype)
	{
	case AFS_MSG_STATUS_CHANGE:
		{
		_afsMsg_statChange *sp=(_afsMsg_statChange *)p;
		switch (sp->exitCode)
		{
		case AFS_EXITCODE_NORMAL:
			{
			if ((m_uStatus = WSAAsyncSelect( m_Socket,GetSafeHwnd(),WSA_EVENT, FD_READ | FD_CLOSE )) > 0) {
				WSAAsyncSelect( m_Socket, GetSafeHwnd(), 0, 0);
				sLog.Format("WSAAsyncSelect: %s", show_socket_error());
				return AFS_EXITCODE_GENERAL_FAILURE;
			}
			int len=sp->hdr.length-sizeof(*sp);
			if (len>0)
			{
				char *x=new char[len+1];
				strncpy(x,&(sp->message),len);
				*(x+len)=0;
				sLog.Format("AFSD operation normal Message#%s#",x);
				delete x;
			} else {
				sLog="AFSD operation normal";
			}
			}
			break;
		case AFS_EXITCODE_PANIC:
			sLog="AFS Client Console Panic Exit";
			break;
		case AFS_EXITCODE_NETWORK_FAILURE:
			sLog="AFS Client Network Failure Exit";
			break;
		default:
			sLog.Format("Receive Error: Messsage format(exit code)=%0x",p->change.exitCode);
			p->change.exitCode=AFS_EXITCODE_GENERAL_FAILURE;
			break;
		}
		return (p->change.exitCode);
		}
	case AFS_MSG_PRINT:
		if ((m_uStatus = WSAAsyncSelect( m_Socket, GetSafeHwnd(),WSA_EVENT, FD_READ | FD_CLOSE )) > 0) {
			WSAAsyncSelect( m_Socket, GetSafeHwnd(), 0, 0);
			sLog.Format("WSAAsyncSelect: %s", show_socket_error());
			return AFS_EXITCODE_GENERAL_FAILURE;
		}
		return AFS_EXITCODE_GENERAL_FAILURE;
	default:
		sLog.Format("Receive Error: Messsage format=%0x",p->header.msgtype);
		return AFS_EXITCODE_GENERAL_FAILURE;
	}
}



void CProgBarDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	//m_iCurrent is the Current item (1,2,4)
	//m_iOption is which are active
	
	CBrush brBack,brGreen,brYellow;
	brBack.CreateSolidBrush(GetSysColor(COLOR_MENU));
	brGreen.CreateSolidBrush(RGB(0,255,0));
	brYellow.CreateSolidBrush(RGB(255,255,0));
	CRect shape;
	switch (m_Current)
	{
	case 1:	//progress
		SetShape(shape,m_Current,m_iOrientation);
		dc.SelectObject(&brYellow);
		dc.Ellipse(shape);
		SetShape(shape,m_Option & 2,0);
		if (shape.Width())
		{
			dc.SelectObject(&brBack);
			dc.Ellipse(shape);
		}
		SetShape(shape,m_Option & 4,0);
		if (shape.Width())
		{
			dc.SelectObject(&brBack);
			dc.Ellipse(shape);
		}
		break;
	case 2:
		SetShape(shape,m_Option & 1,0);
		if (shape.Width())
		{
			dc.SelectObject(&brGreen);
			dc.Ellipse(shape);
		}
		SetShape(shape,m_Current,m_iOrientation);
		if (shape.Width())
		{
			dc.SelectObject(&brYellow);
			dc.Ellipse(shape);
		}
		SetShape(shape,m_Option & 4,0);
		if (shape.Width())
		{
			dc.SelectObject(&brBack);
			dc.Ellipse(shape);
		}
		break;
	case 4:
		SetShape(shape,m_Option & 1,0);
		if (shape.Width())
		{
			dc.SelectObject(&brGreen);
			dc.Ellipse(shape);
		}
		SetShape(shape,m_Option & 2,0);
		if (shape.Width())
		{
			dc.SelectObject(&brGreen);
			dc.Ellipse(shape);
		}
		SetShape(shape,m_Current,m_iOrientation);
		if (shape.Width())
		{
			dc.SelectObject(&brYellow);
			dc.Ellipse(shape);
		}
		break;
	default:
		break;
	}
	// Do not call CDialog::OnPaint() for painting messages
}

void CProgBarDlg::SetShape(CRect &shape,int option,int orientation)
{
	CRect s1,s2,rect;
	switch(option)
	{
	case 1:
		m_cCompleteEnable.GetWindowRect(&s1);
		break;
	case 2:
		m_cCompleteAuth.GetWindowRect(&s1);
		break;
	case 4:
		m_cCompleteMount.GetWindowRect(&s1);
		break;
	default:
		s1=CRect(0,0,0,0);
		break;
	}
	ScreenToClient(&s1);
	int delta=(s1.Width()-s1.Height())/2;
	s2.top=s1.top-delta;
	s2.bottom=s1.bottom+delta;
	s2.left=s1.left+delta;
	s2.right=s1.right-delta;
	rect.top=s2.top;
	rect.bottom=s2.bottom;
	rect.left=s1.left;
	rect.right=s1.right;
	switch (orientation)
	{
	case 1:
		shape=s2;
		break;
	default:
		shape=s1;
		break;
	}
}

