/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <windows.h>
#include <stdio.h>

enum LOGFILE_TIMESTAMP_MODE { TSM_NEVER, TSM_AT_OPEN_AND_CLOSE, TSM_EACH_ENTRY };
enum LOGFILE_OPEN_MODE		{ OM_OVERWRITE, OM_APPEND };


class LOGFILE
{
	FILE *m_fp;

	char m_szPath[MAX_PATH];

	LOGFILE_TIMESTAMP_MODE m_eTimeStampMode;

public:
	LOGFILE();
	~LOGFILE();

	BOOL Open(	const char				*pszLogFilePath, 
				LOGFILE_OPEN_MODE		eOpenMode = OM_OVERWRITE,
				LOGFILE_TIMESTAMP_MODE	eTimeStampMode = TSM_EACH_ENTRY
			 );
	BOOL Close();

	char *GetPath()	{ return m_szPath; }

	BOOL Write(const char *pszMsg, ...);

	BOOL WriteError(const char *pszMsg, DWORD nErrorCode, ...);

	BOOL WriteMultistring(const char *pszMultiStr);

	BOOL WriteTimeStamp();

	BOOL WriteBoolResult(BOOL bResult);
};

