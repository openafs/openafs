// GetWebDll.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "GetWebDll.h"
#include "getwebdllfun.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

/////////////////////////////////////////////////////////////////////////////
// CGetWebDllApp

BEGIN_MESSAGE_MAP(CGetWebDllApp, CWinApp)
	//{{AFX_MSG_MAP(CGetWebDllApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGetWebDllApp construction

CGetWebDllApp::CGetWebDllApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CGetWebDllApp object

CGetWebDllApp theApp;


LPCTSTR pszURL = NULL;
BOOL    bStripMode = FALSE;
BOOL    bProgressMode = FALSE;
DWORD   dwAccessType = PRE_CONFIG_INTERNET_ACCESS;

DWORD dwHttpRequestFlags =
	INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_AUTO_REDIRECT;
//
//GET /updateCellServDB.jsp?type=bunk HTTP/1.1
//Accept: */*
//Accept -Language: en -us
//Accept -Encoding: gzip , deflate
//User-Agent: ufilerNativeClient/1.0 (1.0; Windows NT)
//Host: 192.168.0.1
//Connection: Keep -Alive

const TCHAR szVersion[] =
	_T("HTTP/1.1");

const TCHAR szHeaders[] =
	_T("\
Accept: */*\r\n\
Accept -Language: en -us\r\n\
Accept -Encoding: gzip , deflate\r\n\
User-Agent: ufilerNativeClient/1.0 (1.0; Windows NT)\r\n\
Host: 192.168.0.1\r\n\
Connection: Keep -Alive\r\n");

CTearSession::CTearSession(LPCTSTR pszAppName, int nMethod)
	: CInternetSession(pszAppName, 1, nMethod)
{
}

void CTearSession::OnStatusCallback(DWORD /* dwContext */, DWORD dwInternetStatus,
	LPVOID /* lpvStatusInfomration */, DWORD /* dwStatusInformationLen */)
{
	if (!bProgressMode)
		return;

	if (dwInternetStatus != INTERNET_STATUS_CONNECTED_TO_SERVER)
		AfxMessageBox("Connection Not Made",MB_ICONERROR | MB_OK);
	return;
}

/////////////////////////////////////////////////////////////////////////////
// CTearException -- used if something goes wrong for us

// TEAR will throw its own exception type to handle problems it might
// encounter while fulfilling the user's request.

IMPLEMENT_DYNCREATE(CTearException, CException)

CTearException::CTearException(int nCode)
	: m_nErrorCode(nCode)
{
}

void ThrowTearException(int nCode)
{
	CTearException* pEx = new CTearException(nCode);
	throw pEx;
}

// StripTags() rips through a buffer and removes HTML tags from it.
// The function uses a static variable to remember its state in case
// a HTML tag spans a buffer boundary.

void StripTags(LPTSTR pszBuffer)
{
	static BOOL bInTag = FALSE;
	LPTSTR pszSource = pszBuffer;
	LPTSTR pszDest = pszBuffer;

	while (*pszSource != '\0')
	{
		if (bInTag)
		{
			if (*pszSource == '>')
				bInTag = FALSE;
			pszSource++;
		}
		else
		{
			if (*pszSource == '<')
				bInTag = TRUE;
			else
			{
				*pszDest = *pszSource;
				pszDest++;
			}
			pszSource++;
		}
	}
	*pszDest = '\0';
}


extern "C" 
__declspec(dllexport) INT GetWebPage(LPSTR lpErrMsg,LPSTR lpFile,LPSTR lpCmdLine)
{
	CString emsg;
//	emsg.Format("p1=[%s],p2=[%s]",lpFile,lpCmdLine);
//	AfxMessageBox(emsg,MB_ICONERROR | MB_OK);
	if ((strlen(lpCmdLine)==0) || (strlen(lpFile)==0))
	{
		emsg="Parameter Error";
		return 1;
	}

	int nRetCode = 0;

	CTearSession session(_T("TEAR - MFC Sample App"), dwAccessType);
	CHttpConnection* pServer = NULL;
	CHttpFile* pFile = NULL;
	char *szParm=strstr(lpCmdLine,"?");
	try
	{
		// check to see if this is a reasonable URL
		CFile ofile(lpFile,CFile::modeCreate|CFile::modeWrite);

		CString strServerName;
		CString strObject;
		INTERNET_PORT nPort;
		DWORD dwServiceType;

		if (!AfxParseURL(lpCmdLine, dwServiceType, strServerName, strObject, nPort) ||
			dwServiceType != INTERNET_SERVICE_HTTP)
		{
			emsg="Error: can only use URLs beginning with http://";
			ThrowTearException(1);
		}

		if (bProgressMode)
		{
			VERIFY(session.EnableStatusCallback(TRUE));
		}

		pServer = session.GetHttpConnection(strServerName, nPort);
		pFile = pServer->OpenRequest(
			CHttpConnection::HTTP_VERB_GET,
			strObject,		//updateCellServDB.jsp
			NULL,			// URL of document
			1,				// context
			NULL,
			szVersion,
			dwHttpRequestFlags);
		pFile->AddRequestHeaders(szHeaders);
		pFile->SendRequest();

		DWORD dwRet;
		pFile->QueryInfoStatusCode(dwRet);

		// if access was denied, prompt the user for the password

		if (dwRet == HTTP_STATUS_DENIED)
		{
			DWORD dwPrompt;
			dwPrompt = pFile->ErrorDlg(NULL, ERROR_INTERNET_INCORRECT_PASSWORD,
				FLAGS_ERROR_UI_FLAGS_GENERATE_DATA | FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS, NULL);

			// if the user cancelled the dialog, bail out

			if (dwPrompt != ERROR_INTERNET_FORCE_RETRY)
			{
				emsg="Access denied: Invalid password";
				ThrowTearException(1);
			}

			pFile->SendRequest();
			pFile->QueryInfoStatusCode(dwRet);
		}

		CString strNewLocation;
		pFile->QueryInfo(HTTP_QUERY_RAW_HEADERS_CRLF, strNewLocation);

		// were we redirected?
		// these response status codes come from WININET.H

		if (dwRet == HTTP_STATUS_MOVED ||
			dwRet == HTTP_STATUS_REDIRECT ||
			dwRet == HTTP_STATUS_REDIRECT_METHOD)
		{
			CString strNewLocation;
			pFile->QueryInfo(HTTP_QUERY_RAW_HEADERS_CRLF, strNewLocation);

			int nPlace = strNewLocation.Find(_T("Location: "));
			if (nPlace == -1)
			{
				emsg="Error: Site redirects with no new location";
				ThrowTearException(2);
			}

			strNewLocation = strNewLocation.Mid(nPlace + 10);
			nPlace = strNewLocation.Find('\n');
			if (nPlace > 0)
				strNewLocation = strNewLocation.Left(nPlace);

			// close up the redirected site

			pFile->Close();
			delete pFile;
			pServer->Close();
			delete pServer;

			if (bProgressMode)
			{
				emsg.Format("Caution: redirected to %s",(LPCTSTR) strNewLocation);
			}

			// figure out what the old place was
			if (!AfxParseURL(strNewLocation, dwServiceType, strServerName, strObject, nPort))
			{
				emsg="Error: the redirected URL could not be parsed.";
				ThrowTearException(2);
			}

			if (dwServiceType != INTERNET_SERVICE_HTTP)
			{
				emsg="Error: the redirected URL does not reference a HTTP resource.";
				ThrowTearException(2);
			}

			// try again at the new location
			pServer = session.GetHttpConnection(strServerName, nPort);
			pFile = pServer->OpenRequest(CHttpConnection::HTTP_VERB_GET,
				strObject, NULL, 1, NULL, NULL, dwHttpRequestFlags);
			pFile->AddRequestHeaders(szHeaders);
			pFile->SendRequest();

			pFile->QueryInfoStatusCode(dwRet);
			if (dwRet != HTTP_STATUS_OK)
			{
				emsg.Format("Error: Got status code %d",dwRet);
				ThrowTearException(2);
			}
		}


		TCHAR sz[1024];
		while (pFile->ReadString(sz, 1023))
		{
			if (bStripMode)
				StripTags(sz);
			ofile.Write(sz,strlen(sz));
		}

		pFile->Close();
		pServer->Close();
		ofile.Close();
	}
	catch (CInternetException* pEx)
	{
		// catch errors from WinINet

		TCHAR szErr[1024];
		pEx->GetErrorMessage(szErr, 1024);

		CString emsg;
		emsg.Format("Error: (%s)",szErr);
		nRetCode = 2;
		pEx->Delete();
	}
	catch (CFileException* pEx)
	{
		TCHAR szErr[1024];
		pEx->GetErrorMessage(szErr, 1024);

		emsg.Format("File Error: (%s)",szErr);
		nRetCode = 2;
		pEx->Delete();
	}
	catch (CTearException* pEx)
	{
		// catch things wrong with parameters, etc

		nRetCode = pEx->m_nErrorCode;
		pEx->Delete();
	}

	if (pFile != NULL)
		delete pFile;
	if (pServer != NULL)
		delete pServer;
	session.Close();
	int len=strlen(lpErrMsg);
	strncpy(lpErrMsg,emsg,len);
	lpErrMsg[len]=0;
	return nRetCode;
}

extern "C" 
__declspec(dllexport) INT GetUserLogon(LPSTR lpUserName)
{
	int nRetCode = 1;
	ULONG nSize=strlen(lpUserName);
	if (!GetUserName(lpUserName,&nSize)) nRetCode=0;
	return nRetCode;
}

extern "C" 
__declspec(dllexport) INT BrowseFile(HWND hwndOwner,LPSTR lpstrTitle,LPSTR lpFileName,INT size)
{
	char *xptr;
//	char msg[256];
	char *ptr=strrchr(lpFileName,'\\');
	int nFileOffset=0;
	int nFileExtension=0;
	if (ptr)
		nFileOffset=ptr-lpFileName+1;
	else {
		ptr=strrchr(lpFileName,':');
		if (ptr)
			nFileOffset=ptr-lpFileName;
	}
	if (ptr==NULL)
		ptr=lpFileName;
	if (xptr=strrchr(ptr,'.'))
		nFileExtension=nFileOffset+(xptr-ptr);
//	sprintf(msg,"Title: [%s] filename=[%s], %i,%i,%i",lpstrTitle,lpFileName,nFileOffset,nFileExtension,size);
//	AfxMessageBox(msg,MB_OK);
	OPENFILENAME data={
		sizeof(OPENFILENAME)	//lStructSize
		,hwndOwner				//hwndOwner
		,NULL					//
		,"*.*"					//lpstrFilter
		,NULL					//lpstrCustomFilter
		,NULL					//nMaxCustFilter
		,0						//nFilterIndex
		,lpFileName			//lpstrFile
		,size					//nMaxFile - at least 256 characters
		,NULL					//lpstrFileTitle
		,0						//nMaxFileTitle
		,NULL					//lpstrInitialDir
		,lpstrTitle				//lpstrTitle
		,OFN_HIDEREADONLY|OFN_PATHMUSTEXIST		//Flags
		,nFileOffset			//nFileOffset
		,nFileExtension			//nFileExtension
		,NULL					//lpstrDefExt
		,NULL					//lCustData
		,NULL					//lpfnHook
		,NULL					//lpTemplateName
	};
	return GetOpenFileName(&data);
}