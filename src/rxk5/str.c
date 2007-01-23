/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/******************************************************************************

	Filename:	str.c
	Author:		Marcus Watts
	Date:		24 Aug 92
	
	Description:
	Some custom string handling routines.

 ******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ctype.h>
#include <string.h>
#ifndef index
#define index strchr
#define rindex strrchr
#endif
#include "str.h"
#ifdef ultrix
extern char *malloc();
#endif

char *getword(cp, word)
	register char *cp;
	char *word;
{
	char *wp;

	wp = word;
	while (*cp && isspace(*cp))
		++cp;
	while (*cp && !isspace(*cp))
		*wp++ = *cp++;
	*wp = 0;
	while (*cp && isspace(*cp))
		++cp;
	return cp;
}

int
kwscan(wp, wtbl)
	char *wp;
	char **wtbl;
{
	int i;
	register char *tp, *cp;
	int f;
	static int lctable[256];

	if (!lctable[1]) {
		for (i = 0; i < 256; ++i)
			if (isupper(i))
				lctable[i] = tolower(i);
			else
				lctable[i] = i;
	}
	i = 0;
	while (++i, tp = *wtbl++)
	{
		f = 0;
		cp = wp;
		for (;;)
		{
			if (*tp == '_')
			{
				++f;
				++tp;
				continue;
			}
			if (!*cp && (f || !*tp)) return i;
			if (lctable[(unsigned char)(*cp++)] != lctable[(unsigned char) (*tp++)])
				break;
		}
	}
	return 0;
}

int stripnl(cp)
	char *cp;
{
	if ((cp = index(cp, '\n')))
		*cp = 0;
	return 0;
}

char *skipspace(cp)
	char *cp;
{
	while (*cp && isspace(*cp)) ++cp;
	return cp;
}

int trimtrailing(cp)
	register char *cp;
{
	register int c;
	char *p;
	for (p = 0; (c = *cp); ++cp)
		if (!isspace(c))
			p = 0;
		else if (!p)
			p = cp;
	if (p) *p = 0;
	return 0;
}

int
alldigits(cp)
	char *cp;
{
	while (*cp)
		if (!isdigit(*cp))
			return 0;
		else ++cp;
	return 1;
}

#ifdef ultrix
char *strdup(s)
	char *s;
{
	char *r;
	if (r = malloc(strlen(s)+1))
		strcpy(r, s);
	return r;
}
#endif
