/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "stdafx.h"
#include "submount_info.h"


CSubmountInfo::CSubmountInfo()
{
	m_Status = SIS_NULL;
}

CSubmountInfo::CSubmountInfo(const CString& strShareName, const CString& strPathName, SUBMT_INFO_STATUS status)
{
	m_Status = status;
	
	SetShareName(strShareName);
	SetPathName(strPathName);
}

CSubmountInfo::CSubmountInfo(const CSubmountInfo& info)
{
	SetStatus(info.GetStatus());
	SetShareName(info.GetShareName());
	SetPathName(info.GetPathName());
}	

CSubmountInfo::~CSubmountInfo()
{
}

