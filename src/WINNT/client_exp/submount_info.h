/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
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

