/* Copyright (C) 1998  Transarc Corporation.  All Rights Reserved.
 *
 */

#ifndef TRANSARC_SECUTIL_NT_H
#define TRANSARC_SECUTIL_NT_H

/* Security related utilities for the Windows platform */

#include <windows.h>
#include <aclapi.h>

typedef enum _WELLKNOWN_TRUSTEE_ID {
    WorldGroup = 1,
    LocalAdministratorsGroup
} WELLKNOWN_TRUSTEE_ID;

DWORD
ObjectDaclEntryAdd(HANDLE objectHandle,
		   SE_OBJECT_TYPE objectType,
		   WELLKNOWN_TRUSTEE_ID trustee,
		   DWORD accessPerm,
		   ACCESS_MODE accessMode,
		   DWORD inheritance);

#endif /* TRANSARC_SECUTIL_NT_H */
