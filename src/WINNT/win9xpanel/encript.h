/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#ifndef __ENCRIPT__
#define __ENCRIPT__
#include <wincrypt.h>
typedef BOOL (WINAPI * PCRYPTACQUIRECONTEXT)(HCRYPTPROV *,LPCTSTR,LPCTSTR,DWORD,DWORD);
typedef BOOL (WINAPI * PCRYPTCREATEHASH)(HCRYPTPROV,ALG_ID,HCRYPTKEY,DWORD,HCRYPTHASH *);
typedef BOOL (WINAPI * PCRYPTHASHDATA)(HCRYPTHASH,BYTE *,DWORD,DWORD);
typedef BOOL (WINAPI * PCRYPTDERIVEKEY)(HCRYPTPROV,ALG_ID,HCRYPTHASH,DWORD,HCRYPTKEY *);
typedef BOOL (WINAPI * PCRYPTENCRYPT)(HCRYPTKEY,HCRYPTHASH,BOOL,DWORD,BYTE *,DWORD *,DWORD);
typedef BOOL (WINAPI * PCRYPTDECRYPT)(HCRYPTKEY,HCRYPTHASH,BOOL,DWORD,BYTE *,DWORD *);
typedef BOOL (WINAPI * PCRYPTDESTROYHASH)(HCRYPTHASH);
typedef BOOL (WINAPI * PCRYPTDESTROYKEY)(HCRYPTKEY);
typedef BOOL (WINAPI * PCRYPTRELEASECONTEXT)(HCRYPTPROV,DWORD);


class CEncript
{
public:
	CEncript(CWinAfsLoadDlg *pParent);
	~CEncript();
	CWinAfsLoadDlg *m_pParent;
	BOOL Encript(LPCSTR pMachinename,LPCSTR pLoginName,LPCSTR pUsername,PBYTE pPassword,DWORD &dwSize,BOOL doEncript);
	BOOL IsValid(){return (m_hLibrary!=NULL);}
private:
	HMODULE m_hLibrary;
	PCRYPTACQUIRECONTEXT m_pCryptAcquireContext;
	PCRYPTCREATEHASH m_pCryptCreateHash;
	PCRYPTHASHDATA m_pCryptHashData;
	PCRYPTDERIVEKEY m_pCryptDeriveKey;
	PCRYPTENCRYPT m_pCryptEncrypt;
	PCRYPTDECRYPT m_pCryptDecrypt;
	PCRYPTDESTROYHASH m_pCryptDestroyHash;
	PCRYPTDESTROYKEY m_pCryptDestroyKey;
	PCRYPTRELEASECONTEXT m_pCryptReleaseContext;
};

#endif