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
 * Walrus wrote this code.  All I did was put this comment in.  -mdw Mar1994
 *
 * This code is the glue between our traditional rx based server,
 *  and an MIT style srvtab which contains the key used by our
 *  server.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>
#include <afs/param.h>
#include <afs/kautils.h>
/* #include <krb.h> */
extern char *afs_error_message();

/*
 * The problem is that rxkad wants to call a function
 * as (IN rock, IN kvno, OUT key) but read_service_key
 * uses (IN service, IN instance, IN realm, IN kvno,
 * IN srvtabfile, OUT key). This function, rsk_wrap, and
 * its associated static globals are intended to provide the
 * interface needed by rxkad.
 *
 * Some other solutions might be:
 * - keep the principal & srvtabfile in a structure which gets
 *   passed in
 * - store all the strings in a single char buffer (could cause
 *   problems for anything which tried to determine the size
 *   of the buffer unless the strings were not null-separated.
 */

/*
 * Globals for this file
 */
static char rsk_name[MAXKTCNAMELEN];
static char rsk_instance[MAXKTCNAMELEN];
static char rsk_realm[MAXKTCNAMELEN];
static char rsk_srvtab[MAXKTCNAMELEN];

int rsk_debug;

/*
 * rsk_ParseLoginName--a wrapper to ka_ParseLoginName which
 *                     sets the relevant static variables
 */
#ifdef __STDC__
#if 0
extern long	ka_ParseLoginName(char *, char [], char [], char []);
#endif
extern int	ka_CellToRealm(char *, char *, int *);
#else
extern long	ka_ParseLoginName();
extern int	ka_CellToRealm();
#endif

static long
rsk_ParseLoginName(char *aPrincipal)
{
    int		code;
    char	cell[MAXKTCNAMELEN];

    code = ka_ParseLoginName((char *) aPrincipal, rsk_name, rsk_instance, cell);
    if (code) {
fprintf(stderr,"rsk_ParseLoginName: ka_ParseLoginName failed on %s - %d %s\n",
aPrincipal, code, afs_error_message(code));
	return code;
    }

    /*
     * read_service_key uses * to match any instance
     */
    if (*rsk_instance == '\0')
    {
	rsk_instance[0] = '*';
	rsk_instance[1] = '\0';
    }
    
    code = ka_CellToRealm(cell, rsk_realm, 0);
if (code)
fprintf(stderr,"rsk_ParseLoginName: ka_CellToRealm failed on %s - %d %s\n",
cell, code, afs_error_message(code));
    return code;
}


/*
 * rsk_SetSrvtab--set the static variabl rsk_srvtab
 */
static void
rsk_SetSrvtab(char *aSrvtab)
{
    (void) strncpy(rsk_srvtab, aSrvtab, MAXKTCNAMELEN);
    return;
}


/*
 *  rsk_init--fill in principal variables and srvtab file name
 */
long
rsk_Init(char *aPrincipal,
    char *aSrvtab)
{
    long	code;

    code = rsk_ParseLoginName(aPrincipal);

    if (code)
	return code;

    rsk_SetSrvtab(aSrvtab);
    return 0;
}


/*
 * rsk_wrap--a wrapper function for read_service_key
 */
#ifdef __STDC__
extern int	read_service_key(char *, char *, char *,
				 int, char *, char *);
#endif

int
rsk_Wrap(int *dummy, int aKvno, char *aKey)
{
    long	code;
#if 0
    static char	rn[] = "rsk_Wrap";

printf ("%s called\n", rn);
#endif
    code = read_service_key(rsk_name, rsk_instance, rsk_realm,
			    aKvno, rsk_srvtab, aKey);
#if 0
printf ("%s returns %08lx %08lx\n", rn, ((long*)aKey)[0], ((long*)aKey)[1]);
#endif

    return code;
}
