#ifndef TAAFSADMSVRCLIENTCACHE_H
#define TAAFSADMSVRCLIENTCACHE_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CreateCellCache (ASID idCell);
BOOL DestroyCellCache (ASID idCell);

LPASOBJPROP GetCachedProperties (ASID idCell, ASID idObject);

BOOL RefreshCachedProperties (DWORD idClient, ASID idCell, ASID idObject, AFSADMSVR_GET_LEVEL GetLevel, ULONG *pStatus);
BOOL RefreshCachedProperties (DWORD idClient, ASID idCell, LPASIDLIST pAsidList, AFSADMSVR_GET_LEVEL GetLevel, ULONG *pStatus);


#endif // TAAFSADMSVRCLIENTCACHE_H

