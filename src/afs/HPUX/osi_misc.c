/*
 * (C) COPYRIGHT IBM CORPORATION 1988, 1999
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

/* 
 * osi_misc.c
 *
 * Implements:
 * afs_suser
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 *
 * Here we have to save and restore errno since the HP-UX suser() sets errno.
 */

afs_suser() {
    int save_errno;
    int code;

    save_errno = u.u_error;
    code = suser();
    u.u_error = save_errno;

    return code;
}
