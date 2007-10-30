/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdarg.h>

/* Just like strncpy but shift-case in transit and forces null termination */
char *
lcstring(char *d, char *s, int n)
{
    char *original_d = d;
    char c;

    if ((s == 0) || (d == 0))
	return 0;		/* just to be safe */
    while (n) {
	c = *s++;
	if (isupper(c))
	    c = tolower(c);
	*d++ = c;
	if (c == 0)
	    break;		/* quit after transferring null */
	if (--n == 0)
	    *(d - 1) = 0;	/* make sure null terminated */
    }
    return original_d;
}

char *
ucstring(char *d, char *s, int n)
{
    char *original_d = d;
    char c;

    if ((s == 0) || (d == 0))
	return 0;
    while (n) {
	c = *s++;
	if (islower(c))
	    c = toupper(c);
	*d++ = c;
	if (c == 0)
	    break;
	if (--n == 0)
	    *(d - 1) = 0;	/* make sure null terminated */
    }
    return original_d;
}

int
stolower(char *s)
{
  while (*s) {
        if (isupper(*s))
            *s = tolower(*s);
        s++;
    }
    return 0;
}

int
stoupper(char *s)
{
  while (*s) {
        if (islower(*s))
            *s = toupper(*s);
        s++;
    }
    return 0;
}

/* strcompose - concatenate strings passed to it.
 * Input: 
 *   buf: storage for the composed string. Any data in it will be lost.
 *   len: length of the buffer.
 *   ...: variable number of string arguments. The last argument must be
 *        NULL. 
 * Returns buf or NULL if the buffer was not sufficiently large.
 */
char *
strcompose(char *buf, size_t len, ...)
{
    va_list ap;
    size_t spaceleft = len - 1;
    char *str;
    size_t slen;

    if (buf == NULL || len <= 0)
	return NULL;

    *buf = '\0';
    va_start(ap, len);
    str = va_arg(ap, char *);
    while (str) {
	slen = strlen(str);
	if (spaceleft < slen)	/* not enough space left */
	    return NULL;

	strcat(buf, str);
	spaceleft -= slen;

	str = va_arg(ap, char *);
    }
    va_end(ap);

    return buf;
}
