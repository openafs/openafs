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
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#include "krb5.h"
#else
#include "k5ssl.h"
#endif

int eflag;
int tflag;
int fflag;
int aflag;
int nflag;
int Kflag;
int Vflag;

krb5_context k5context;
int exitcode;
#if USING_HEIMDAL
#define HM(h,m)	h
#else
#define HM(h,m)	m
#endif

int
show_cc_entry(krb5_creds *ccentry)
{
    int code;
    char *what;
    char *cname = 0, *sname = 0;
    krb5_address **ap;
    int i, j;
#if USING_HEIMDAL
    Ticket tkt[1];
    size_t len;
#else
    krb5_ticket *tkt = 0;
#endif

#if USING_HEIMDAL
    memset(tkt, 0, sizeof *tkt);
#endif
    what = "krb5_unparse_name";
    cname = 0;
    code = krb5_unparse_name(k5context, ccentry->client, &cname);
    if (code) goto Failed;
    sname = 0;
    code = krb5_unparse_name(k5context, ccentry->server, &sname);
    if (code) goto Failed;
    printf ("server: <%s>", sname);
    printf (" issued=%#x expires=%#x\n",
	ccentry->times.starttime,
	ccentry->times.endtime);
    if (eflag || Vflag) {
#if USING_HEIMDAL
	what = "decode_Ticket";
	code = decode_Ticket(ccentry->ticket.data, ccentry->ticket.length,
	    tkt, &len);
#else
	what = "krb5_decode_ticket";
	code = krb5_decode_ticket(&ccentry->ticket, &tkt);
#endif
#if 1
	code = 0;
#else
	if (code) goto Failed;
#endif
    }
    if (Vflag)
	printf (" kvno=%d", (int) HM(*,) tkt->enc_part.kvno);
    if (fflag) {
	printf (" fflags=%#x", (int) ccentry->HM(flags.i,ticket_flags));
    }
    if (eflag) {
	printf (" skey.enctype=%#x",
	    ccentry->HM(session.keytype,keyblock.enctype));
	if (tkt)
	printf (" tkt.enctype=%#x", tkt->enc_part.HM(etype,enctype));
	else
	printf (" tkt.enctype=?");
    }
    if (Kflag) {
	printf (" length=%#x: ",
	    ccentry->HM(session.keyvalue.length,keyblock.length));
	for (i = 0; i < ccentry->HM(session.keyvalue.length,
		keyblock.length); ++i)
	    printf ("%02x",
		i[HM((unsigned char *)ccentry->session.keyvalue.data,
		    ccentry->keyblock.contents)]);
    }
    if (Vflag || Kflag || eflag || fflag)
	printf ("\n");
    if (!aflag)
	;
    else if (HM(!ccentry->addresses.len,
	!ccentry->addresses || !*ccentry->addresses))
	printf ("No addresses\n");
    else {
	ap = HM(&,)ccentry->addresses HM(.val,);
	for (i = 0; HM(i < ccentry->addresses.len,ap[i]); ++i) {
/* XXX nflag == print numeric instead of symbolic */
	    printf (" address type=%d len=%d ",
		ap[i]->HM(addr_type,addrtype), ap[i]->HM(address.,)length);
	    for (j = 0; j < ap[i]->HM(address.,)length; ++j)
		printf ("%02x", j[HM((unsigned char *)ap[i]->address.data,ap[i]->contents)]);
	    printf ("\n");
	}
    }
Failed:
    if (code)
	fprintf(stderr,"Failed in %s - error %d (%s)\n",
	    what, code, afs_error_message(code));
    if (sname) free(sname);
    if (cname) free(cname);
#if USING_HEIMDAL
    free_Ticket(tkt);
#else
    if (tkt) krb5_free_ticket(k5context, tkt);
#endif
    return code;
}

int
read_ccache_all(char *fn)
{
    krb5_ccache ccache = 0;
    int code, r;
    char *what, *pname = 0;
    krb5_cc_cursor cursor;
    krb5_creds ccentry[1];
    krb5_principal princ = 0;

    what = "krb5_init_context";
    if (!k5context && (code = krb5_init_context(&k5context)))
	goto Failed;
    if (!fn) {
	what = "krb5_cc_default";
	if ((code = krb5_cc_default(k5context, &ccache)))
	    goto Failed;
    } else {
	what = "krb5_cc_resolve";
	if ((code = krb5_cc_resolve(k5context, fn, &ccache)))
	    goto Failed;
    }
    what = krb5_cc_get_name(k5context, ccache);
    if (what) {
	printf ("credentials cache: %s\n", what);
    } else {
	printf ("no cache name?\n");
    }

    what = "krb5_cc_get_principal";
    code = krb5_cc_get_principal(k5context, ccache, &princ);
    if (code) goto Failed;
    krb5_unparse_name(k5context, princ, &pname);
    printf ("Principal: <%s>\n", pname);

    what = "krb5_cc_start_seq_get";
    code = krb5_cc_start_seq_get(k5context, ccache, &cursor);
    if (code) goto Failed;
    what = "krb5_cc_next_cred";
    while (!(code = krb5_cc_next_cred(k5context, ccache, &cursor, ccentry))) {
	code = show_cc_entry(ccentry);
	if (code) return 1;
	krb5_free_cred_contents(k5context, ccentry);
    }
    if (code == KRB5_CC_END)
	code = 0;
if (!code)
what = "krb5_cc_end_seq_get";
    r = krb5_cc_end_seq_get(k5context, ccache, &cursor);
if (!code && r)
{
code = r;
}

    if (!code) goto Success;
Failed:
    fprintf(stderr,"Failed in %s - error %d (%s)\n",
	what, code, afs_error_message(code));
Success:
    if (princ)
	krb5_free_principal(k5context, princ);
    if (pname)
	free(pname);
    if (ccache) krb5_cc_close(k5context, ccache);
    return !!code;
}

int
read_ccache_lookup(char **names)
{
    krb5_ccache ccache = 0;
    int code;
    char *what;
    krb5_creds ccentry[1], mcreds[1];
    char *name;
    char *fn;

    krb5_principal princ = 0;

    if ((fn = *names))
	++names;

    what = "krb5_init_context";
    if (!k5context && (code = krb5_init_context(&k5context)))
	goto Failed;
    if (!fn) {
	what = "krb5_cc_default";
	if ((code = krb5_cc_default(k5context, &ccache)))
	    goto Failed;
    } else {
	what = "krb5_cc_resolve";
	if ((code = krb5_cc_resolve(k5context, fn, &ccache)))
	    goto Failed;
    }
    while ((name = *names++)) {
	krb5_parse_name(k5context, name, &mcreds->server);
{
char *p;
p = 0;
krb5_unparse_name(k5context, princ, &p);
printf ("Looking for: <%s>\n", p);
free(p);
}
	what = "cc_retrieve_entry";
	code = krb5_cc_retrieve_cred(k5context, ccache,
	    0,
	    mcreds, ccentry);
	if (code) goto Failed;
	if ((code = show_cc_entry(ccentry))) {
	    goto Done;
	}
    }
Failed:
    fprintf(stderr,"Failed in %s - error %d (%s)\n",
	what, code, afs_error_message(code));
Done:
    if (ccache) krb5_cc_close(k5context, ccache);
    return !!code;
}

int
show_kt_entry(krb5_keytab_entry *ktentry)
{
    int code;
    char *what;
    char *pname;
    int i;

    what = "krb5_unparse_name";
    pname = 0;
    code = krb5_unparse_name(k5context, ktentry->principal, &pname);
    if (code) goto Failed;
    printf ("princ: <%s> kvno %#x", pname, ktentry->vno);
    if (tflag)
	printf (" timestamp %#x", (int) ktentry->timestamp);
    printf ("\n");
    if (eflag)
	printf (" enctype=%#x",
	    HM(ktentry->keyblock.keytype,
		ktentry->key.enctype));
    if (Kflag) {
	printf (" length=%#x: ", ktentry->HM(keyblock.keyvalue.length,key.length));
	for (i = 0; i < ktentry->HM(keyblock.keyvalue.length,
		key.length); ++i)
	    printf ("%02x", i[HM((unsigned char *)ktentry->
		keyblock.keyvalue.data,ktentry->key.contents)]);
    }
    if (Kflag || eflag)
	printf ("\n");
Failed:
    if (code)
	fprintf(stderr,"Failed in %s - error %d (%s)\n",
	    what, code, afs_error_message(code));
    return code;
}

int
read_keytab_all(char *fn)
{
    krb5_keytab keytab = 0;
    int code, r;
    char *what;
    krb5_kt_cursor cursor;
    krb5_keytab_entry ktentry[1];

    what = "krb5_init_context";
    if (!k5context && (code = krb5_init_context(&k5context)))
	goto Failed;
    if (!fn) {
	what = "krb5_kt_default";
	if ((code = krb5_kt_default(k5context, &keytab)))
	    goto Failed;
    } else {
	what = "krb5_kt_resolve";
	if ((code = krb5_kt_resolve(k5context, fn, &keytab)))
	    goto Failed;
    }

    what = "krb5_kt_start_seq_get";
    code = krb5_kt_start_seq_get(k5context, keytab, &cursor);
    if (code) goto Failed;
    what = "krb5_kt_next_entry";
    while (!(code = krb5_kt_next_entry(k5context, keytab, ktentry, &cursor))) {
	code = show_kt_entry(ktentry);
	if (code) goto Success;
	return 1;
#if USING_HEIMDAL
	krb5_kt_free_entry(k5context, ktentry);
#else
	krb5_free_keytab_entry_contents(k5context, ktentry);
#endif
    }
    if (code == KRB5_KT_END)
	code = 0;
if (!code)
what = "krb5_kt_end_seq_get";
    r = krb5_kt_end_seq_get(k5context, keytab, &cursor);
if (!code && r)
{
code = r;
}

    if (!code) goto Success;
Failed:
    fprintf(stderr,"Failed in %s - error %d (%s)\n",
	what, code, afs_error_message(code));
Success:
    if (keytab) krb5_kt_close(k5context, keytab);
    return !!code;
}

int
read_keytab_lookup(char **names)
{
    krb5_keytab keytab = 0;
    int code;
    char *what;
    krb5_keytab_entry ktentry[1];
    char *name;
    char *fn;

    krb5_principal princ = 0;
    int vno = 0;
    int enctype = 0;

    if ((fn = *names))
	++names;

    what = "krb5_init_context";
    if (!k5context && (code = krb5_init_context(&k5context)))
	goto Failed;
    if (!fn) {
	what = "krb5_kt_default";
	if ((code = krb5_kt_default(k5context, &keytab)))
	    goto Failed;
    } else {
	what = "krb5_kt_resolve";
	if ((code = krb5_kt_resolve(k5context, fn, &keytab)))
	    goto Failed;
    }
    while ((name = *names++)) {
	krb5_parse_name(k5context, name, &princ);
{
char *p;
p = 0;
krb5_unparse_name(k5context, princ, &p);
printf ("Looking for: <%s>\n", p);
free(p);
}
	what = "kt_get_entry";
	code = krb5_kt_get_entry(k5context, keytab, princ, vno, enctype, ktentry);
	if (code) goto Failed;
	if ((code = show_kt_entry(ktentry)))
	    goto Success;
    }
Failed:
    fprintf(stderr,"Failed in %s - error %d (%s)\n",
	what, code, afs_error_message(code));
Success:
    if (keytab) krb5_kt_close(k5context, keytab);
    return !!code;
}

int
main(int argc, char **argv)
{
    int f;
    char *argp;
    char *names[30];
    int ni = 0;
    int mode = 'c';

    f = 0;
    while (--argc > 0) if (*(argp = *++argv)=='-')
    while (*++argp) switch(*argp) {
    case 'k':
    case 'c':
	mode = *argp;
	break;
    case 'V':
	++Vflag;
	break;
    case 'e':
	++eflag;
	break;
    case 't':
	++tflag;
	break;
    case 'f':
	++fflag;
	 break;
    case 'a':
	++aflag;
	break;
    case 'n':
	++nflag;
	break;
    case 'K':
	++Kflag;
	break;
#if 0
    case 'Z':
	if (argc <= 1) goto Usage;
	--argc;
	offset = atol(*++argv);
	break;
#endif
    case '-':
	break;
    default:
	fprintf (stderr,"Bad switch char <%c>\n", *argp);
    Usage:
	fprintf(stderr, "Usage: klist [-k[etK]|-c[aefKV]] [fn [names...]]\n");
	exit(1);
    }
    else names[ni++] = argp;

    names[ni] = 0;
    switch(mode) {
    case 'c':
	if (ni > 2)
	    exitcode = read_ccache_lookup(names);
	else
	    exitcode = read_ccache_all(*names);
	break;
    case 'k':
	if (ni > 2)
	    exitcode = read_keytab_lookup(names);
	else
	    exitcode = read_keytab_all(*names);
	break;
    default:
	goto Usage;
    }
    if (k5context) {
	krb5_free_context(k5context);
	k5context = 0;
    }
    exit(exitcode);
}
