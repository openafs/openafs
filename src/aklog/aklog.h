/*
 * $Id$
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#ifndef __AKLOG_H__
#define __AKLOG_H__

#include <afsconfig.h>

#include <krb5.h>
#include "linked_list.h"

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

#ifndef u_int32_t
#define u_int32_t uint32_t
#endif

struct ktext {
    unsigned int length;
    unsigned char dat[MAX_KTXT_LEN];
    afs_uint32 mbz;
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

#endif /* __AKLOG_H__ */
