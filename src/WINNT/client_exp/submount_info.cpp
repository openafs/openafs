/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

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

