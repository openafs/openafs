/*
 * Copyright (C) 1998  Transarc Corporation.
 * All rights reserved.
 *
 */

#ifndef	_ADMIN_INFO_DLG_H_
#define _ADMIN_INFO_DLG_H_

// Each option includes the ones before it.  
enum GET_ADMIN_INFO_OPTIONS { GAIO_LOGIN_ONLY,  GAIO_GET_SCS };

BOOL GetAdminInfo(HWND hParent, GET_ADMIN_INFO_OPTIONS eOptions);

#endif	// _ADMIN_INFO_DLG_H_

