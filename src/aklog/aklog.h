/* 
 * $Id$
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#ifndef __AKLOG_H__
#define __AKLOG_H__

#if !defined(lint) && !defined(SABER)
static char *rcsid_aklog_h = "$Id$";
#endif /* lint || SABER */

#include <krb5.h>
#include <kerberosIV/krb.h>
#include "linked_list.h"
#include <afsconfig.h>

#ifdef __STDC__
#define ARGS(x) x
#else
#define ARGS(x) ()
#endif /* __STDC__ */

typedef struct {
    int (*readlink)ARGS((char *, char *, size_t));
    int (*isdir)ARGS((char *, unsigned char *));
    char *(*getwd)ARGS((char *));
    int (*get_cred)ARGS((krb5_context, char *, char *, char *, CREDENTIALS *,
		krb5_creds **));
    int (*get_user_realm)ARGS((krb5_context, char *));
    void (*pstderr)ARGS((char *));
    void (*pstdout)ARGS((char *));
    void (*exitprog)ARGS((char));
} aklog_params;

void aklog ARGS((int, char *[], aklog_params *));
void aklog_init_params ARGS((aklog_params *));

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
