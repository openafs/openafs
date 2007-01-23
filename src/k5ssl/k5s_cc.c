/*
 * Copyright (c) 2006
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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include "k5ssl.h"
#include "k5s_im.h"

#define KRB5_FCC_FVNO_1 0x0501
#define KRB5_FCC_FVNO_2 0x0502
#define KRB5_FCC_FVNO_3 0x0503
#define KRB5_FCC_FVNO_4 0x0504

static const char krb5i_fcc_prefix[] = "FILE";

struct krb5_fcc_data {
    char name[sizeof krb5i_fcc_prefix + 1];
};
struct krb5_fcc_cursor {
    int fd;
    int verno;
    int pos;
};

krb5_error_code
krb5i_fcc_resolve(krb5_context context, krb5_ccache cache, const char *name)
{
    struct krb5_fcc_data *data;
    int l;
    char temp[80];
    if (!name || !*name) {
	snprintf (temp, sizeof temp, "/tmp/krb5cc_%d", (int)getuid());
	name = temp;
    }
    data = malloc(sizeof *data + (l = strlen(name)));
    if (!data) return ENOMEM;
    memset(data, 0, sizeof *data);
    memcpy(data->name, krb5i_fcc_prefix, sizeof krb5i_fcc_prefix - 1);
    data->name[sizeof krb5i_fcc_prefix - 1] = ':';
    memcpy(data->name + sizeof krb5i_fcc_prefix, name, l+1);
    cache->data = data;
    return 0;
}

krb5_error_code
krb5i_fcc_open(krb5_context context, struct krb5_fcc_data *fdata,
	struct krb5_fcc_cursor *cursor, int mode)
{
    int fd;
    struct flock lockarg[1];
    short stemp;
    int temp;
    int code;
    int flen, tag, taglen;

    memset(cursor, 0, sizeof *cursor);
    cursor->fd = -1;
    fd = open(fdata->name + sizeof krb5i_fcc_prefix, mode);
    if (fd == -1) {
	return errno;
    }
    memset(lockarg, 0, sizeof *lockarg);
    switch(mode) {
    case 0:
	lockarg->l_type = F_RDLCK;
	break;
    default:
	lockarg->l_type = F_WRLCK;
    }
    if (fcntl(fd, F_SETLKW, lockarg) < 0) {
	code = errno;
	goto Done;
    }
    if (fcntl(fd, F_SETFD, 1) < 0) {
	code = errno;
	goto Done;
    }
    code = KRB5_CC_FORMAT;
    if (read(fd, (char*)&stemp, sizeof stemp) != sizeof stemp)
	goto Done;
    switch(cursor->verno = ntohs(stemp)) {
    default:
	code = KRB5_CCACHE_BADVNO;
	goto Done;
    case KRB5_FCC_FVNO_3:
	break;
    case KRB5_FCC_FVNO_4:
	if (read(fd, (char*)&stemp, sizeof stemp) != sizeof stemp)
	    goto Done;
	flen = (unsigned short) ntohs(stemp);
	while ((flen -= 4) >= 0) {
	    if (read(fd, (char*)&temp, sizeof temp) != sizeof temp)
		goto Done;
	    temp = ntohl(temp);
	    tag = (unsigned short) (temp >> 16);
	    taglen = (unsigned short) temp;
	    switch(tag) {
	    default:
		if (lseek(fd, (off_t) taglen, 1) == (off_t) -1)
		    goto Done;
	    }
	    flen -= taglen;
	}
    }
    code = 0;
Done:
    if (code)
	close(fd);
    else
	cursor->fd = fd;
    return code;
}

krb5_error_code
krb5i_fcc_store_data(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_data *data)
{
    int temp, code;

    code = KRB5_CC_FORMAT;
    temp = htonl(data->length);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    if (write(cursor->fd, data->data, data->length) != data->length)
	return code;
    return 0;
}

krb5_error_code
krb5i_fcc_store_principal(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_principal principal)
{
    int temp, code, i;

    code = KRB5_CC_FORMAT;
    switch(cursor->verno) {
    default:
	temp = htonl(principal->type);
	if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	    return code;
	temp = htonl(principal->length);
    }
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    if ((code = krb5i_fcc_store_data(context, cursor, &principal->realm)))
	return code;
    for (i = 0; i < principal->length; ++i)
	if ((code = krb5i_fcc_store_data(context, cursor, principal->data+i)))
	    return code;
    return 0;
}

krb5_error_code
krb5i_fcc_store_keyblock(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_keyblock *keyblock)
{
    int temp, code;
    short stemp;

    code = KRB5_CC_FORMAT;
    stemp = htons(keyblock->enctype);
    if (write(cursor->fd, &stemp, sizeof stemp) != sizeof stemp)
	return code;
    if (cursor->verno == KRB5_FCC_FVNO_3 &&
	    write(cursor->fd, &stemp, sizeof stemp) != sizeof stemp) {
	return code;
    }
    temp = htonl(keyblock->length);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    if (write(cursor->fd, keyblock->contents, keyblock->length) != keyblock->length)
	return code;
    return 0;
}

krb5_error_code
krb5i_fcc_store_times(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_ticket_times *times)
{
    int temp, code;

    code = KRB5_CC_FORMAT;
    temp = htonl(times->authtime);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    temp = htonl(times->starttime);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    temp = htonl(times->endtime);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    temp = htonl(times->renew_till);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    return 0;
}

krb5_error_code
krb5i_fcc_store_addrs(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_address **addresses)
{
    int temp, code;
    short stemp;
    int i, count;

    code = KRB5_CC_FORMAT;
    count = 0;
    if (addresses)
	while (addresses[count])
	    ++count;
    temp = htonl(count);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    for (i = 0; i < count; ++i) {
	stemp = htons(addresses[i]->addrtype);
	if (write(cursor->fd, &stemp, sizeof stemp) != sizeof stemp)
	    return code;
	temp = htonl(addresses[i]->length);
	if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	    return code;
	if (write(cursor->fd, addresses[i]->contents,
		addresses[i]->length) != addresses[i]->length)
	    return code;
    }
    return 0;
}

int
krb5i_fcc_store_authdata(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_authdata **authdata)
{
    int temp, code;
    short stemp;
    int i, count;

    code = KRB5_CC_FORMAT;
    count = 0;
    if (authdata)
	while (authdata[count])
	    ++count;
    temp = htonl(count);
    if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    for (i = 0; i < count; ++i) {
	stemp = htons(authdata[i]->ad_type);
	if (write(cursor->fd, &stemp, sizeof stemp) != sizeof stemp)
	    return code;
	temp = htonl(authdata[i]->length);
	if (write(cursor->fd, &temp, sizeof temp) != sizeof temp)
	    return code;
	if (write(cursor->fd, authdata[i]->contents,
		authdata[i]->length) != authdata[i]->length)
	    return code;
    }
    return 0;
}

krb5_error_code
krb5i_fcc_store_cred(krb5_context context, krb5_ccache cache,
    krb5_creds *creds)
{
    struct krb5_fcc_cursor fcursor[1];
    int code;
    char ctemp;
    int temp;

    if ((code = krb5i_fcc_open(context, (struct krb5_fcc_data *) cache->data,
	    fcursor, 2))) {
	return code;
    }
    code = KRB5_CC_FORMAT;
    if (lseek(fcursor->fd, (off_t) 0L, 2) == (off_t) -1)
	goto Done;
    if ((code = krb5i_fcc_store_principal(context, fcursor, creds->client)))
	goto Done;
    if ((code = krb5i_fcc_store_principal(context, fcursor, creds->server)))
	goto Done;
    if ((code = krb5i_fcc_store_keyblock(context, fcursor, &creds->keyblock)))
	goto Done;
    if ((code = krb5i_fcc_store_times(context, fcursor, &creds->times)))
	goto Done;
    code = KRB5_CC_FORMAT;
    ctemp = creds->is_skey;
    if (write(fcursor->fd, &ctemp, 1) != 1)
	goto Done;
    temp = htonl(creds->ticket_flags);
    if (write(fcursor->fd, &temp, sizeof temp) != sizeof temp)
	goto Done;
    if ((code = krb5i_fcc_store_addrs(context, fcursor, creds->addresses)))
	goto Done;
    if ((code = krb5i_fcc_store_authdata(context, fcursor, creds->authdata)))
	goto Done;
    if ((code = krb5i_fcc_store_data(context, fcursor, &creds->ticket)))
	goto Done;
    if ((code = krb5i_fcc_store_data(context, fcursor, &creds->second_ticket)))
	goto Done;
Done:
    close(fcursor->fd);
    return code;
}

int
krb5i_fcc_fetch_data(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_data *data)
{
    int temp, code;

    code = KRB5_CC_FORMAT;
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    data->length = ntohl(temp);
    if (data->length >= 1UL<<20) {
	return ENOMEM;
    }
    if (!(data->data = malloc(1 + data->length))) {
	return ENOMEM;
    }
    data->data[data->length] = 0;
    if (read(cursor->fd, data->data, data->length) != data->length) {
	free(data->data);
	data->data = 0;
	return code;
    }
    return 0;
}

int
krb5i_fcc_fetch_principal(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_principal *principal)
{
    int temp, code, i;
    krb5_principal princ;

    code = KRB5_CC_FORMAT;
    princ = (krb5_principal) malloc(sizeof *princ);
    memset(princ, 0, sizeof *princ);
    switch(cursor->verno) {
    default:
	if ((i = read(cursor->fd, &temp, sizeof temp)) != sizeof temp) {
	    if (!i) code = KRB5_CC_END;
	    goto Done;
	}
	princ->type = ntohl(temp);
    }
    if ((i = read(cursor->fd, &temp, sizeof temp)) != sizeof temp) {
	if (!i) code = KRB5_CC_END;
	goto Done;
    }
    code = KRB5_CC_FORMAT;
    princ->length = ntohl(temp);
    princ->data = (krb5_data *) malloc(princ->length * sizeof *princ->data);
    if (!princ->data) {
	code = ENOMEM;
	goto Done;
    }
    memset(princ->data, 0, princ->length * sizeof *princ->data);
    if ((code = krb5i_fcc_fetch_data(context, cursor, &princ->realm)))
	goto Done;
    for (i = 0; i < princ->length; ++i)
	if ((code = krb5i_fcc_fetch_data(context, cursor, princ->data+i)))
	    goto Done;
    code = 0;
Done:
    if (code)
	krb5_free_principal(context, princ);
    else *principal = princ;
    return code;
}

int
krb5i_fcc_fetch_keyblock(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_keyblock *keyblock)
{
    int temp, code;
    short stemp;

    code = KRB5_CC_FORMAT;
    keyblock->contents = 0;
    if (read(cursor->fd, &stemp, sizeof stemp) != sizeof stemp)
	return code;
    keyblock->enctype = (short) ntohs(stemp);	/* Sign-extend !!! */
    if (cursor->verno == KRB5_FCC_FVNO_3 &&
	    read(cursor->fd, &stemp, sizeof stemp) != sizeof stemp) {
	return code;
    }
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    keyblock->length = ntohl(temp);
    if (keyblock->length >= 1UL<<20) {
	return ENOMEM;
    }
    if (!keyblock->length) return 0;
    keyblock->contents = malloc(keyblock->length);
    if (read(cursor->fd, keyblock->contents, keyblock->length) != keyblock->length) {
	free(keyblock->contents);
	return code;
    }
    return 0;
}

int
krb5i_fcc_fetch_times(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_ticket_times *times)
{
    int temp, code;

    code = KRB5_CC_FORMAT;
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    times->authtime = ntohl(temp);
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    times->starttime = ntohl(temp);
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    times->endtime = ntohl(temp);
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    times->renew_till = ntohl(temp);
    return 0;
}

int
krb5i_fcc_fetch_addrs(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_address ***addresses)
{
    int temp, code;
    short stemp;
    int i, count;
    krb5_address **ap;

    code = KRB5_CC_FORMAT;
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    count = ntohl(temp);
    if (count >= 1UL<<18) {
	return ENOMEM;
    }
    ap = malloc((count+1) * sizeof *ap);
    memset(ap, 0, (count+1) * sizeof *ap);
    for (i = 0; i < count; ++i) {
	ap[i] = malloc(sizeof **ap);
	if (!ap[i]) {code = ENOMEM; goto Done; }
	memset(ap[i], 0, sizeof **ap);
	if (read(cursor->fd, &stemp, sizeof stemp) != sizeof stemp)
	    return code;
	ap[i]->addrtype = ntohs(stemp);
	if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	    return code;
	ap[i]->length = ntohl(temp);
	if (ap[i]->length >= 1UL<<20) {code = ENOMEM; goto Done; }
	if (!ap[i]->length)
	    continue;
	ap[i]->contents = malloc(ap[i]->length);
	if (!ap[i]->contents) {code = ENOMEM; goto Done; }
	if (read(cursor->fd, ap[i]->contents,
		ap[i]->length) != ap[i]->length)
	    return code;
    }
    code = 0;
Done:
    if (code)
	krb5_free_addresses(context, ap);
    else
	*addresses = ap;
    return 0;
}

int
krb5i_fcc_fetch_authdata(krb5_context context, struct krb5_fcc_cursor *cursor,
    krb5_authdata ***authdata)
{
    int temp, code;
    short stemp;
    int i, count;
    krb5_authdata **ap;

    code = KRB5_CC_FORMAT;
    if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	return code;
    count = ntohl(temp);
    if (count >= 1UL<<18) {
	return ENOMEM;
    }
    ap = malloc((count+1) * sizeof *ap);
    memset(ap, 0, (count+1) * sizeof *ap);
    for (i = 0; i < count; ++i) {
	ap[i] = malloc(sizeof **ap);
	if (!ap[i]) {code = ENOMEM; goto Done; }
	memset(ap[i], 0, sizeof **ap);
	if (read(cursor->fd, &stemp, sizeof stemp) != sizeof stemp)
	    return code;
	ap[i]->ad_type = ntohs(stemp);
	if (read(cursor->fd, &temp, sizeof temp) != sizeof temp)
	    return code;
	ap[i]->length = ntohl(temp);
	if (ap[i]->length >= 1UL<<20) {code = ENOMEM; goto Done; }
	if (!ap[i]->length)
	    continue;
	ap[i]->contents = malloc(ap[i]->length);
	if (!ap[i]->contents) {code = ENOMEM; goto Done; }
	if (read(cursor->fd, ap[i]->contents,
		ap[i]->length) != ap[i]->length)
	    return code;
    }
    code = 0;
Done:
    if (code)
	krb5_free_authdata(context, ap);
    else
	*authdata = ap;
    return 0;
}

krb5_error_code
krb5i_fcc_start_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_fcc_cursor *fcursor;
    int code;
    krb5_principal princ;

    *cursor = 0;
    princ = 0;
    fcursor = (struct krb5_fcc_cursor *)malloc(sizeof *fcursor);
    if (!fcursor)
	return ENOMEM;
    if ((code = krb5i_fcc_open(context, (struct krb5_fcc_data *) cache->data,
	    fcursor, 0))) {
	goto Done;
    }
    code = krb5i_fcc_fetch_principal(context, fcursor, &princ);
    if (code) goto Done;
    fcursor->pos = lseek(fcursor->fd, (off_t) 0L, 1);
Done:
    if (princ) krb5_free_principal(context, princ);
    if (code) {
	close(fcursor->fd);
	free(fcursor);
    } else
	*cursor = (krb5_cc_cursor) fcursor;
    return code;
}

krb5_error_code
krb5i_fcc_get_next(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor, krb5_creds *creds)
{
    struct krb5_fcc_cursor *fcursor = (struct krb5_fcc_cursor *) *cursor;
    int code;
    char ctemp;
    int temp;

    memset(creds, 0, sizeof *creds);
    code = KRB5_CC_FORMAT;
    if (lseek(fcursor->fd, fcursor->pos, 0) == (off_t) -1)
	goto Done;
    if ((code = krb5i_fcc_fetch_principal(context, fcursor, &creds->client)))
	goto Done;
    if ((code = krb5i_fcc_fetch_principal(context, fcursor, &creds->server)))
	goto Done;
    if ((code = krb5i_fcc_fetch_keyblock(context, fcursor, &creds->keyblock)))
	goto Done;
    if ((code = krb5i_fcc_fetch_times(context, fcursor, &creds->times)))
	goto Done;
    code = KRB5_CC_FORMAT;
    if (read(fcursor->fd, &ctemp, 1) != 1)
	goto Done;
    creds->is_skey = ctemp;
    if (read(fcursor->fd, &temp, sizeof temp) != sizeof temp)
	goto Done;
    creds->ticket_flags = ntohl(temp);
    if ((code = krb5i_fcc_fetch_addrs(context, fcursor, &creds->addresses)))
	goto Done;
    if ((code = krb5i_fcc_fetch_authdata(context, fcursor, &creds->authdata)))
	goto Done;
    if ((code = krb5i_fcc_fetch_data(context, fcursor, &creds->ticket)))
	goto Done;
    if ((code = krb5i_fcc_fetch_data(context, fcursor, &creds->second_ticket)))
	goto Done;
Done:
    if (!code)
	fcursor->pos = lseek(fcursor->fd, (off_t) 0L, 1);
    return code;
}

krb5_error_code
krb5i_fcc_end_get(krb5_context context,
    krb5_ccache cache, krb5_cc_cursor *cursor)
{
    struct krb5_fcc_cursor *fcursor = (struct krb5_fcc_cursor *) *cursor;

    close(fcursor->fd);
    free(fcursor);
    return 0;
}

krb5_error_code
krb5i_fcc_get_principal(krb5_context context,
    krb5_ccache cache, krb5_principal *princ)
{
    struct krb5_fcc_cursor fcursor[1];
    int code;

    *princ = 0;
    if ((code = krb5i_fcc_open(context, (struct krb5_fcc_data *) cache->data,
	    fcursor, 0))) {
	return code;
    }
    code = krb5i_fcc_fetch_principal(context, fcursor, princ);
    close(fcursor->fd);
    return code;
}

krb5_error_code
krb5i_fcc_close(krb5_context context, krb5_ccache cache)
{
    free((struct krb5_fcc_data *) cache->data);
    return 0;
}

const char *
krb5i_fcc_get_name(krb5_context context, krb5_ccache cache)
{
    return ((struct krb5_fcc_data *) cache->data)->name;
}

krb5_error_code
krb5i_fcc_initialize(krb5_context context, 
    krb5_ccache cache,
    krb5_principal principal)
{
    struct krb5_fcc_data *fdata = (struct krb5_fcc_data *) cache->data;
    struct krb5_fcc_cursor fcursor[1];
    short stemp;
    int code = 0;

    /* locking? */
    fcursor->fd = creat(fdata->name + sizeof krb5i_fcc_prefix, 0666);
    if (fcursor->fd < 0) {
	return errno;
    }
    stemp = ntohs(KRB5_FCC_FVNO_3);
    if (write(fcursor->fd, &stemp, sizeof stemp) == sizeof stemp)
	;
    else if ((code = errno))
	;
    else code = KRB5_CC_FORMAT;
    if (!code)
	code = krb5i_fcc_store_principal(context, fcursor, principal);
    close(fcursor->fd);
    return code;
}

const krb5_cc_ops krb5_cc_file_ops = {
    krb5i_fcc_prefix,
    krb5i_fcc_resolve,
    krb5i_fcc_store_cred,
    krb5i_fcc_start_get,
    krb5i_fcc_get_next,
    krb5i_fcc_end_get,
    krb5i_fcc_get_principal,
    krb5i_fcc_close,
    krb5i_fcc_get_name,
    krb5i_fcc_initialize,
};

struct krb5_cc_type {
    struct krb5_cc_type *next;
    const krb5_cc_ops *ops;
};

#define DEFAULT_CC	krb5_cc_file_ops

#ifdef DC_SUPPORT
extern krb5_cc_ops krb5_dcc_ops;
#undef DEFAULT_CC
#define DEFAULT_CC krb5_dcc_ops
#endif

struct krb5_cc_type const krb5i_cc_element1[] = {
#ifdef DC_SUPPORT
{krb5i_cc_element1+1, &krb5_dcc_ops},
#endif
{0, &krb5_cc_file_ops}
};
struct krb5_cc_type *krb5i_cc_typelist
	= (struct krb5_cc_type *) krb5i_cc_element1;
krb5_cc_ops const *krb5_cc_dfl_ops = &DEFAULT_CC;

krb5_boolean
krb5i_compare_creds(krb5_context context, krb5_flags flags,
    const krb5_creds *match, const krb5_creds *creds)
{
    if (!(flags & KRB5_TC_MATCH_SRV_NAMEONLY)
	&& match->client && !krb5_principal_compare(context,
	    match->client, creds->client))
	return 0;
    if (match->server && !krb5_principal_compare(context,
	    match->server, creds->server))
	return 0;
    return 1;
}

krb5_error_code
krb5_cc_get_principal(krb5_context context, krb5_ccache cache,
    krb5_principal *princ)
{
    return cache->ops->get_principal(context, cache, princ);
}

krb5_error_code
krb5_cc_resolve(krb5_context context, const char *name, krb5_ccache *cache)
{
    const char *cp;
    const krb5_cc_ops *ops;
    const struct krb5_cc_type *clp;
    krb5_ccache p;
    int code;

    *cache = 0;
    ops = 0;
    if (name && (cp = strchr(name, ':'))) {
	for (clp = krb5i_cc_typelist; clp; clp = clp->next)
		if (!memcmp(clp->ops->prefix, name, cp-name)) {
		    ops = clp->ops;
		    break;
		}
	++cp;
    } else {
	ops = krb5_cc_dfl_ops;
	cp = name;
    }
    if (!ops) return KRB5_CC_UNKNOWN_TYPE;
    if (!(p = (krb5_ccache) malloc(sizeof *p)))
	return ENOMEM;
    memset(p, 0, sizeof *p);
    p->ops = ops;
    code = ops->resolve(context, p, cp);
    if (code)
	free(p);
    else
	*cache = p;
    return code;
}

krb5_error_code
krb5_cc_register(krb5_context context, krb5_cc_ops *ops, krb5_boolean override)
{
    struct krb5_cc_type *clp, **clpp;

    for (clpp = &krb5i_cc_typelist; (clp = *clpp); clpp = &clp->next)
	if (!strcmp(clp->ops->prefix, ops->prefix)) break;
    if (!clp) {
	clp = (struct krb5_cc_type *) malloc(sizeof *clp);
	memset(clp, 0, sizeof *clp);
	clp->next = *clpp;
	*clpp = clp;
    }
    if (clp->ops && !override)
	return KRB5_CC_TYPE_EXISTS;
    clp->ops = ops;
    return 0;
}

krb5_error_code
krb5_cc_store_cred(krb5_context context, krb5_ccache cache,
    krb5_creds *creds)
{
    return cache->ops->store_cred(context, cache, creds);
}

krb5_error_code
krb5_cc_start_seq_get(krb5_context context, krb5_ccache cache,
	krb5_cc_cursor *cursor)
{
    return cache->ops->start_get(context, cache, cursor);
}

krb5_error_code
krb5_cc_next_cred(krb5_context context, krb5_ccache cache,
	krb5_cc_cursor *cursor, krb5_creds *outcred)
{
    return cache->ops->get_next(context, cache, cursor, outcred);
}

krb5_error_code
krb5_cc_end_seq_get(krb5_context context, krb5_ccache cache,
	krb5_cc_cursor *cursor)
{
    return cache->ops->end_get(context, cache, cursor);
}

krb5_error_code
krb5_cc_close (krb5_context context, krb5_ccache cache)
{
    int code;

    code = cache->ops->close(context, cache);
    free(cache);
    return code;
}

int
krb5i_cc_pref(krb5_enctype enctype, krb5_enctype *ktypes)
{
    int i;

    if (!ktypes) return 0;
    for (i = 0; ktypes[i]; ++i)
	if (enctype == ktypes[i])
	    break;
    return i;
}

krb5_error_code
krb5_cc_retrieve_cred(krb5_context context, krb5_ccache cache,
    krb5_flags flags, krb5_creds *match, krb5_creds *out_creds)
{
    int code;
    krb5_cc_cursor cursor[1];
    krb5_creds fetched[1], best[1];
    krb5_enctype *ktypes;
    int bestpref, pref, wrong_ktype;

    if ((code = krb5_get_tgs_ktypes(context, match->server, &ktypes)))
	return code;
    *cursor = 0;
    code = krb5_cc_start_seq_get(context, cache, cursor);
    if (code) goto Done;
    bestpref = 999;
    memset(best, 0, sizeof *best);
    wrong_ktype = 0;
    while (!(code = krb5_cc_next_cred(context, cache, cursor, fetched))) {
	if (krb5i_compare_creds(context, flags, match, fetched)) {
	    if ((flags & KRB5_TC_MATCH_KTYPE)
		    && match->keyblock.enctype != fetched->keyblock.enctype) {
		wrong_ktype = KRB5_CC_NOT_KTYPE;
		continue;
	    }
	    pref = krb5i_cc_pref(fetched->keyblock.enctype, ktypes);
	    if ((flags & KRB5_TC_SUPPORTED_KTYPES) && ktypes && !ktypes[pref]) {
		wrong_ktype = KRB5_CC_NOT_KTYPE;
		continue;
	    }
	    if (pref < bestpref) {
		krb5_free_cred_contents(context, best);
		bestpref = pref;
		*best = *fetched;
		continue;
	    }
	}
	krb5_free_cred_contents(context, fetched);
    }
    if (code == KRB5_CC_END)
	code = KRB5_CC_NOTFOUND;
    if (bestpref < 999) {
	code = 0;
	*out_creds = *best;
    }
    if (code && wrong_ktype) code = wrong_ktype;
Done:
    if (ktypes)
	free(ktypes);
    if (*cursor)
	krb5_cc_end_seq_get(context, cache, cursor);
    return code;
}

const char *
krb5_cc_get_name(krb5_context context, krb5_ccache cache)
{
    return cache->ops->get_name(context, cache);
}

krb5_error_code
krb5_cc_initialize(krb5_context context, 
    krb5_ccache cache,
    krb5_principal principal)
{
    return cache->ops->init(context, cache, principal);
}
