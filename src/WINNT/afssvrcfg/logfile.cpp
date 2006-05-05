/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES ___________________________________________________________________
 *
 */
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

extern "C" {
#include <afs\afs_utilAdmin.h>
}

#include <windows.h>
#include <stdio.h>
#include "logfile.h"
#include <WINNT\talocale.h>
#include <time.h>


/*
 * MEMBER FUNCTIONS _________________________________________________________
 *
 */
LOGFILE::LOGFILE()
{
	m_fp = 0;
}

LOGFILE::~LOGFILE()
{
	if (m_fp)
		Close();
}

BOOL LOGFILE::Open(const char *pszLogFilePath, 
				   LOGFILE_OPEN_MODE eOpenMode,	
				   LOGFILE_TIMESTAMP_MODE eTimeStampMode)
{
	char *pszOpenMode;

	m_eTimeStampMode = eTimeStampMode;

	if (eOpenMode == OM_OVERWRITE)
		pszOpenMode = "w";
	else
		pszOpenMode = "a+";

	strcpy(m_szPath, pszLogFilePath);
	
	m_fp = fopen(pszLogFilePath, pszOpenMode);
	if (m_fp) {
		if (m_eTimeStampMode != TSM_NEVER)
			WriteTimeStamp();
		fprintf(m_fp, "Log file open.\r\n");

		return TRUE;
	}

	return FALSE;
}

BOOL LOGFILE::Close()
{
	int nResult = 0;
	
	if (m_fp) {
		if (m_eTimeStampMode != TSM_NEVER)
			WriteTimeStamp();
		fprintf(m_fp, "Closing log file.\r\n");
		nResult = fclose(m_fp);
		if (nResult == 0)
			m_fp = 0;
	}

	return (nResult == 0);
}

BOOL LOGFILE::Write(const char *pszEntry, ...)
{
	static BOOL bTimestampNextLine = TRUE;

	if (!m_fp)
		return FALSE;

	if (bTimestampNextLine && (m_eTimeStampMode == TSM_EACH_ENTRY))
		WriteTimeStamp();
	
	va_list args;
	
	va_start(args, pszEntry);

	int nWritten = vfprintf(m_fp, pszEntry, args);

	va_end(args);

	fflush(m_fp);

	// Don't timestamp next line unless current line ended with a newline
	bTimestampNextLine = (pszEntry[strlen(pszEntry) - 1] == '\n');

	return (nWritten > 0);
}

BOOL LOGFILE::WriteError(const char *pszMsg, DWORD nErrorCode, ...)
{
	if (!m_fp)
		return FALSE;

	if (m_eTimeStampMode == TSM_EACH_ENTRY)
		WriteTimeStamp();
	
	va_list args;
	
	va_start(args, nErrorCode);

	int nWritten = vfprintf(m_fp, pszMsg, args);
	va_end(args);

	if (nWritten < 1)
		return FALSE;

	afs_status_t nStatus;
	const char *pszErrorText;

	int nResult = util_AdminErrorCodeTranslate(nErrorCode, TaLocale_GetLanguage(), &pszErrorText, &nStatus);
	if (nResult)
		fprintf(m_fp, ":  (0x%lx), %s.\r\n", nErrorCode, pszErrorText);
	else
		fprintf(m_fp, ":  (0x%lx).\r\n", nErrorCode);

	fflush(m_fp);

	return (nWritten > 0);
}

BOOL LOGFILE::WriteTimeStamp()
{
	if (!m_fp)
		return FALSE;

	char szTime[64], szDate[64];

	_strtime(szTime);
	_strdate(szDate);

	fprintf(m_fp, "%s %s:  ", szTime, szDate);
	
	return TRUE;
}

BOOL LOGFILE::WriteBoolResult(BOOL bResult)
{
	if (!m_fp)
		return FALSE;

	fprintf(m_fp, "%s.\r\n", bResult ? "Yes" : "No");

	fflush(m_fp);

	return TRUE;
}

BOOL LOGFILE::WriteMultistring(const char *pszMultiStr)
{
	if (!m_fp)
		return FALSE;

	for (const char *p = pszMultiStr; *p; p += strlen(p))
		Write("%s\r\n", p);

	return TRUE;
}

