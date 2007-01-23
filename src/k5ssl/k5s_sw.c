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

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include "k5ssl.h"

krb5_error_code
krb5_kt_register(krb5_context context, const krb5_kt_ops *ops)
{
    struct krb5_kt_type *p, **pp;
    int code = 0;
    for (pp = &krb5i_kt_typelist; (p = *pp); pp = &p->next)
	if (p->ops == ops)
	    break;
	else if (!strcmp(p->ops->prefix, ops->prefix)) {
	    code = KRB5_KT_TYPE_EXISTS;
	    break;
	}
    if (p)
	;
    else if (!(p = malloc(sizeof *p)))
	code = ENOMEM;
    else {
	p->next = *pp;
	p->ops = (krb5_kt_ops *)ops;
	*pp = p;
    }
    return code;
}

extern const struct _krb5_kt_ops krb5_ktf_ops;

struct krb5_fkt_cursor {
    FILE *fd;
    int version;
    int pos;
};

struct krb5_fkt_data {
    char name[1];
};

struct _krb5_keytab {
    const struct _krb5_kt_ops *ops;
    krb5_pointer data;
};

int
krb5i_putint(struct krb5_fkt_cursor *fcursor, int out)
{
    int r, code;
    if (fcursor->version != 1) r = ntohl(out); else r = out;
    code = fwrite((char *)&r, sizeof r, 1, fcursor->fd) != 1;
    return code ? KRB5_KT_IOERR : 0;
}

int
krb5i_putshort(struct krb5_fkt_cursor *fcursor, int out)
{
    short r;
    int code;
    if (fcursor->version != 1) r = ntohs(out); else r = out;
    code = fwrite((char *)&r, sizeof r, 1, fcursor->fd) != 1;
    return code ? KRB5_KT_IOERR : 0;
}

int
krb5i_putbyte(struct krb5_fkt_cursor *fcursor, int out)
{
    unsigned char r;
    int code;
    r = out;
    code = fwrite((char *)&r, sizeof r, 1, fcursor->fd) != 1;
    return code ? KRB5_KT_IOERR : 0;
}

int
krb5i_putdata(struct krb5_fkt_cursor *fcursor, krb5_data *kd)
{
    int code;
    code = krb5i_putshort(fcursor, kd->length);
    if (code) return code;
    code = fwrite(kd->data, kd->length, 1, fcursor->fd) != 1;
    if (code)
	code = KRB5_KT_IOERR;
    return code;
}

krb5_error_code
krb5i_fkt_add_entry(krb5_context context, krb5_keytab kdesc, krb5_keytab_entry *e)
{
    struct krb5_fkt_cursor cursor[1];
    struct krb5_fkt_data *r = (struct krb5_fkt_data *)kdesc->data;
    krb5_data kd[1];
    int pos;
    int i;
    int code;

    memset(cursor, 0, sizeof *cursor);
    errno = 0;
    i = open(r->name, O_RDWR|O_CREAT, 0666);
    if (i >= 0)
	cursor->fd = fdopen(i, "r+");
    if (!cursor->fd) {
	if (!(code = errno)) code = EDOM;
	goto Done;
    }
    fseek(cursor->fd, (off_t) 0, 2);
    pos = ftell(cursor->fd);
    if (pos) {
	cursor->pos = pos;
	code = KRB5_KEYTAB_BADVNO;
	fseek(cursor->fd, (off_t) 0, 0);
	if (getc(cursor->fd) != 5) {
	    goto Done;
	}
	cursor->version = getc(cursor->fd);
	switch(cursor->version) {
	case 1:	/* XXX */
	case 2:
	    break;
	default:
	    goto Done;
	}
	fseek(cursor->fd, (off_t) cursor->pos, 0);
    } else {
	if ((code = krb5i_putbyte(cursor, 5)))
	    goto Done;
	if ((code = krb5i_putbyte(cursor, 2)))
	    goto Done;
	cursor->version = 2;
	cursor->pos = ftell(cursor->fd);
    }

    if ((code = krb5i_putint(cursor, 0)))
	goto Done;
    if ((code = krb5i_putshort(cursor,
	    e->principal->length + (cursor->version == 1))))
	goto Done;
    if ((code = krb5i_putdata(cursor, &e->principal->realm)))
	goto Done;
    for (i = 0; i < e->principal->length; ++i) {
	if ((code = krb5i_putdata(cursor, e->principal->data+i)))
	    goto Done;
    }
    if (cursor->version != 1) {
	if ((code = krb5i_putint(cursor, e->principal->type)))
	    goto Done;
    }
    if ((code = krb5i_putint(cursor, e->timestamp)))
	goto Done;
    if ((code = krb5i_putbyte(cursor, e->vno)))
	goto Done;
    if ((code = krb5i_putshort(cursor, e->key.enctype)))
	goto Done;
    kd->length = e->key.length;
    kd->data = e->key.contents;
    if ((code = krb5i_putdata(cursor, kd)))
	goto Done;
    if (e->vno != (unsigned char) e->vno) {
	if ((code = krb5i_putint(cursor, e->vno)))
	    goto Done;
    }
    pos = ftell(cursor->fd);
    fseek(cursor->fd, (off_t) cursor->pos, 0);
    if ((code = krb5i_putint(cursor, pos - cursor->pos - 4)))
	goto Done;
    /* fseek(cursor->fd, (off_t) pos, 0);
    cursor->pos = pos; */
Done:
    if (cursor->fd)
	fclose(cursor->fd);
    return code;
}

int krb5i_fkt_resolve(krb5_context, krb5_keytab, const char *);
int krb5i_fkt_close(krb5_context, krb5_keytab);
int krb5i_fkt_start_seq_get(krb5_context, krb5_keytab, krb5_kt_cursor *);
int krb5i_fkt_end_seq_get(krb5_context, krb5_keytab, krb5_kt_cursor *);
int krb5i_fkt_next_entry(krb5_context, krb5_keytab, krb5_keytab_entry *,
    krb5_kt_cursor *);

const struct _krb5_kt_ops krb5_ktf_wops = {
    "WRFILE",
    krb5i_fkt_resolve,
    krb5i_fkt_start_seq_get,
    krb5i_fkt_next_entry,
    krb5i_fkt_end_seq_get,
    krb5i_fkt_close,
};

krb5_error_code
krb5_kt_add_entry(krb5_context context, krb5_keytab kdesc, krb5_keytab_entry *e)
{
#if 0
    if (!kdesc->ops->add) return KRB5_KT_NOWRITE;
    return kdesc->ops->add(context, kdesc, e);
#else
    if (kdesc->ops != &krb5_ktf_wops) return KRB5_KT_NOWRITE;
    return krb5i_fkt_add_entry(context, kdesc, e);
#endif
}

krb5_error_code
krb5_ktfile_wresolve(krb5_context context, const char *name, krb5_keytab *kdescp)
{
    int code;
    if ((code = krb5_kt_register(context, &krb5_ktf_wops)))
	return code;
    if ((code = krb5_kt_resolve(context, name, kdescp)))
	return code;
    if ((*kdescp)->ops == &krb5_ktf_ops)
	(*kdescp)->ops = &krb5_ktf_wops;
    return 0;
}
