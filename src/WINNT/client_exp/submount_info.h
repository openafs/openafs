/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _SUBMOUNTINFO_H_
#define _SUBMOUNTINFO_H_

#include <afxtempl.h>

enum SUBMT_INFO_STATUS { SIS_NULL, SIS_ADDED, SIS_CHANGED, SIS_DELETED };


class CSubmountInfo : public CObject
{
	SUBMT_INFO_STATUS m_Status;

	CString m_strShareName;
	CString m_strPathName;

public:
	CSubmountInfo();
	CSubmountInfo(const CString& strShareName, const CString& strPathName, SUBMT_INFO_STATUS status = SIS_NULL);
	CSubmountInfo(const CSubmountInfo& info);
	~CSubmountInfo();

	CString GetShareName() const			{ return m_strShareName; }
	CString GetPathName() const			{ return m_strPathName; }
	SUBMT_INFO_STATUS GetStatus() const		{ return m_Status; }

	void SetShareName(const CString& strShareName)	{ m_strShareName = strShareName; }
	void SetPathName(const CString& strPathName)	{ m_strPathName = strPathName; }
	void SetStatus(SUBMT_INFO_STATUS status)	{ m_Status = status; }
};

typedef CArray<CSubmountInfo*, CSubmountInfo*> SUBMT_INFO_ARRAY;

#endif

