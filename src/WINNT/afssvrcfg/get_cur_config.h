/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _GET_CUR_CONFIG_H_
#define _GET_CUR_CONFIG_H_

afs_status_t DoRootVolumesExist(BOOL& bExists);
afs_status_t AreRootVolumesReplicated(BOOL& bReplicated);

int GetCurrentConfig(HWND hParent, BOOL& bCanceled);

#endif	// _GET_CUR_CONFIG_H_

