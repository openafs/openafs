/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file contains routines common to both the client and the server-
 * primarily an interface routine to the pioctl call
 * and a routine for setting the primary flag in the token structure in
 * order to create a new PAG while doing the SET TOKEN. In addition there are
 * debugging routines of interest to both the client and server processes
 */

#include "apache_afs_utils.h"
#include "apache_afs_utils.h"

/*
 * do_pioctl does the actual call to pioctl (or equivalent for that platform)
 * sets up the ViceIoctl buffer with all the parameters required
 * NOTE: in_buffer and out_buffer may point to the same memory buffer
 */
int
do_pioctl(char *in_buffer, int in_size, char *out_buffer, int out_size,
	  int opcode, char *path, int followSymLinks)
{
    struct ViceIoctl iob;
    iob.in = in_buffer;
    iob.in_size = in_size;
    iob.out = out_buffer;
    iob.out_size = out_size;

#ifdef AFS_USR_SUN5_ENV
    return syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, _VICEIOCTL(opcode),
		   &iob, followSymLinks);
#else /* AFS_USR_SUN5_ENV */
    return lpioctl(path, _VICEIOCTL(opcode), &iob, followSymLinks);
#endif /* AFS_USR_SUN5_ENV */
}

int
do_setpag()
{
    int code;

    do {
	code = lsetpag();
    } while (code && errno == EINTR);

    return code;
}

/*
 * Return 1 if we have a token, 0 if we don't
 */
int
haveToken()
{
    char temp[1024];
    afs_int32 i = 0;
    int code;

    memcpy((void *)temp, (void *)&i, sizeof(afs_int32));
    code =
	do_pioctl(temp, sizeof(afs_int32), temp, sizeof(temp), VIOCGETTOK,
		  NULL, 0);
    if (code)
	return 0;
    else
	return 1;
}


/*
 * flipPrimary sets the primary cell longword -
 * enabling a SETPAG if the same buffer is used with VICESETTOK
 */
flipPrimary(char *tokenBuf)
{
    afs_int32 i;
    char *temp = tokenBuf;

    /* skip over the secret token */
    memcpy(&i, temp, sizeof(afs_int32));
    temp += (i + sizeof(afs_int32));

    /* skip over the clear token */
    memcpy(&i, temp, sizeof(afs_int32));
    temp += (i + sizeof(afs_int32));

    /* set the primary flag */
    memcpy(&i, temp, sizeof(afs_int32));
    i |= 0x8000;
    memcpy(temp, &i, sizeof(afs_int32));
    temp += sizeof(afs_int32);
    return 0;
}

/* Returns the AFS pag number, if any, otherwise return -1 */
afs_int32
getPAG()
{
    afs_int32 pag;

    assert(sizeof(afs_uint32) == 4);
    assert(sizeof(afs_int32) == 4);

    pag = ktc_curpag();
    if (pag == 0 || pag == -1)
	return -1;

    /* high order byte is always 'A'; actual pag value is low 24 bits */
    return (pag & 0xFFFFFF);
}

/*
 * Write out the formatted string to the error log if the specified level
 * is greater than or equal to the global afsDebugLevel which is set by
 * the directive SetAFSDebugLevel in the httpd.conf file
 */
/* VARARGS1 */
void
afsLogError(a, b, c, d, e, f, g, h, i, j, k, l, m, n)
     char *a;
     char *b;
     char *c;
     char *d;
     char *e;
     char *f;
     char *g;
     char *h;
     char *i;
     char *j;
     char *k;
     char *l;
     char *m;
     char *n;
{
    char reason[1024];

    sprintf(reason, a, b, c, d, e, f, g, h, i, j, k, l, m, n);
    /*  LOG_REASON(reason,r->uri,r); */
    strcat(reason, "\n");
    fprintf(stderr, reason);
}


/* the following are debug utilities */

void
hexDump(char *tbuffer, int len)
{
    int i;
    int byte;
    char *temp = tbuffer;

    fprintf(stderr, "HEXDUMP:\n");
    for (i = 0; i < len; i++) {
	byte = *temp;
	temp++;
	fprintf(stderr, "%x", byte);
    }
    fprintf(stderr, "\n");
}

/*
 * debugging utility to see if the group id changes - if SETPAG worked
 * call this before and after the routine to dosetpag and verify results
 */

int
printGroups()
{
    int numGroups, i;
    gid_t grouplist[NGROUPS_MAX];

    numGroups = getgroups(NGROUPS_MAX, &grouplist[0]);
    if (numGroups == -1) {
	perror("getgroups:");
	return -1;
    }
    for (i = 0; i < numGroups; i++) {
	fprintf(stderr, "grouplist[%d]=%d\n", i, grouplist[i]);
    }
    return 0;
}

/*
 * prints clear token fields, cell name and primary flag to stdout
 */

void
parseToken(char *buf)
{
    afs_int32 len = 0;
    char cellName[64];
    char *tp;

    struct ClearToken {
	afs_int32 AuthHandle;
	char HandShakeKey[8];
	afs_int32 ViceId;
	afs_int32 BeginTimestamp;
	afs_int32 EndTimestamp;
    } clearToken;

    assert(buf != NULL);

    tp = buf;
    memcpy(&len, tp, sizeof(afs_int32));	/* get size of secret token */
    tp += (sizeof(afs_int32) + len);	/* skip secret token */

    memcpy(&len, tp, sizeof(afs_int32));	/* get size of clear token */
    if (len != sizeof(struct ClearToken)) {
	fprintf(stderr,
		"weblog:parseToken: error getting length of ClearToken\n");
	return;
    }

    tp += sizeof(afs_int32);	/* skip length of cleartoken */
    memcpy(&clearToken, tp, sizeof(struct ClearToken));	/* copy cleartoken */

    tp += len;			/* skip clear token itself */

    memcpy(&len, tp, sizeof(afs_int32));	/* copy the primary flag */
    tp += sizeof(afs_int32);	/* skip primary flag */

    /* tp now points to the cell name */
    strcpy(cellName, tp);

    fprintf(stderr, "CellName:%s\n", cellName);
    fprintf(stderr, "Primary Flag (Hex):%x\n", len);
    fprintf(stderr,
	    "ClearToken Fields:-\nAuthHandle=%d\n"
	    "ViceId=%d\nBeginTimestamp=%d\nEndTimestamp=%d\n",
	    clearToken.AuthHandle, clearToken.ViceId,
	    clearToken.BeginTimestamp, clearToken.EndTimestamp);
}
