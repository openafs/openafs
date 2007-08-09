/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 */

/* This modified from the code in kerberos/src/lib/krb/tf_util.c. */

/*
 * This file contains routines for manipulating the ticket cache file.
 *
 * The ticket file is in the following format:
 *
 *      principal's name        (null-terminated string)
 *      principal's instance    (null-terminated string)
 *      CREDENTIAL_1
 *      CREDENTIAL_2
 *      ...
 *      CREDENTIAL_n
 *      EOF
 *
 *      Where "CREDENTIAL_x" consists of the following fixed-length
 *      fields from the CREDENTIALS structure (see "krb.h"):
 *
 *              char            service[ANAME_SZ]
 *              char            instance[INST_SZ]
 *              char            realm[REALM_SZ]
 *              C_Block         session
 *              int             lifetime
 *              int             kvno
 *              KTEXT_ST        ticket_st
 *              afs_int32            issue_date
 *
 * . . .
 */

/* Inspite of what the above comment suggests the fields are not fixed length
   but null terminated as you might figure, except for the ticket which is
   preceded by a 4 byte length.  All fields in host order. 890306 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef AFS_NT40_ENV
#include <io.h>
#else
#include <sys/file.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <sys/types.h>
#include <rx/xdr.h>
#include <errno.h>
#include <afs/auth.h>
#include "kauth.h"
#include "kautils.h"

afs_int32
krb_write_ticket_file(realm)
     char *realm;
{
    char ticket_file[AFSDIR_PATH_MAX];
    int fd;
    int count;
    afs_int32 code;
    int lifetime, kvno;
    char *tf_name;
    struct ktc_principal client, server;
    struct ktc_token token;
    long mit_compat;	/* MIT Kerberos 5 with Krb4 uses a "long" for issue_date */

    if ((strlen(realm) >= sizeof(client.cell)))
	return KABADNAME;
    strcpy(server.name, KA_TGS_NAME);
    strcpy(server.instance, realm);
    lcstring(server.cell, realm, sizeof(server.cell));

    code = ktc_GetToken(&server, &token, sizeof(struct ktc_token), &client);
    if (code)
	return code;

    /* Use the KRBTKFILE environment variable if it exists, otherwise fall
     * back upon /tmp/tkt(uid}. 
     */
    if (tf_name = (char *)getenv("KRBTKFILE"))
	(void)sprintf(ticket_file, "%s", tf_name);
    else
	(void)sprintf(ticket_file, "%s/tkt%d", gettmpdir(), getuid());
    fd = open(ticket_file, O_WRONLY + O_CREAT + O_TRUNC, 0700);
    if (fd <= 0)
	return errno;

    /* write client name as file header */

    count = strlen(client.name) + 1;
    if (write(fd, client.name, count) != count)
	goto bad;

    count = strlen(client.instance) + 1;
    if (write(fd, client.instance, count) != count)
	goto bad;

    /* Write the ticket and associated data */
    /* Service */
    count = strlen(server.name) + 1;
    if (write(fd, server.name, count) != count)
	goto bad;
    /* Instance */
    count = strlen(server.instance) + 1;
    if (write(fd, server.instance, count) != count)
	goto bad;
    /* Realm */
    ucstring(server.cell, server.cell, sizeof(server.cell));
    count = strlen(server.cell) + 1;
    if (write(fd, server.cell, count) != count)
	goto bad;
    /* Session key */
    if (write(fd, (char *)&token.sessionKey, 8) != 8)
	goto bad;
    /* Lifetime */
    lifetime = time_to_life(token.startTime, token.endTime);
    if (write(fd, (char *)&lifetime, sizeof(int)) != sizeof(int))
	goto bad;
    /* Key vno */
    kvno = token.kvno;
    if (write(fd, (char *)&kvno, sizeof(int)) != sizeof(int))
	goto bad;
    /* Tkt length */
    if (write(fd, (char *)&(token.ticketLen), sizeof(int)) != sizeof(int))
	goto bad;
    /* Ticket */
    count = token.ticketLen;
    if (write(fd, (char *)(token.ticket), count) != count)
	goto bad;
    /* Issue date */
    mit_compat = token.startTime;
    if (write(fd, (char *)&mit_compat, sizeof(mit_compat))
	!= sizeof(mit_compat))
	goto bad;
    close(fd);
    return 0;

  bad:
    close(fd);
    return -1;
}
