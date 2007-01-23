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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#if defined(USE_SSL) || defined(USE_FAKESSL)
#include "k5ssl.h"
#else
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include <krb5.h>
#endif

struct _krb5_keytab {
    const struct _krb5_kt_ops *ops;
    krb5_pointer data;
};

/*
 * 
 * keytab format:
 * 
 * head:
 * 0 1	5
 * 1 1	VNO 1 or 2
 * per entry:
 * 0 4	len (excludes len)
 * 4 2	count of princ components (pc)
 * 6 2	length realm (rl)
 * 8 rl	realm
 * REP *pc {
 * 	0 2 length nl
 * 	2 nl name-component
 * }
 * IF new? {
 * xxx 4	name-type
 * }
 * xxx 4	timestamp
 * xxx 1	vno
 * {
 * 0 2	keytype
 * 2 2	keylen
 * 4 keylen	keydata
 * }
 * POSSIBLE if length left {
 * xxx 4	vno
 * }
 */

struct krb5_fkt_data {
    char name[1];
};

struct krb5_fkt_cursor {
    FILE *fd;
    int version;
    int pos;
};

int
krb5i_fkt_close(krb5_context context, krb5_keytab kdesc)
{
    if (kdesc) {
	struct krb5_fkt_data *fkdesc = (struct krb5_fkt_data *) kdesc->data;
	if (fkdesc) {
	    free(fkdesc);
	}
	free(kdesc);
    }
    return 0;
}

int
krb5i_fkt_resolve(krb5_context context, krb5_keytab kdesc, const char *filename)
{
    struct krb5_fkt_data *r;
    int l;
    r = malloc(sizeof *r + (l = strlen(filename)));
    if (!r) return ENOMEM;
    memset(r, 0, sizeof *r);
    memcpy(r->name, filename, l+1);
    kdesc->data = r;
    return 0;
}

int
krb5i_fkt_open(krb5_context context, struct krb5_fkt_data *r,
    struct krb5_fkt_cursor *cursor, int mode)
{
    int code;
    FILE *fp;

    memset(cursor, 0, sizeof *cursor);
    errno = 0;
    fp = fopen(r->name, "r");
    if (!fp) {
	free(r);
	return errno ? errno : EDOM;
    }
    if (fcntl(fileno(fp), F_SETFD, 1) < 0) {
	code = errno ? errno : EDOM;
	goto Done;
    }
    code = KRB5_KEYTAB_BADVNO;
    if (getc(fp) != 5) {
	goto Done;
    }
    cursor->version = getc(fp);
    switch(cursor->version) {
    case 1:		/* XXX */
    case 2:
	break;
    default:
	goto Done;
    }
    code = 0;
Done:
    if (code) fclose(fp);
    else cursor->fd = fp;
    return code;
}

int krb5i_fkt_start_seq_get(krb5_context context, krb5_keytab kdesc,
    krb5_kt_cursor *cursor)
{
    struct krb5_fkt_cursor *fcursor;
    int code;

    *cursor = 0;
    fcursor = malloc(sizeof *fcursor);
    if (!fcursor) return ENOMEM;
    if ((code = krb5i_fkt_open(context, (struct krb5_fkt_data *)kdesc->data,
	    fcursor, 0))) {
	goto Done;
    }
    fcursor->pos = ftell(fcursor->fd);
Done:
    if (code) {
	fclose(fcursor->fd);
	free(fcursor);
    } else *cursor = (krb5_kt_cursor) fcursor;
    return 0;
}

int
krb5i_getint(struct krb5_fkt_cursor *fcursor, int *out)
{
    int r, code;
    code = fread((char*)&r, sizeof r, 1, fcursor->fd) != 1;
    if (fcursor->version != 1) *out = ntohl(r); else *out = r;
    return code ? KRB5_KT_IOERR : 0;;
}

int
krb5i_getshort(struct krb5_fkt_cursor *fcursor, short *out)
{
    short r;
    int code;

    code = fread((char*)&r, sizeof r, 1, fcursor->fd) != 1;
    if (fcursor->version != 1) *out = ntohs(r); else *out = r;
    return code ? KRB5_KT_IOERR : 0;;
}

int
krb5i_getbyte(struct krb5_fkt_cursor *fcursor, unsigned char *out)
{
    unsigned char r;
    int code;

    code = fread((char*)&r, sizeof r, 1, fcursor->fd) != 1;
    *out = r;
    return code ? KRB5_KT_IOERR : 0;;
}

int
krb5i_getdata(struct krb5_fkt_cursor *fcursor, krb5_data *kd)
{
    int l;
    unsigned char *r;
    int code;
    short stemp;

    code = krb5i_getshort(fcursor, &stemp);
    if (code) return code;
    l = (unsigned short) stemp;
    r = malloc(l+1);
    r[l] = 0;
    code = fread((char*)r, l, 1, fcursor->fd) != 1;
    if (code) {
	free(r);
	r = 0;
    }
    kd->data = r;
    kd->length = l;
    return code ? KRB5_KT_IOERR : 0;;
}

int
krb5i_fkt_end_seq_get(krb5_context context, krb5_keytab kdesc,
    krb5_kt_cursor *cursor)
{
    struct krb5_fkt_cursor *fcursor = (struct krb5_fkt_cursor *) *cursor;

    fclose(fcursor->fd);
    free(fcursor);
    return 0;
}

int
krb5i_fkt_next_entry(krb5_context context, krb5_keytab kdesc,
    krb5_keytab_entry *entry, krb5_kt_cursor *cursor)
{
    struct krb5_fkt_cursor *fcursor = (struct krb5_fkt_cursor *) *cursor;
    int len, i, code;
    krb5_data kd[1];
    int off, pos;
    short stemp;
    int temp;
    unsigned char ctemp;

    pos = fcursor->pos;
    memset(entry, 0, sizeof *entry);
    fseek(fcursor->fd, (off_t) pos, 0);
/* length(4) count(2) realm names*n type(4)? vno(1)
 * [ type(2) len(2) data ] ???vno(4)
 */
    for (;;) {
	if ((code = krb5i_getint(fcursor, &len)))
	    return KRB5_KT_END;
	pos += 4;
	if (!len)
	    return KRB5_KT_END;
	if (len > 0) break;
	fseek(fcursor->fd, (off_t) -len, 1);
	pos -= len;
    }
    pos += len;
    fcursor->pos = pos;
    if ((code = krb5i_getshort(fcursor, &stemp)))
	return code;
    stemp -= fcursor->version == 1;
    entry->principal = malloc(sizeof *entry->principal);
    entry->principal->length = (unsigned short) stemp;
    entry->principal->data = (krb5_data *)
	malloc(sizeof *entry->principal->data * entry->principal->length);
    if ((code = krb5i_getdata(fcursor, &entry->principal->realm)))
	goto Failed;
    for (i = 0; i < entry->principal->length; ++i) {
	if ((code = krb5i_getdata(fcursor, entry->principal->data+i)))
	    goto Failed;
    }
    if (fcursor->version == 2) {
	if ((code = krb5i_getint(fcursor, &temp)))
	    goto Failed;
	entry->principal->type = temp;
    }
    if ((code = krb5i_getint(fcursor, &temp)))
	goto Failed;
    entry->timestamp = temp;
    if ((code = krb5i_getbyte(fcursor, &ctemp)))
	goto Failed;
    entry->vno = ctemp;
    if ((code = krb5i_getshort(fcursor, &stemp)))
	goto Failed;
    entry->key.enctype = stemp;
    if ((code = krb5i_getdata(fcursor, kd)))
	    goto Failed;
    entry->key.length = kd->length;
    entry->key.contents = kd->data;
    off = (int) ftell(fcursor->fd);
    if (fcursor->version == 2 && off + 4 == pos) {
	if ((code = krb5i_getint(fcursor, &temp)))
	    goto Failed;
	entry->vno = temp;
    }
    if (code) {
Failed:
	krb5_free_keytab_entry_contents(context, entry);
    }
    return code;
}

const struct _krb5_kt_ops krb5_ktf_ops = {
    "FILE",
    krb5i_fkt_resolve,
    krb5i_fkt_start_seq_get,
    krb5i_fkt_next_entry,
    krb5i_fkt_end_seq_get,
    krb5i_fkt_close,
};

struct krb5_kt_type *krb5i_kt_typelist;
const struct _krb5_kt_ops * krb5_kt_dfl_ops = &krb5_ktf_ops;

int krb5_kt_resolve(krb5_context context, const char *name, krb5_keytab *kdescp)
{
    const char *cp;
    const struct _krb5_kt_ops *ops;
    struct krb5_kt_type *clp;
    krb5_keytab p;
    int code;

    *kdescp = 0;
    ops = 0;
    if ((cp = strchr(name, ':'))) {
	for (clp = krb5i_kt_typelist; clp; clp = clp->next)
		if (!memcmp(clp->ops->prefix, name, cp-name)) {
		    ops = clp->ops;
		    break;
		}
	++cp;
    } else {
	ops = krb5_kt_dfl_ops;
	cp = name;
    }
    if (!ops) return KRB5_KT_UNKNOWN_TYPE;
    if (!(p = (krb5_keytab) malloc(sizeof *p)))
	return ENOMEM;
    memset(p, 0, sizeof *p);
    p->ops = ops;
    code = ops->resolve(context, p, cp);
    if (code)
	free(p);
    else
	*kdescp = p;
    return code;
}

int krb5_kt_start_seq_get(krb5_context context, krb5_keytab kdesc,
    krb5_kt_cursor *cursor)
{
    return kdesc->ops->start_seq_get(context, kdesc, cursor);
}

int krb5_kt_next_entry(krb5_context context, krb5_keytab kdesc,
    krb5_keytab_entry *entry, krb5_kt_cursor *cursor)
{
    return kdesc->ops->get_next(context, kdesc, entry, cursor);
}

int krb5_kt_end_seq_get(krb5_context context, krb5_keytab kdesc,
    krb5_kt_cursor *cursor)
{
    return kdesc->ops->end_seq_get(context, kdesc, cursor);
}
int krb5_kt_close(krb5_context context, krb5_keytab kdesc)
{
    return kdesc->ops->close(context, kdesc);
}

void
krb5_free_keytab_entry_contents(krb5_context context, krb5_keytab_entry *entry)
{
    krb5_free_principal(context, entry->principal);
    entry->principal = 0;
    krb5_free_keyblock_contents(context, &entry->key);
    entry->key.contents = 0;
}

int
krb5_kt_get_entry(krb5_context context, krb5_keytab kdesc,
    krb5_const_principal princ, krb5_kvno vno, krb5_enctype enctype,
    krb5_keytab_entry *entry)
{
    int code, r;
    krb5_kt_cursor cursor;

    code = krb5_kt_start_seq_get(context, kdesc, &cursor);
    if (code) return code;
    memset(entry, 0, sizeof *entry);
    for (;;) {
	krb5_free_keytab_entry_contents(context, entry);
	code = krb5_kt_next_entry(context, kdesc, entry, &cursor);
	if (code) break;
	if (princ && !krb5_principal_compare(context, entry->principal, princ))
	    continue;
	if (vno && vno != entry->vno)
	    continue;
	if (enctype && enctype != entry->key.enctype)
	    continue;
	break;
    }
    r = krb5_kt_end_seq_get(context, kdesc, &cursor);
    if (!code) code = r;
    return code;
}
