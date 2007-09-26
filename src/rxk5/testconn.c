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

/*
 * test application connect logic.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <afs/param.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <lock.h>
#ifdef HAVE_KERBEROSIV_DES_H
#include <kerberosIV/des.h>
#endif
#ifdef HAVE_DES_H
#include <des.h>
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <ctype.h>
#include <string.h>
#include "rxk5errors.h"

#include "str.h"
#include "test.h"
#include "servconn.h"
#include "rxk5.h"

char def_krbname[] = "rxk5/test";

struct servparm test_parm[1] = {{
    "localhost",
    def_krbname,
    TESTSERVICEID,
    TESTPORT,
    5,
    rxk5_crypt,
}};
struct servstash teststash[1];

int
settesthost(char *cp, char *word)
{
    return setservhost(test_parm, cp, word);
}

int
settestport(char *cp, char *word)
{
    while (*cp && isspace(*cp))
	++cp;
    test_parm->port = atoi(cp);
    ++test_parm->changed;
    return 0;
}

int
settestlevel(char *cp, char *word)
{
    while (*cp && isspace(*cp))
	++cp;
    test_parm->level = atoi(cp);
    ++test_parm->changed;
    return 0;
}

int
settestsecurityindex(char *cp, char *word)
{
    while (*cp && isspace(*cp))
	++cp;
    test_parm->securityindex = atoi(cp);
    ++test_parm->changed;
    return 0;
}

int
settestkrbname(char *cp, char *word)
{
    return setservkrbname(test_parm, cp, word, def_krbname);
}

int
gettestconn(struct rx_connection **connp)
{
    return getservconn(test_parm, teststash, connp);
}
