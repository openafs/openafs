/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_ADMIN_INFO_DLG_H_
#define _ADMIN_INFO_DLG_H_

// Each option includes the ones before it.  
enum GET_ADMIN_INFO_OPTIONS { GAIO_LOGIN_ONLY,  GAIO_GET_SCS };

BOOL GetAdminInfo(HWND hParent, GET_ADMIN_INFO_OPTIONS eOptions);

#endif	// _ADMIN_INFO_DLG_H_

