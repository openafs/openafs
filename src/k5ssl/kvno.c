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
#include <errno.h>
#include <string.h>
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include "krb5.h"
#else
#include "k5ssl.h"
#endif

char * eflag;
int aflag;
int qflag;
int Eflag;
int Kflag;
int nflag;
int vflag;

krb5_context k5context;
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
    if (1) {
#if USING_HEIMDAL
	what = "decode_Ticket";
	code = decode_Ticket(ccentry->ticket.data, ccentry->ticket.length,
	    tkt, &len);
#else
	what = "krb5_decode_ticket";
	code = krb5_decode_ticket(&ccentry->ticket, &tkt);
#endif
#if 0
	code = 0;
#else
	if (code) goto Failed;
#endif
    }
    if (!qflag)
	printf ("%s: kvno = %d\n", sname, (int) HM(*,) tkt->enc_part.kvno);
    if (Eflag) {
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
    if (Kflag || Eflag)
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
	fprintf(stderr,"Failed in %s - error %d (%s)\n", what, code, error_message(code));
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
process(char **names)
{
    krb5_ccache ccache = 0;
    int code;
    char *what;
    krb5_creds *outcreds, mcreds[1];
    char *name;
    char *p = 0;
    krb5_enctype enctype = 0;
    krb5_principal client = 0;

    what = "krb5_init_context";
    if (!k5context && (code = krb5_init_context(&k5context)))
	goto Failed;
    what = "krb5_string_to_enctype";
    if (eflag && (code = krb5_string_to_enctype
	    HM((k5context, eflag, &enctype),
		(eflag, &enctype) )))
	goto Failed;
    what = "krb5_cc_default";
    if ((code = krb5_cc_default(k5context, &ccache)))
	goto Failed;
    what = "krb5_cc_get_principal";
    if ((code = krb5_cc_get_principal(k5context, ccache, &client)))
	goto Failed;
    while ((name = *names++)) {
	memset(mcreds, 0, sizeof *mcreds);
	mcreds->client = client;
	what = "krb5_parse_name";
	code = krb5_parse_name(k5context, name, &mcreds->server);
	if (code) goto Failed;
	what = "krb5_unparse_name";
	code = krb5_unparse_name(k5context, mcreds->server, &p);
	if (code) goto Failed;
/*	printf ("Looking for: <%s>\n", p);  */
#if USING_HEIMDAL
	mcreds->session.keytype = enctype;
#else
	mcreds->keyblock.enctype = enctype;
#endif
	what = "krb5_get_credentials";
	code = krb5_get_credentials(k5context, 0, ccache,
	    mcreds, &outcreds);
	if (code) goto Failed;
	krb5_free_principal(k5context, mcreds->server);
	if ((code = show_cc_entry(outcreds)))
	    goto Failed2;
	krb5_free_creds(k5context, outcreds);
    }
    if (!code) goto Failed2;
Failed:
    fprintf(stderr,"Failed in %s - error %d (%s)\n", what, code, error_message(code));
Failed2:
    if (ccache) krb5_cc_close(k5context, ccache);
    if (client) krb5_free_principal(k5context, client);
    if (p) free(p);
    if (k5context) {
	krb5_free_context(k5context);
	k5context = 0;
    }
    return !!code;
}

int
main(int argc, char **argv)
{
    int f;
    char *argp;
    char *names[30];
    int ni = 0;

    f = 0;
    while (--argc > 0) if (*(argp = *++argv)=='-')
    while (*++argp) switch(*argp) {
    case 'a':
	++aflag;
	break;
    case 'E':
	++Eflag;
	break;
    case 'K':
	++Kflag;
	break;
    case 'n':
	++nflag;
	break;
    case 'v':
	++vflag;
	break;
    case 'e':
	if (argc <= 1) goto Usage;
	--argc;
	eflag = *++argv;
	break;
    case 'q':
	++qflag;
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
	fprintf(stderr, "Usage: kvno [-e etype] [-EKanq] [names...]\n");
	exit(1);
    }
    else names[ni++] = argp;

    names[ni] = 0;
    exit(process(names));
}
