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
 */

afs_suser() {
    int error;

    if ((error = suser(u.u_cred, &u.u_acflag)) == 0) {
	return(1);
    }
    return(0);
}
