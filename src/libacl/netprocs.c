/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	Information Technology Center
	Carnegie-Mellon University
*/


#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/ptclient.h>
#include "acl.h"

int
acl_HtonACL(struct acl_accessList *acl)
{
    /* Converts the access list defined by acl to network order.  Returns 0 always. */

    int i;
    if (htonl(1) == 1)
	return (0);		/* no swapping needed */
    for (i = 0; i < acl->positive; i++) {
	acl->entries[i].id = htonl(acl->entries[i].id);
	acl->entries[i].rights = htonl(acl->entries[i].rights);
    }
    for (i = acl->total - 1; i >= acl->total - acl->negative; i--) {
	acl->entries[i].id = htonl(acl->entries[i].id);
	acl->entries[i].rights = htonl(acl->entries[i].rights);
    }
    acl->size = htonl(acl->size);
    acl->version = htonl(acl->version);
    acl->total = htonl(acl->total);
    acl->positive = htonl(acl->positive);
    acl->negative = htonl(acl->negative);
    return (0);
}

int
acl_NtohACL(struct acl_accessList *acl)
{
    /* Converts the access list defined by acl to network order. Returns 0 always. */

    int i;
    if (ntohl(1) == 1)
	return (0);		/* no swapping needed */
    acl->size = ntohl(acl->size);
    acl->version = ntohl(acl->version);
    acl->total = ntohl(acl->total);
    acl->positive = ntohl(acl->positive);
    acl->negative = ntohl(acl->negative);
    for (i = 0; i < acl->positive; i++) {
	acl->entries[i].id = ntohl(acl->entries[i].id);
	acl->entries[i].rights = ntohl(acl->entries[i].rights);
    }
    for (i = acl->total - 1; i >= acl->total - acl->negative; i--) {
	acl->entries[i].id = ntohl(acl->entries[i].id);
	acl->entries[i].rights = ntohl(acl->entries[i].rights);
    }
    return (0);
}
