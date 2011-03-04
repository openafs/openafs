/*
 * Copyright 2010, Sine Nomine Associates.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_VOL_DAEMON_COM_INLINE_H
#define AFS_VOL_DAEMON_COM_INLINE_H

#include "daemon_com.h"

#define SYNC_ENUMCASE(en) \
    case en: return #en

static_inline char *
SYNC_res2string(afs_int32 response)
{
    switch (response) {
	SYNC_ENUMCASE(SYNC_OK);
	SYNC_ENUMCASE(SYNC_DENIED);
	SYNC_ENUMCASE(SYNC_COM_ERROR);
	SYNC_ENUMCASE(SYNC_BAD_COMMAND);
	SYNC_ENUMCASE(SYNC_FAILED);
    default:
	return "**UNKNOWN**";
    }
}

#undef SYNC_ENUMCASE

#endif /* AFS_VOL_DAEMON_COM_INLINE_H */
