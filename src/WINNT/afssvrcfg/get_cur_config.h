/*
 * Copyright (C) 1998  Transarc Corporation.
 * All rights reserved.
 *
 */

#ifndef _GET_CUR_CONFIG_H_
#define _GET_CUR_CONFIG_H_

afs_status_t DoRootVolumesExist(BOOL& bExists);
afs_status_t AreRootVolumesReplicated(BOOL& bReplicated);

int GetCurrentConfig(HWND hParent, BOOL& bCanceled);

#endif	// _GET_CUR_CONFIG_H_

