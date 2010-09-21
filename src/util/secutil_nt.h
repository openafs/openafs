/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_SECUTIL_NT_H
#define OPENAFS_SECUTIL_NT_H

/* Security related utilities for the Windows platform */

#include <windows.h>
#include <aclapi.h>

typedef enum _WELLKNOWN_TRUSTEE_ID {
    WorldGroup = 1,
    LocalAdministratorsGroup
} WELLKNOWN_TRUSTEE_ID;

DWORD ObjectDaclEntryAdd(HANDLE objectHandle, SE_OBJECT_TYPE objectType,
			 WELLKNOWN_TRUSTEE_ID trustee, DWORD accessPerm,
			 ACCESS_MODE accessMode, DWORD inheritance);

#endif /* OPENAFS_SECUTIL_NT_H */
