/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#include "stdafx.h"
#include "cregkey.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRegkey::CRegkey(HKEY hKey,const char *subkey,const char *skey)
{
	DWORD disposition,result;
	m_kPkey=m_kSkey=0;
	result=(RegCreateKeyEx(hKey	//HKEY_LOCAL_MACHINE
		,subkey			//"Software\\IBM"
		,0,NULL
		,REG_OPTION_NON_VOLATILE
		,KEY_ALL_ACCESS,NULL
		,&m_kPkey
		,&disposition)==ERROR_SUCCESS);
	if(!result)
	{
		AfxMessageBox("AFS Error - Could Not create a registration key!");
		m_kPkey=0;
	}
	result=(RegCreateKeyEx(m_kPkey
		,skey
		,0,NULL
		,REG_OPTION_NON_VOLATILE
		,KEY_ALL_ACCESS,NULL
		,&m_kSkey,&disposition)==ERROR_SUCCESS);
	if(!result)
	{
		RegCloseKey(m_kPkey);
		AfxMessageBox("AFS Error - Could Not create a registration key!");
		m_kPkey=m_kSkey=0;
	}
	m_dwIndex=0;
}

BOOL CRegkey::Enumerate()
{
	DWORD result;
	CString lpName;
	DWORD dwLen=128;
	char *p=lpName.GetBuffer(dwLen);
	FILETIME ftLastWriteTime;
	result=RegEnumKeyEx(m_kSkey,// handle to key to enumerate
	m_dwIndex++,              // subkey index
	p,              // subkey name
	&dwLen,            // size of subkey buffer
	NULL,         // reserved
	NULL,             // class string buffer
	NULL,           // size of class string buffer
	&ftLastWriteTime // last write time
	);
	switch (result)
	{
	case ERROR_NO_MORE_ITEMS:
		return TRUE;
	case ERROR_SUCCESS:
		return TRUE;
	default:
		AfxMessageBox("AFS Error - Could Not enumerate a registration key!");
		return FALSE;
	}
	return TRUE;
}


CRegkey::CRegkey(const char *skey)
{
	DWORD disposition,result;
	m_kPkey=m_kSkey=0;
	result=(RegCreateKeyEx(HKEY_CURRENT_USER
		,"Software\\IBM"
		,0,NULL
		,REG_OPTION_NON_VOLATILE
		,KEY_ALL_ACCESS,NULL
		,&m_kPkey
		,&disposition)==ERROR_SUCCESS);
	if(!result)
	{
		AfxMessageBox("AFS Error - Could Not create a registration key!");
		m_kPkey=0;
	}
	result=(RegCreateKeyEx(m_kPkey
		,skey
		,0,NULL
		,REG_OPTION_NON_VOLATILE
		,KEY_ALL_ACCESS,NULL
		,&m_kSkey,&disposition)==ERROR_SUCCESS);
	if(!result)
	{
		RegCloseKey(m_kPkey);
		AfxMessageBox("AFS Error - Could Not create a registration key!");
		m_kPkey=m_kSkey=0;
	}
}

BOOL CRegkey::GetString(const char *field,CString &buffer,DWORD len)
{
	if (m_kSkey==0) return FALSE;
	UCHAR *pBuffer=(UCHAR *)buffer.GetBuffer(len);
	DWORD type;
	if (RegQueryValueEx(m_kSkey
		,field,0,&type
		,pBuffer
		,&len)!=ERROR_SUCCESS)
	{
		buffer.ReleaseBuffer();
		return FALSE; //assume never been set
	}
	ASSERT(type==REG_SZ);
	buffer.ReleaseBuffer(len);
	return TRUE;
}

BOOL CRegkey::PutString(const char *field,const char *pBuffer)
{
	if (m_kSkey==0)
		return FALSE;
	if ((pBuffer)&&(strlen(pBuffer)))
		return (RegSetValueEx(m_kSkey,field,0,REG_SZ,(CONST BYTE *)pBuffer,strlen(pBuffer))==ERROR_SUCCESS);
	return (RegSetValueEx(m_kSkey,field,0,REG_SZ,(CONST BYTE *)"",0)==ERROR_SUCCESS);
}


BOOL CRegkey::GetBinary(const char *field,LPBYTE msg,DWORD &len)
{
	if (m_kSkey==0)
		return FALSE;
	DWORD type;
	if (RegQueryValueEx(m_kSkey
			,field,0,&type
			,msg
			,&len)!=ERROR_SUCCESS)
	{
		return FALSE; //assume never been set
	}
	ASSERT(type==REG_BINARY);
	return TRUE;
}

BOOL CRegkey::PutBinary(const char *field,LPBYTE msg,DWORD len)
{
	CHAR empty[1];
	empty[0]=0;
	if (m_kSkey==0)
		return FALSE;
	if (msg)
		return (RegSetValueEx(m_kSkey,field,0,REG_BINARY,msg,len)==ERROR_SUCCESS);
	return (RegSetValueEx(m_kSkey,field,0,REG_BINARY,(LPBYTE)&empty,0)==ERROR_SUCCESS);
}

CRegkey::~CRegkey()
{
	if (m_kSkey)
		RegCloseKey(m_kSkey);
	if (m_kPkey)
		RegCloseKey(m_kPkey);
}

