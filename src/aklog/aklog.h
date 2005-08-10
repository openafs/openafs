/* 
 * $Id: aklog.h,v 1.1.2.4 2005/07/11 19:07:00 shadow Exp $
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#ifndef __AKLOG_H__
#define __AKLOG_H__

#if !defined(lint) && !defined(SABER)
static char *rcsid_aklog_h = "$Id: aklog.h,v 1.1.2.4 2005/07/11 19:07:00 shadow Exp $";
#endif /* lint || SABER */

#include <krb5.h>
#include "linked_list.h"
#include <afsconfig.h>

#ifdef __STDC__
#define ARGS(x) x
#else
#define ARGS(x) ()
#endif /* __STDC__ */

void aklog ARGS((int, char *[]));

/*
 * If we have krb.h, use the definition of CREDENTIAL from there.  Otherwise,
 * inline it.  When we inline it we're using the inline definition from the
 * Heimdal sources (since Heimdal doesn't include a definition of struct
 * credentials with the sources
 */

#ifdef HAVE_KERBEROSIV_KRB_H
#include <kerberosIV/krb.h>
#else /* HAVE_KERBEROSIV_KRB_H */

#ifndef MAX_KTXT_LEN
#define MAX_KTXT_LEN 1250
#endif /* MAX_KTXT_LEN */
#ifndef ANAME_SZ
#define ANAME_SZ 40
#endif /* ANAME_SZ */
#ifndef REALM_SZ
#define REALM_SZ 40
#endif /* REALM_SZ */
#ifndef SNAME_SZ
#define SNAME_SZ 40
#endif /* SNAME_SZ */
#ifndef INST_SZ
#define INST_SZ 40
#endif /* INST_SZ */

struct ktext {
    unsigned int length;
    unsigned char dat[MAX_KTXT_LEN];
    u_int32_t mbz;
};

struct credentials {
    char    service[ANAME_SZ];
    char    instance[INST_SZ];
    char    realm[REALM_SZ];
    char    session[8];
    int     lifetime;
    int     kvno;
    struct ktext ticket_st;
    int32_t    issue_date;
    char    pname[ANAME_SZ];
    char    pinst[INST_SZ];
};

typedef struct credentials CREDENTIALS;
#endif /* ! HAVE_KERBEROSIV_KRB_H */

#ifdef WINDOWS
/*
 * Complete server info for one cell.
 *
 * Normally this is defined in afs/cellconfig.h, but the Windows header
 * files and API don't use this structure. So, I'll include it here so
 * I don't have to rewrite large chunks of code.
 */
#define MAXCELLCHARS    64
#define MAXHOSTCHARS    64
#define MAXHOSTSPERCELL  8

struct afsconf_cell {
    char name[MAXCELLCHARS];        /* Cell name */
    short numServers;               /* Num active servers for the cell*/
    short flags;                    /* useful flags */
    struct sockaddr_in hostAddr[MAXHOSTSPERCELL];
	                            /* IP addresses for cell's servers*/
    char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];       
                                    /* Names for cell's servers */
    char *linkedCell;               /* Linked cell name, if any */
};

/* Windows krb5 libraries don't seem to have this call */
#define krb5_xfree(p)	free(p)

/* Title for dialog boxes */
#define AKLOG_DIALOG_NAME		"aklog"

#endif /* WINDOWS */

#endif /* __AKLOG_H__ */
