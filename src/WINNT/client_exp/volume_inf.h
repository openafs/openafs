/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

#ifndef _VOLUME_INF_H_
#define _VOLUME_INF_H_

class CVolInfo
{
public:
	CString m_strFilePath;
	CString m_strFileName;
	CString m_strName;
	LONG m_nID;
	LONG m_nQuota;
	LONG m_nNewQuota;
	LONG m_nUsed;
	LONG m_nPartSize;
	LONG m_nPartFree;
	int m_nDup;
	CString m_strErrorMsg;
};


#endif // _VOLUME_INF_H_

