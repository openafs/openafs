/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 2.4  1990/10/01  10:01:37
 * Include <netinet/in.h> so ntohl is defined properly.
 *
 * Revision 2.3  90/09/28  16:18:23
 * Cleanups.
 * Move KeyIsZero here.
 * Add KeyCheckSum function.
 * 
 * Revision 2.2  90/09/19  08:31:04
 * Include afs/param.h.
 * 
 * Revision 2.1  90/08/29  08:37:41
 * Start with clean version to sync test and dev trees.
 * */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/file.h>
#endif
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <des.h>
#include "kauth.h"
#include "kautils.h"

#include "../permit_xprt.h"



/* This should match the behavior of ParseLoginName on input so that the output
 * and input are compatible.  In names "." should show as \056 and in names and
 * instances "@" should show as \100 */

void ka_PrintUserID (
  char *prefix,				/* part to be output before userID */
  char *name,				/* user name */
  char *instance,			/* instance, possible null or len=0 */
  char *postfix)			/* for output following userID */
{
    unsigned char *c;
    printf ("%s", prefix);
    for (c=(unsigned char *)name; *c; c++)
	if (isalnum(*c) || (ispunct(*c) && (*c != '.') && (*c != '@')))
	    printf ("%c", *c);
	else printf ("\\%0.3o", *c);
    if (instance && strlen (instance)) {
	printf (".");
	for (c=(unsigned char *)instance; *c; c++)
	    if (isalnum(*c) || (ispunct(*c) && (*c != '@'))) printf ("%c", *c);
	    else printf ("\\%0.3o", *c);
    }
    printf ("%s", postfix);
}

void ka_PrintBytes (
  char bs[],
  int  bl)
{
    int i = 0;

    for (i=0; i < bl; i++) {
	unsigned char c = bs[i];
	printf ("\\%0.3o", c);
    }
}

/* converts a byte string to ascii.  Return the number of unconverted bytes. */

int ka_ConvertBytes (
  char *ascii,				/* output buffer */
  int   alen,				/* buffer length */
  char  bs[],				/* byte string */
  int   bl)				/* number of bytes */
{
    int i;
    unsigned char c;

    alen--;				/* make room for termination */
    for (i=0; i < bl; i++) {
	c = bs[i];
	if (alen <= 0) return bl-i;
	if (isalnum(c) || ispunct(c)) (*ascii++ = c), alen--;
	else {
	    if (alen <= 3) return bl-i;
	    *ascii++ = '\\';
	    *ascii++ = (c>>6) + '0';
	    *ascii++ = (c>>3 & 7) + '0';
	    *ascii++ = (c&7) + '0';
	    alen -= 4;
	}
    }
    *ascii = 0;				/* terminate string */
    return 0;				/* all OK */
}

/* This is the inverse of the above function.  The return value is the number
   of bytes read.  The blen parameter gived the maximum size of the output
   buffer (binary). */

int ka_ReadBytes (
  char *ascii,
  char *binary,
  int   blen)
{
    char *cp = ascii;
    char  c;
    int   i = 0;
    while ((i < blen) && *cp) {		/* get byte till null or full */
	if (*cp == '\\') {		/* get byte in octal */
	    c = (*++cp) - '0';
	    c = (c<<3) + (*++cp) - '0';
	    c = (c<<3) + (*++cp) - '0';
	    cp++;
	} else c = *cp++;		/* get byte */
	binary[i++] = c;
    }
    return i;
}

int umin (
  afs_uint32 a,
  afs_uint32 b)
{
    if (a < b) return a;
    else return b;
}

/* ka_KeyCheckSum - returns a 32 bit cryptographic checksum of a DES encryption
 * key.  It encrypts a block of zeros and uses first 4 bytes as cksum. */

afs_int32 ka_KeyCheckSum (
  char *key,
  afs_uint32 *cksumP)
{
    des_key_schedule s;
    char block[8];
    afs_uint32 cksum;
    afs_int32 code;

    *cksumP = 0;
    bzero (block, 8);
    code = des_key_sched (key, s);
    if (code) return KABADKEY;
    des_ecb_encrypt (block, block, s, ENCRYPT);
    bcopy (block, &cksum, sizeof(afs_int32));
    *cksumP = ntohl(cksum);
    return 0;
}

/* is the key all zeros? */
int ka_KeyIsZero(
  register char *akey,
  register int alen)
{
    register int i;
    for(i=0;i<alen;i++) {
	if (*akey++ != 0) return 0;
    }
    return 1;
}

void ka_timestr (
  afs_int32 time,
  char *tstr,
  afs_int32 tlen)
{
    char tbuffer[32]; /* need at least 26 bytes */

    if (!time) strcpy (tstr, "no date");/* special case this */
    else if (time == NEVERDATE) strcpy(tstr, "never");
    else {
	strncpy(tstr,
		afs_ctime((time_t *)&time, tbuffer, sizeof(tbuffer)), tlen);
	tstr[strlen(tstr)-1] = '\0';	/* punt the newline character */
    }
}
