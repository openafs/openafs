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

#include "k5s_config.h"

#if defined(KERNEL)
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"    /*Standard vendor system headers */
#include "afsincludes.h"        /*AFS-based standard headers */
#include "afs_stats.h"
#else
#include <stdlib.h>
#include <string.h>
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strcasecmp(p,q)	strcasecmp(p,q)
#include <errno.h>
#endif
#include <sys/types.h>
#include "k5ssl.h"
#include "k5s_im.h"

#if defined(KERNEL)
/* Minimal implementations of kernel-mode variants */

struct _krb5_context {
    int initialized;
};

krb5_error_code
krb5_init_context(krb5_context *contextp)
{
    *contextp = afs_osi_Alloc(sizeof(struct _krb5_context));
    memset(*contextp, 0, sizeof(struct _krb5_context));
    (*contextp)->initialized = 1701;
    return 0;
}

#else
static const char def_config_files[] = "/etc/krb5.conf"
#if defined(__APPLE__) && (defined(DC_SUPPORT) || defined(AC_SUPPORT))
	":" "/Library/Preferences/edu.mit.Kerberos"
#endif
	":" "/home/k5/etc/krb5.conf";
	;

struct _krb5_context {
    krb5_profile profile;
};

int krb5i_issuid(void)
{
/* XXX need autoconfig logic for issetugid */
#if 0 && !defined(linux)
    return issetugid();
#else
    return (geteuid() != getuid());
#endif
}

krb5_error_code
krb5_init_context(krb5_context *contextp)
{
    int code;
    krb5_context ctx;
    const char *files;
    char *cp, *ep, *work;

    initialize_krb5_error_table();

    code = ENOMEM;
    ctx = malloc(sizeof *ctx);
    if (!ctx)
	goto Done;
    memset(ctx, 0, sizeof *ctx);
    files = 0;
    if (!krb5i_issuid())
	files = getenv("KRB5_CONFIG");
    if (!files)
	files = def_config_files;
    code = ENOMEM;
    work = strdup(files);
    if (!work) goto Done;
    code = KRB5_CONFIG_CANTOPEN;
    for (cp = work; cp; cp = ep) {
	ep = strchr(cp, ':');
	if (ep) *ep++ = 0;
	code = krb5i_parse_profile(cp, &ctx->profile);
	if (!code) break;
    }
    free(work);
    code = krb5_c_random_os_entropy(ctx, 0, NULL);
Done:
    if (code) {
	if (ctx) free(ctx);
	ctx = 0;
    }
    *contextp = ctx;
    return code;
}

void
krb5_free_context(krb5_context context)
{
    krb5i_free_profile(context->profile);
    free(context);
}

krb5_error_code
krb5i_config_get_strings(krb5_context context, const char * const *names, char ***out)
{
    *out = 0;
    if (!context) return KRB5_CONFIG_CANTOPEN;
    return krb5i_find_names(context->profile, names, out);
}

krb5_error_code
krb5_get_default_realm(krb5_context context, char ** out)
{
    static const char * const names[] = {"libdefaults", "default_realm", 0};
    char **strings;
    int i;
    int code;

    i = 0;
    code = krb5i_config_get_strings(context, names, &strings);
    if (code) goto Done;
    code = KRB5_CONFIG_NODEFREALM; if (!*strings) goto Done;
    *out = strdup(*strings);
    code = ENOMEM; if (!*out) goto Done;
    ++i;
    code = 0;
Done:
    if (strings) {
	for (; strings[i]; ++i)
	    free(strings[i]);
	free(strings);
    }
    return code;
}

static int
k5sdr(char *value, void *arg)
{
    return PROFILE_GO_AWAY;
}

krb5_error_code
krb5_set_default_realm (krb5_context context, const char * realm)
{
    static const char * const names[] = {"libdefaults", "default_realm", 0};
    int code;

    code = krb5i_config_lookup(&context->profile, names, k5sdr, 0);
    if (code) return code;
    code = krb5i_add_name_value(&context->profile, realm, names);
    return code;
}
#endif
