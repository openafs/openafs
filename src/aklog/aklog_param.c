/* 
 * $Id$
 * 
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#if !defined(lint) && !defined(SABER)
static char *rcsid = "$Id$";
#endif /* lint || SABER */

#include <afs/stds.h>
#include "aklog.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <kerberosIV/krb.h>
#include <krb5.h>


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef WINDOWS
#if !defined(HAVE_UNISTD_H) || (defined(__sun__) && ! defined(__svr4__))
extern int readlink ARGS((char *, char *, size_t));
#endif
/* extern int lstat ARGS((char *, struct stat *)); */
extern char *getwd ARGS((char *));
#endif /* WINDOWS */

static krb5_ccache  _krb425_ccache = 0;

#ifndef WINDOWS		/* Don't have lstat() */
#ifdef __STDC__
static int isdir(char *path, unsigned char *val)
#else
static int isdir(path, val)
  char *path;
  unsigned char *val;
#endif /* __STDC__ */
{
    struct stat statbuf;

    if (lstat(path, &statbuf) < 0)
	return (-1);
    else {
	if ((statbuf.st_mode & S_IFMT) == S_IFDIR) 
	    *val = TRUE;
	else
	    *val = FALSE;
	return (0);
    }  
}
#endif /* WINDOWS */

#ifdef __STDC__
static int get_cred(krb5_context context, 
			char *name, char *inst, char *realm, CREDENTIALS *c,
			krb5_creds **creds)
#else
static int get_cred(context, name, inst, realm, c, creds)
  krb5_context context;
  char *name;
  char *inst;
  char *realm;
  CREDENTIALS *c;
  krb5_creds **creds;
#endif /* __STDC__ */
{
    krb5_creds increds;
    krb5_error_code r;
    static krb5_principal client_principal = 0;

    memset((char *)&increds, 0, sizeof(increds));
/* ANL - instance may be ptr to a null string. Pass null then */
    if ((r = krb5_build_principal(context, &increds.server,
                     strlen(realm), realm,
                     name,
           (inst && strlen(inst)) ? inst : (void *) NULL,
                     (void *) NULL))) {
        return((int)r);
    }

    if (!_krb425_ccache)
        krb5_cc_default(context, &_krb425_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, _krb425_ccache, &client_principal);

    increds.client = client_principal;
    increds.times.endtime = 0;
	/* Ask for DES since that is what V4 understands */
    increds.keyblock.enctype = ENCTYPE_DES_CBC_CRC;

    r = krb5_get_credentials(context, 0, _krb425_ccache, &increds, creds);
    if (r)
        return((int)r);

/*       This requires krb524d to be running with the KDC */
    r = krb5_524_convert_creds(context, *creds, c);
    return((int)r);
}


#ifdef __STDC__
static int get_user_realm(krb5_context context,char *realm)
#else
static int get_user_realm(context, realm)
  krb5_context context;
  char *realm;

#endif /* __STDC__ */
{
    static krb5_principal client_principal = 0;
    int i;

    if (!_krb425_ccache)
        krb5_cc_default(context, &_krb425_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, _krb425_ccache, &client_principal);

    i = krb5_princ_realm(context, client_principal)->length;
    if (i > REALM_SZ-1) i = REALM_SZ-1;
    strncpy(realm,krb5_princ_realm(context, client_principal)->data,i);
    realm[i] = 0;
    return(KSUCCESS);
}


#ifndef WINDOWS

#ifdef __STDC__
static void pstderr(char *string)
#else
static void pstderr(string)
  char *string;
#endif /* __STDC__ */
{
    write(2, string, strlen(string));
}


#ifdef __STDC__
static void pstdout(char *string)
#else
static void pstdout(string)
  char *string;
#endif /* __STDC__ */
{
    write(1, string, strlen(string));
}

#else /* WINDOWS */

static void pstderr(char *string)
{
    if (_isatty(_fileno(stderr)))
	fprintf(stderr, "%s\r\n", string);
    else
	MessageBox(NULL, string, AKLOG_DIALOG_NAME, MB_OK | MB_ICONSTOP);
}

static void pstdout(char *string)
{
    if (_isatty(_fileno(stdout)))
	fprintf(stdout, "%s\r\n", string);
    else
	MessageBox(NULL, string, AKLOG_DIALOG_NAME, MB_OK);
}

#endif /* WINDOWS */

#ifdef __STDC__
static void exitprog(char status)
#else
static void exitprog(status)
  char status;
#endif /* __STDC__ */
{
    exit(status);
}


#ifdef __STDC__
void aklog_init_params(aklog_params *params)
#else
void aklog_init_params(params)
  aklog_params *params;
#endif /* __STDC__ */
{
#ifndef WINDOWS
    params->readlink = readlink;
    params->isdir = isdir;
    params->getwd = getwd;
#endif
    params->get_cred = get_cred;
    params->get_user_realm = get_user_realm;
    params->pstderr = pstderr;
    params->pstdout = pstdout;
    params->exitprog = exitprog;
}
