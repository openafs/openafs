/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRCLIENTCACHE_H
#define TAAFSADMSVRCLIENTCACHE_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CreateCellCache (ASID idCell);
BOOL DestroyCellCache (ASID idCell);

LPASOBJPROP GetCachedProperties (ASID idCell, ASID idObject);

BOOL RefreshCachedProperties (UINT_PTR idClient, ASID idCell, ASID idObject, AFSADMSVR_GET_LEVEL GetLevel, ULONG *pStatus);
BOOL RefreshCachedProperties (UINT_PTR idClient, ASID idCell, LPASIDLIST pAsidList, AFSADMSVR_GET_LEVEL GetLevel, ULONG *pStatus);


#endif // TAAFSADMSVRCLIENTCACHE_H

