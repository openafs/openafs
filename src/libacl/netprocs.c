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

#include <errno.h>

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

/*!
 * Sanity check the access list.
 *
 * \param acl   pointer to the acl structure to be checked
 *
 * \returns  0 if acl list is valid
 *   \retval 0       success
 *   \retval EINVAL  invalid values in the acl header
 */
static int
CheckAccessList(struct acl_accessList *acl)
{
    if (acl->total < 0 || acl->total > ACL_MAXENTRIES) {
	return EINVAL;
    }
    if (acl->positive < 0 || acl->negative < 0
	|| (acl->positive + acl->negative) != acl->total) {
	return EINVAL;
    }
    /* Note: size may exceed sizeof(struct acl_accessList). */
    if (acl->size < sizeof(struct acl_accessList)
	+ (acl->total - 1) * sizeof(struct acl_accessEntry)) {
	return EINVAL;
    }
    return 0;
}

/*!
 * Convert the access list to network byte order.
 *
 * \param acl   pointer to the acl structure to be converted
 *
 * \returns  zero on success
 *   \retval 0       success
 *   \retval EINVAL  invalid values in the acl header
 */
int
acl_HtonACL(struct acl_accessList *acl)
{
    int i;
    int code;

    code = CheckAccessList(acl);
    if (code) {
	return code;
    }

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

/*!
 * Convert the access list to host byte order.
 *
 * \param acl   pointer to the acl structure to be converted
 *
 * \returns  zero on success
 *   \retval 0       success
 *   \retval EINVAL  invalid values in the acl header
 */
int
acl_NtohACL(struct acl_accessList *acl)
{
    int i;
    int code;

    acl->size = ntohl(acl->size);
    acl->version = ntohl(acl->version);
    acl->total = ntohl(acl->total);
    acl->positive = ntohl(acl->positive);
    acl->negative = ntohl(acl->negative);

    code = CheckAccessList(acl);
    if (code) {
	return code;
    }

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
