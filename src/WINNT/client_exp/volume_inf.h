/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
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

