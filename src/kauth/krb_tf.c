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
    ("$Header: /cvs/openafs/src/kauth/krb_tf.c,v 1.6.2.2 2007/08/20 17:29:25 shadow Exp $");

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

#ifndef WORDS_BIGENDIAN
/* This was taken from jhutz's patch for heimdal krb4. It only
 * applies to little endian systems. Big endian systems have a
 * less elegant solution documented below.
 *
 * This record is written after every real ticket, to ensure that
 * both 32- and 64-bit readers will perceive the next real ticket
 * as starting in the same place.  This record looks like a ticket
 * with the following properties:
 *   Field         32-bit             64-bit
 *   ============  =================  =================
 *   sname         "."                "."
 *   sinst         ""                 ""
 *   srealm        ".."               ".."
 *   session key   002E2E00 xxxxxxxx  xxxxxxxx 00000000
 *   lifetime      0                  0
 *   kvno          0                  12
 *   ticket        12 nulls           4 nulls
 *   issue         0                  0
 */
static unsigned char align_rec[] = {
    0x2e, 0x00, 0x00, 0x2e, 0x2e, 0x00, 0x00, 0x2e,
    0x2e, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00,
    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

#else /* AFSLITTLE_ENDIAN */

/* This was taken from asedeno's patch for MIT Kerberos. These
 * alignment records are for big endian systems. We need more of them
 * because the portion of the 64-bit issue_date that overlaps with the
 * start of a ticket on 32-bit systems contains an unpredictable
 * number of NULL bytes. Preceeding these records is a second copy of
 * the 32-bit issue_date. The srealm for the alignment records is
 * always one of ".." or "?.."
 */

/* No NULL bytes
 * This is actually two alignment records since both 32- and 64-bit
 * readers will agree on everything in the first record up through the
 * issue_date size, except where sname starts.
 *   Field (1)     32-bit             64-bit
 *   ============  =================  =================
 *   sname         "????."            "."
 *   sinst         ""                 ""
 *   srealm        ".."               ".."
 *   session key   00000000 xxxxxxxx  00000000 xxxxxxxx
 *   lifetime      0                  0
 *   kvno          0                  0
 *   ticket        4 nulls           4 nulls
 *   issue         0                  0
 *
 *   Field (2)     32-bit             64-bit
 *   ============  =================  =================
 *   sname         "."                "."
 *   sinst         ""                 ""
 *   srealm        ".."               ".."
 *   session key   002E2E00 xxxxxxxx  xxxxxxxx 00000000
 *   lifetime      0                  0
 *   kvno          0                  12
 *   ticket        12 nulls           4 nulls
 *   issue         0                  0
 *
 */
static unsigned char align_rec_0[] = {
    0x2e, 0x00, 0x00, 0x2e, 0x2e, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x2e, 0x00, 0x00, 0x2e, 0x2e, 0x00,
    0x00, 0x2e, 0x2e, 0x00, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/* One NULL byte
 *   Field         32-bit             64-bit
 *   ============  =================  =================
 *   sname         "x"  |"xx"|"xxx"   "."
 *   sinst         "xx."|"x."|"."     ".."
 *   srealm        ".."               "..."
 *   session key   2E2E2E00 xxxxxxxx  xxxxxxxx 00000000
 *   lifetime      0                  0
 *   kvno          0                  12
 *   ticket        12 nulls           4 nulls
 *   issue         0                  0
 */
static unsigned char align_rec_1[] = {
    0x2e, 0x00, 0x2e, 0x2e, 0x00, 0x2e, 0x2e, 0x2e,
    0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00
};

/* Two NULL bytes
 *   Field         32-bit             64-bit
 *   ============  =================  =================
 *   sname         "x"  |"x" |"xx"    ".."
 *   sinst         ""   |"x" |""      ""
 *   srealm        "x.."|".."|".."    ".."
 *   session key   002E2E00 xxxxxxxx  xxxxxxxx 00000000
 *   lifetime      0                  0
 *   kvno          0                  12
 *   ticket        12 nulls           4 nulls
 *   issue         0                  0
 */
 static unsigned char align_rec_2[] = {
    0x2e, 0x2e, 0x00, 0x00, 0x2e, 0x2e, 0x00, 0xff,
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00,
    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Three NULL bytes
 * Things break here for 32-bit krb4 libraries that don't
 * understand this alignment record. We can't really do
 * anything about the fact that the three strings ended
 * in the duplicate timestamp. The good news is that this
 * only happens once every 0x1000000 seconds, once roughly
 * every six and a half months. We'll live.
 *
 * Discussion on the krbdev list has suggested the
 * issue_date be incremented by one in this case to avoid
 * the problem. I'm leaving this here just in case.
 *
 *   Field         32-bit             64-bit
 *   ============  =================  =================
 *   sname         ""                 "."
 *   sinst         ""                 ""
 *   srealm        ""                 ".."
 *   session key   2E00002E 2E00FFFF  xxxx0000 0000xxxx
 *   lifetime      0                  0
 *   kvno          4294901760         917504
 *   ticket        14 nulls           4 nulls
 *   issue         0                  0
 */
/*
static unsigned char align_rec_3[] = {
    0x2e, 0x00, 0x00, 0x2e, 0x2e, 0x00, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
*/
#endif /* AFSLITTLE_ENDIAN */

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
    if (write(fd, (char *)&(token.startTime), sizeof(afs_int32))
	!= sizeof(afs_int32))
	goto bad;
    close(fd);
    return 0;

    /* Alignment Record, from MIT Kerberos */
#ifndef WORDS_BIGENDIAN
    if (write(fd, align_rec, sizeof(align_rec)) != sizeof(align_rec))
	goto bad;
#else /* AFSLITTLE_ENDIAN */
    {
	int null_bytes = 0;
	if (0 == (token.startTime & 0xff000000))
	    ++null_bytes;
	if (0 == (token.startTime & 0x00ff0000))
	    ++null_bytes;
	if (0 == (token.startTime & 0x0000ff00))
	    ++null_bytes;
	if (0 == (token.startTime & 0x000000ff))
	    ++null_bytes;

	switch(null_bytes) {
	case 0:
	     /* Issue date */
	    if (write(fd, (char *) token.startTime, sizeof(afs_int32))
		!= sizeof(afs_int32))
		goto bad;
	    if (write(fd, align_rec_0, sizeof(align_rec_0))
		!= sizeof(align_rec_0))
		goto bad;
	    break;

	case 1:
	    /* Issue date */
	    if (write(fd, (char *) &token.startTime, sizeof(afs_int32))
		!= sizeof(afs_int32))
		goto bad;
	    if (write(fd, align_rec_1, sizeof(align_rec_1))
		!= sizeof(align_rec_1))
		goto bad;
	    break;

	case 3:
	    /* Three NULLS are troublesome but rare. We'll just pretend
	     * they don't exist by decrementing the token.startTime.
	     */
	    --token.startTime;
	case 2:
	    /* Issue date */
	    if (write(fd, (char *) &token.startTime, sizeof(afs_int32))
		!= sizeof(afs_int32))
		goto bad;
	    if (write(fd, align_rec_2, sizeof(align_rec_2))
		!= sizeof(align_rec_2))
		goto bad;
	    break;

	default:
	     goto bad;
	}
    }
#endif  /* AFSLITTLE_ENDIAN */
    close(fd);
    return 0;
      
    
  bad:
    close(fd);
    return -1;
}
