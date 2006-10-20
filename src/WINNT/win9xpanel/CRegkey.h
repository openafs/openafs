/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
//Cregkey.h

#ifndef __REGKEY__
#define __REGKEY__

class CRegkey
{
public:
	CRegkey(HKEY hKey,const char *subkey,const char *skey);
	CRegkey(const char *);
	~CRegkey();
	HKEY &Getkey(){return m_kSkey;}
	BOOL GetString(const char *field,CString &buffer,DWORD len);
	BOOL PutString(const char *field,const char *pBuffer);
	BOOL GetBinary(const char *field,LPBYTE msg,DWORD &len);
	BOOL PutBinary(const char *field,LPBYTE msg,DWORD len);
	BOOL Enumerate();
private:
	HKEY m_kPkey;
	HKEY m_kSkey;
	DWORD m_dwIndex;
};

#endif