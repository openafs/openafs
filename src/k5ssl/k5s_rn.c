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
#include "afs/afsincludes.h"        /*AFS-based standard headers */
#include "afs_stats.h"
#else
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strcasecmp(p,q)	strcasecmp(p,q)
#endif

#include <sys/types.h>
#ifdef USING_SSL
#include <openssl/evp.h>
#else
#include "k5s_evp.h"
#endif
#include "k5ssl.h"

krb5_error_code
krb5_c_random_add_entropy(krb5_context ctx,
    unsigned int source,
    const krb5_data *data)
{
    RAND_seed(data->data, data->length);
    return 0;
}

static krb5_error_code
stir_random_state(krb5_context ctx)
{
    struct {
	int pid;
#if !defined(KERNEL)
	struct timeval tv[1];
	int ls;
#else
	struct { int tv_sec, tv_usec; } tv[1];
#endif
    } junk[1];
    krb5_data seed[1];

    junk->pid = getpid();
#if !defined(KERNEL)
    gettimeofday(junk->tv, 0);
#else
#ifdef __GNUC__
    #warning Review entropy issues
#endif
    junk->tv->tv_sec = osi_Time();
#endif
#if !defined(KERNEL)
    junk->ls = lseek(0, 0, 1);
#endif
    seed->data = (void *) junk;
    seed->length = sizeof *junk;
    return krb5_c_random_add_entropy(ctx,
	KRB5_C_RANDSOURCE_OSRAND,
	seed);
}

krb5_error_code
krb5_c_random_os_entropy(krb5_context ctx,
    int strong,
    int *success)
{
    int junk;
    if (!success) success=&junk;
    while (RAND_status() != 1)
	stir_random_state(ctx);
    *success = 1;
    return 0;
}

krb5_error_code
krb5_c_random_make_octets(krb5_context ctx,
    krb5_data *data)
{
    int code;

    code = 0;
    if (RAND_status() != 1)
	return KRB5_CRYPTO_INTERNAL;
    memset(data->data, 0, data->length);
    if (!RAND_bytes(data->data, data->length))
	code = KRB5_CRYPTO_INTERNAL;
    return code;
}
