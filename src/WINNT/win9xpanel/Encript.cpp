/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// Encription Module
#include "stdafx.h"
#include "WinAfsLoad.h"
#include "WinAfsLoadDlg.h"
#include "encript.h"


CEncript::CEncript(CWinAfsLoadDlg *pParent)
{
	m_pParent=pParent;
	m_hLibrary=LoadLibrary("ADVAPI32.dll");
	if (m_hLibrary)
	{
		m_pCryptAcquireContext=(PCRYPTACQUIRECONTEXT)GetProcAddress(m_hLibrary,"CryptAcquireContextA");
		m_pCryptCreateHash=(PCRYPTCREATEHASH)GetProcAddress(m_hLibrary,"CryptCreateHash");
		m_pCryptHashData=(PCRYPTHASHDATA)GetProcAddress(m_hLibrary,"CryptHashData");
		m_pCryptDeriveKey=(PCRYPTDERIVEKEY)GetProcAddress(m_hLibrary,"CryptDeriveKey");
		m_pCryptEncrypt=(PCRYPTENCRYPT)GetProcAddress(m_hLibrary,"CryptEncrypt");
		m_pCryptDecrypt=(PCRYPTDECRYPT)GetProcAddress(m_hLibrary,"CryptDecrypt");
		m_pCryptDestroyHash=(PCRYPTDESTROYHASH)GetProcAddress(m_hLibrary,"CryptDestroyHash");
		m_pCryptDestroyKey=(PCRYPTDESTROYKEY)GetProcAddress(m_hLibrary,"CryptDestroyKey");
		m_pCryptReleaseContext=(PCRYPTRELEASECONTEXT)GetProcAddress(m_hLibrary,"CryptReleaseContext");
		
		if ((m_pCryptAcquireContext==NULL)
			||(m_pCryptCreateHash==NULL)
			||(m_pCryptHashData==NULL)
			||(m_pCryptDeriveKey==NULL)
			||(m_pCryptEncrypt==NULL)
			||(m_pCryptDecrypt==NULL)
			||(m_pCryptDestroyHash==NULL)
			||(m_pCryptDestroyKey==NULL)
			||(m_pCryptReleaseContext==NULL)
			)
		{
			LOG("Incorrect ADVAPI32.DLL, Load failure");
			FreeLibrary(m_hLibrary);
			m_hLibrary=NULL;
		}
	}
}

CEncript::~CEncript()
{
	if (m_hLibrary)
		FreeLibrary(m_hLibrary);
	m_hLibrary=NULL;
}


// Encript pPassword
// if doEncript then encript else de encript
// on doEncript (true) dwSize sets maxmium size of encription output and pPassword is string to encript 
//		and output of encripted string with dwSize set to output size
// on not doEncript(false) dwSize sets size of pPassword input and pPassword is returned string

BOOL CEncript::Encript(LPCSTR pMachinename,LPCSTR pLoginName,LPCSTR pUsername,PBYTE pPassword,DWORD &dwSize,BOOL doEncript)
{
#define MY_ENCODING_TYPE  (PKCS_7_ASN_ENCODING | X509_ASN_ENCODING)

	BOOL bResult;
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
	HCRYPTKEY hKey;
	CString sKey;
	if (m_hLibrary==NULL)
	{
		if (doEncript)
			dwSize=strlen((char *)pPassword);
		return TRUE;
	}
	if(! (*m_pCryptAcquireContext)(
	   &hProv,               // Handle to the CSP
	   pLoginName,                  // Container name 
	   MS_DEF_PROV,               // Provider name
	   PROV_RSA_FULL,             // Provider type
	   0))                        // Flag values
	{ 
	//--------------------------------------------------------------------
	// Some sort of error occurred in acquiring the context. 
	// Create a new default key container. 

	   if(!(*m_pCryptAcquireContext) (
		  &hProv, 
		  pLoginName, 
		  MS_DEF_PROV, 
		  PROV_RSA_FULL, 
		  CRYPT_NEWKEYSET)) 
		{
		  m_pParent->HandleError("Could not create a new key container.\n");
		}
	} 	

	// Obtain handle to hash object.
	bResult = (*m_pCryptCreateHash)(
				hProv,             // Handle to CSP obtained earlier
				CALG_MD5,          // Hashing algorithm
				0,                 // Non-keyed hash
				0,                 // Should be zero
				&hHash);           // Variable to hold hash object handle 

	if (!bResult) { m_pParent->HandleError("Password Encription Error!");return FALSE;}
	// Hash data.
	sKey.Format("AFS%s%s%s",pLoginName,pUsername,pMachinename);
	bResult = (*m_pCryptHashData)(
				hHash,             // Handle to hash object
				(PBYTE)(const char *)sKey,         // Pointer to password
				dwSize,  // Length of data
				0);                // No special flags

	if (!bResult) { m_pParent->HandleError("Password Encription Error!");return FALSE;}

	// Create key from specified password.
	bResult = (*m_pCryptDeriveKey)(
				hProv,               // Handle to CSP obtained earlier.
				CALG_RC4,            // Use a stream cipher.
				hHash,               // Handle to hashed password.
				CRYPT_EXPORTABLE,    // Make key exportable.
				&hKey);              // Variable to hold handle of key.
	if (!bResult) { m_pParent->HandleError("Password Encription Error!");return FALSE;}

// Now encrypt data.
	if (doEncript)
	{
		bResult = (*m_pCryptEncrypt)(
					hKey,            // Key obtained earlier
					0,               // No hashing of data
					TRUE,            // Final or only buffer of data
					0,               // Must be zero
					pPassword,         // Data buffer
					&dwSize,         // Size of data
					dwSize);         // Size of block
		if (!bResult) { m_pParent->HandleError("Password Encription Error!");return FALSE;}

		// save password in encription area
	} else {
		bResult = (*m_pCryptDecrypt)(
					hKey,            // Key obtained earlier
					0,               // No hashing of data
					TRUE,            // Final or only buffer of data
					0,               // Must be zero
					pPassword,         // Data buffer
					&dwSize);         // Size of data
		if (!bResult) { m_pParent->HandleError("Password Encription Error!");return FALSE;}
	}
// Release hash object.
	(*m_pCryptDestroyHash)(hHash);
	(*m_pCryptDestroyKey)(hKey);
	(*m_pCryptReleaseContext)(hProv,0);
	return TRUE;
}

