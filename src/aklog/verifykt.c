/*
 * Copyright (c) 2007
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

/*
 * verifykt - check principals in a keytab.  Do they work?
 */

#include <afsconfig.h>
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

krb5_context k5context;
int exitcode;
#if USING_HEIMDAL
#define krb5_free_keytab_entry_contents krb5_kt_free_entry
#define HM(h,m)	h
#else
#define HM(h,m)	m
#endif
#ifdef USING_MIT
#define MO(x)	x
#else
#define	MO(X)	/**/
#endif

int vflag;
int pflag;

/* RFC 2045. */

struct pemstate {
    int pemfrag, pems, pemw, pemcount;
    int (*pemf)();
    char *pemarg;
    int pempos;
    char pembuf[64];
};

struct pemstate *
pemopen(int (*f)(), void *arg)
{
    struct pemstate *result;

    if (result = (struct pemstate *) malloc(sizeof *result)) {
	memset(result, 0, sizeof *result);
	result->pemf = f;
	result->pemarg = arg;
    }
    return result;
}

int
pemwrite(struct pemstate *state, void *buf, int s)
{
    int c, i, frag, x = s;
    unsigned char *bp = buf;

    if (!state->pemw) state->pemw = 1;
    while (x) {
	--x; c = *bp++;
	for (;;) {
	    switch(state->pems) {
	    case 5:
		return -1;
	    case 3:
		state->pems -= 4;
		++state->pemcount;
	    default:
		++state->pems;
		switch(state->pems) {
		case 1:
		    if (state->pemcount >= 19) {
			state->pemcount = 0;
			if (state->pempos < sizeof state->pembuf)
			    ;
			else if ((*state->pemf)(state->pemarg,
				state->pembuf, state->pempos) != state->pempos) {
			    goto Fail;
			} else state->pempos = 0;
			state->pembuf[state->pempos++] = '\n';
		    }
		    state->pemfrag = (c & 3) << 4;
		    c >>= 2;
		    break;
		case 2:
		    frag = state->pemfrag;
		    state->pemfrag = (c & 15) << 2;
		    c >>= 4;
		    c += frag;
		    break;
		case 3:
		    frag = state->pemfrag;
		    state->pemfrag = (c & 63);
		    c >>= 6;
		    c += frag;
		    break;
		case 0:
		    c = state->pemfrag;
		    break;
		}
		if (c < 26)
			c += 'A';
		else if (c < 52)
			c += ('a'-26);
		else if (c < 62)
			c += ('0'-52);
		else c = "+/"[c-62];
		if (state->pempos < sizeof state->pembuf)
		    ;
		else if ((*state->pemf)(state->pemarg,
			state->pembuf, state->pempos) != state->pempos) {
		Fail:
		    state->pems = 5;
		    return -1;
		}
		else state->pempos = 0;
		state->pembuf[state->pempos++] = c;
		if (state->pems == 3 && state->pemw != 2) continue;
	    }
	    break;
	}
    }
    return s;
}

int
pemclose(struct pemstate *state)
{
    int r;

    if (!state) return -1;
    if (state->pemw && state->pems) {
	char *cp = "==" + (state->pems-1);
	state->pemw = 2;
	pemwrite(state, "", 1);
	while (*cp) {
	    if (state->pempos < sizeof state->pembuf)
		;
	    else if ((*state->pemf)(state->pemarg,
		    state->pembuf, state->pempos) != state->pempos) {
		break;
	    }
	    else state->pempos = 0;
	    state->pembuf[state->pempos++] = *cp++;
	}
    }
    if (state->pempos)
    (void) (*state->pemf)(state->pemarg,
	    state->pembuf, state->pempos);
    r = -(state->pems < 5);
    free((char*)state);
    return r;
}

int
writebuf(FILE *f, void *buf, int s)
{
    return fwrite(buf, 1, s, f);
}

int
pem_dump(char *cp, int s, int ino)
{
    struct pemstate *state;

    state = pemopen(writebuf, stdout);
    pemwrite(state, cp, s);
    pemclose(state);
    puts("");
}

int
bin_dump(char *cp, int s, int ino)
{
    char *buffer;
    char c;
    int w;
    int i;
    long o;

    o = 0;
    buffer = cp;
    while (s > 0) {
	c = 16;
	if (c > s) c = s;
	printf ("%06lx:", ino+o);
	w = 0;
#if 0
#define WLIM 41
	for (i = 0; i < c/2; ++i)
	    w += 5, printf (" %4x",
(((int)(((unsigned char)(buffer[i<<1]))))<<8)+
((int)(((unsigned char)(buffer[(i<<1)+1])))));
	if (c & 1)
	    w += 3, printf (" %2x", (unsigned char)(buffer[c-1]));
#else
#define WLIM 49
	for (i = 0; i < c; ++i)
	    w += 3, printf (" %02x", (unsigned char)(buffer[i]));
#endif
	while (w < WLIM)
	    ++w, putchar(' ');
	for (i = 0; i < c; ++i)
	    if (isascii(buffer[i]) && isprint(buffer[i]))
		putchar(buffer[i]);
	    else
		putchar('.');
	putchar('\n');
	o += c;
	buffer += c;
	s -= c;
    }
    return 1;
}

int tflag = 1;
int eflag = 1;
int Kflag = 1;

int
show_keyblock(krb5_keyblock *kb)
{
    int i;

    if (eflag)
	printf (" enctype=%#x", kb-> HM(keytype, enctype));
    if (Kflag) {
	printf (" length=%#x: ",
	    kb-> HM(keyvalue.length,length));
	for (i = 0; i < kb-> HM(keyvalue.length, length); ++i)
	    printf ("%02x", i[(unsigned char *)kb-> HM(
		keyvalue.data,contents)]);
	if (pflag & 2) {
	    printf ("\nkey as pem:\n");
	    pem_dump((unsigned char *)
		kb-> HM(keyvalue.data,contents),
		kb-> HM(keyvalue.length, length), 0);
	}
    }
    if (!(pflag & 2) && (Kflag || eflag))
	printf ("\n");
}

int
show_kt_entry(krb5_keytab_entry *ktentry)
{
    int code;
    char *what;
    char *pname;

    what = "krb5_unparse_name";
    pname = 0;
    code = krb5_unparse_name(k5context, ktentry->principal, &pname);
    if (code) goto Failed;
    printf ("princ: <%s> kvno %#x", pname, ktentry->vno);
    if (tflag)
	printf (" timestamp %#x", (int) ktentry->timestamp);
    printf ("\n");
    if (eflag || Kflag)
	show_keyblock(&ktentry-> HM(keyblock,key));
Failed:
    if (code)
	fprintf(stderr,"Failed in %s - error %d (%s)\n",
	    what, code, afs_error_message(code));
    return code;
}

int
krb5i_keyblock_compare(krb5_context context,
    const krb5_keyblock *a,
    const krb5_keyblock *b)
{
    if (a-> HM(keytype, enctype) != b-> HM(keytype, enctype))
	return 0;
    if (a-> HM(keyvalue.length,length) != b-> HM(keyvalue.length,length))
	return 0;
    return !memcmp(a-> HM(keyvalue.data,contents),
	b-> HM(keyvalue.data,contents),
	a-> HM(keyvalue.length,length));
}

struct vk_list {
    struct vk_list *next;
    krb5_enctype enctype;
    int vno;
    krb5_principal principal;
};

int
save_princ(krb5_enctype enctype,
    int vno,
    krb5_principal principal,
    struct vk_list **list)
{
    struct vk_list *p, **pp;
    int code;

    for (pp = list; p = *pp; ) {
	if (!krb5_principal_compare(k5context,
                    principal, p->principal))
	    ;
	else if (p->vno < vno) {
	    *pp = p->next;
	    krb5_free_principal(k5context, p->principal);
	    free(p);
	    continue;
	} else if (p->vno > vno || !enctype) {
	    return 0;
	} else if (p->enctype == enctype) {
	    char *p1 = 0;
	    static char nullstr[1];
	    (void) krb5_unparse_name(k5context, p->principal, &p1);
	    if (!p1) p1 = nullstr;
fprintf(stderr,"*** principal %s etype=%d vno=%d found more than once!\n",
p1, p->enctype, p->vno);
	    if (p1 != nullstr) free(p1);
	    return 0;
	}
	pp = &p->next;
    }
    p = malloc(sizeof *p);
    if (!p) {
	code = ENOMEM;
	goto Failed;
    }
    memset(p, 0, sizeof *p);
    p->enctype = enctype;
    p->vno = vno;
    if ((code = krb5_copy_principal(k5context, principal,
	    &p->principal))) {
	goto Failed;
    }
    *pp = p;
    p = 0;
Failed:
    if (p) free(p);
    return code;
}

#ifdef USING_HEIMDAL
#define _krb5_principalname2krb5_principal my_krb5_principalname2krb5_principal
static int
my_krb5_principalname2krb5_principal(krb5_principal *pp,
    const PrincipalName from,
    const Realm realm)
{
    krb5_principal p;
    int code = ENOMEM;

    *pp = 0;
    if (!(p = malloc(sizeof *p))) goto Done;
    memset(p, 0, sizeof *p);
    if (!(p->realm = strdup(realm))) goto Done;
    if ((code = copy_PrincipalName(&from, &p->name))) goto Done;
    *pp = p; p = 0;
    /* code = 0; */
Done:
    if (p) {
	free_Principal(p);
	free(p);
    }
    return code;
}
#endif

int
verify_keytab(char *fn, char **names)
{
    krb5_keytab keytab = 0;
    int code, r, failed;
    char *what;
    krb5_keytab_entry ktentry[1];
    char *name;
    char *princ_name = 0, *client_name = 0;
    struct vk_list *list = 0, *p, **pp = &list;
    krb5_principal princ = 0;
    krb5_get_init_creds_opt gic_opts[1];
    krb5_kt_cursor cursor;
    krb5_creds creds[1];
    krb5_data data[1];
#ifdef USING_HEIMDAL
    Ticket enctkt[1];
    krb5_ticket ticket[1];
    krb5_enc_data ke[1];
#else
    krb5_ticket *ticket = 0;
#endif
    int vno = 0;
    size_t len;
    static char nullstr[1];

    memset(creds, 0, sizeof *creds);
    memset(ktentry, 0, sizeof *ktentry);
    memset(data, 0, sizeof *data);
#ifdef USING_HEIMDAL
    memset(enctkt, 0, sizeof *enctkt);
    memset(ticket, 0, sizeof *ticket);
#endif
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

    if (!*names) {
	what = "krb5_kt_start_seq_get";
	code = krb5_kt_start_seq_get(k5context, keytab, &cursor);
	if (code) goto Failed;
	what = "krb5_kt_next_entry";
	while (!(code = krb5_kt_next_entry(k5context, keytab, ktentry, &cursor))) {
	    what = "while saving principal";
	    code = save_princ(ktentry-> HM(keyblock.keytype,
                            key.enctype),
			ktentry->vno,
			ktentry->principal, &list);
	    if (code) goto Skip;
	    krb5_free_keytab_entry_contents(k5context, ktentry);
	    memset(ktentry, 0, sizeof *ktentry);
	}
    Skip:
	if (code == KRB5_KT_END)
	    code = 0;
	if (!code)
	    what = "krb5_kt_end_seq_get";
	r = krb5_kt_end_seq_get(k5context, keytab, &cursor);
	if (!code && r)
	{
	    code = r;
	}
	if (code) goto Failed;
    }
    else while ((name = *names++)) {
	what = "krb5_parse_name";
	code = krb5_parse_name(k5context, name, &princ);
	if (code) goto Failed;
	what = "while saving principal";
	code = save_princ(0, 0, princ, &list);
	if (code) goto Failed;
    }
    for (p = list; p; p = p->next) {
	failed = 0;
	what = "krb5_unparse_name";
	code = krb5_unparse_name(k5context, p->principal, &princ_name);
	if (code) goto Failed;

	if (vflag) {
	    what = "kt_get_entry";
	    code = krb5_kt_get_entry(k5context, keytab, p->principal, 0,
		p->enctype, ktentry);
	    if (code) goto Failed;
	    printf ("About to use keytab entry:\n");
	    if ((code = show_kt_entry(ktentry)))
		goto Success;
	    krb5_free_keytab_entry_contents(k5context, ktentry);
	    memset(ktentry, 0, sizeof *ktentry);
	}

	krb5_get_init_creds_opt_init(gic_opts);
	if (p->enctype)
	    krb5_get_init_creds_opt_set_etype_list(gic_opts, &p->enctype, 1);
	what = "krb5_get_init_creds_keytab";
	code = krb5_get_init_creds_keytab(k5context, creds,
	    p->principal, keytab, 0, princ_name, gic_opts);
	if (code) goto Failed;
	if (vflag) {
	    printf ("session key:\n");
	    show_keyblock(&creds-> HM(session, keyblock));
	}
	if (p->enctype && creds-> HM(session.keytype, keyblock.enctype)
		!= p->enctype) {
	    fprintf (stderr, "*** kdc returns wrong keytype: wanted %d got %d\n",
		p->enctype, creds-> HM(session.keytype,keyblock.enctype));
	    failed = 1;
	}
	if (!krb5_principal_compare(k5context,
		    p->principal, creds->client)
		|| !krb5_principal_compare(k5context,
		    p->principal, creds->server)) {
	    char *p1 = 0, *p2 = 0;
	    (void) krb5_unparse_name(k5context, creds->client, &p1);
	    (void) krb5_unparse_name(k5context, creds->server, &p2);
	    if (!p1) p1 = nullstr;
	    if (!p2) p2 = nullstr;
	    if (code) goto Failed;
	    fprintf (stderr, "*** kdc returns wrong princ: requested=%s; ret'd client=%s server=%s\n",
		princ_name, p1, p2);
	    if (p1 != nullstr) free(p1);
	    if (p2 != nullstr) free(p2);
	    failed = 1;
	}
	if (vflag) {
	    printf ("Ticket w/ wrapper in credentials cache:\n");
	    if (pflag & 1)
		    bin_dump(creds->ticket.data, creds->ticket.length, 0);
	    if (pflag & 2)
		    pem_dump(creds->ticket.data, creds->ticket.length, 0);
	}
#ifdef USING_HEIMDAL
	what = "decode_Ticket";
	code = decode_Ticket(creds->ticket.data, creds->ticket.length, enctkt, &len);
	if (code) goto Failed;
	what = "checking enctkt";
	if (enctkt->tkt_vno != 5) {
	    code = KRB5KDC_ERR_BAD_PVNO;
	    goto Failed;
	}
	if (!enctkt->enc_part.kvno) {
	    code = KRB5KRB_AP_ERR_TKT_INVALID;
	    goto Failed;
	}
	what = "_krb5_principalname2krb5_principal";
	code = _krb5_principalname2krb5_principal(&ticket->server,
	    enctkt->sname,
	    enctkt->realm);
	if (code) goto Failed;
#else
	what = "krb5_decode_ticket";
	code = krb5_decode_ticket(&creds->ticket, &ticket);
	if (code) goto Bad;
#endif
	if (!krb5_principal_compare(k5context,
		p->principal, ticket->server)) {
	    char *p1 = 0;
	    (void) krb5_unparse_name(k5context, ticket->server, &p1);
	    fprintf (stderr, "*** ticket wrapper has wrong server: requested=%s; server=%s\n",
		princ_name, p1);
	    if (!p1) p1 = nullstr;
	    if (p1 != nullstr) free(p1);
	    failed = 1;
	}
	what = "kt_get_entry";
	code = krb5_kt_get_entry(k5context, keytab, p->principal, vno,
	    HM(enctkt->enc_part.etype, ticket->enc_part.enctype),
	    ktentry);
	if (code) goto Failed;
	data->data = malloc(data->length =
	    HM(enctkt->enc_part.cipher.length,
		ticket->enc_part.ciphertext.length));
	if (vflag) {
	    printf ("Decrypt Keytab entry:\n");
	    if ((code = show_kt_entry(ktentry)))
		goto Success;
	}
#ifdef USING_HEIMDAL
	ke->enctype = enctkt->enc_part.etype;
	ke->kvno = *enctkt->enc_part.kvno;
	ke->ciphertext.data = enctkt->enc_part.cipher.data;
	ke->ciphertext.length = enctkt->enc_part.cipher.length;
#endif
	/* XXX weird -- 2nd parm is not a pointer? */
	what = "krb5_c_decrypt";
	code = krb5_c_decrypt(k5context,
		HM(ktentry->keyblock,&ktentry->key),
		HM(KRB5_KU_TICKET,KRB5_KEYUSAGE_KDC_REP_TICKET),
		0,
		HM(ke,&ticket->enc_part), data);
	if (code) goto Bad;
	if (vflag) {
		printf ("Decrypted encrypted part of ticket:\n");
		if (!pflag) pflag = 1;
		if (pflag & 1)
			bin_dump(data->data, data->length, 0);
		if (pflag & 2)
			pem_dump(data->data, data->length, 0);
	}
#ifdef USING_HEIMDAL
	what = "krb5_decrypt_ticket";
	code = krb5_decrypt_ticket(k5context,
	    enctkt,
	    &ktentry->keyblock,
	    &ticket->ticket, 0);
	if (code) goto Failed;
	what = "_krb5_principalname2krb5_principal";
	code = _krb5_principalname2krb5_principal(&ticket->client,
	    ticket->ticket.cname,
	    ticket->ticket.crealm);
	if (code) goto Failed;
#else
	what = "decode_krb5_enc_tkt_part";
	code = decode_krb5_enc_tkt_part(data, &ticket->enc_part2);
	if (code) goto Bad;
#endif
	free(client_name); client_name = 0;
	what = "krb5_unparse_name";
	code = krb5_unparse_name(k5context,
		ticket-> HM(client,enc_part2->client),
		&client_name);
	if (code) goto Bad;
	if (!krb5_principal_compare(k5context,
		p->principal, ticket-> HM(client,enc_part2->client))) {
	    fprintf (stderr,
"*** ticket encpart has wrong client: requested=%s; client=%s\n",
		princ_name, client_name);
	    failed = 1;
	}
	if (!krb5i_keyblock_compare(k5context,
	    &creds-> HM(session, keyblock),
	    HM(&ticket->ticket.key, ticket->enc_part2->session))) {
	    fprintf (stderr, "*** ticket encpart has different session key\n");
	    if (!vflag) {
		printf ("creds. session key:\n");
		show_keyblock(&creds-> HM(session, keyblock));
	    }
	    printf ("encrypted ticket had session key:\n");
	    show_keyblock(HM(&ticket->ticket.key, ticket->enc_part2->session));
	    failed = 1;
	}
#ifdef USING_HEIMDAL
	printf ("%s; princ=<%s> vno=%d req.etype=%d ans.etype=%d ses.etype=%d ticket.length = %d\n",
	    failed ? "Failed" : "Success",
	    client_name, 
	    *enctkt->enc_part.kvno,
	    p->enctype,
	    enctkt->enc_part.etype,
	    creds->session.keytype,
	    enctkt->enc_part.cipher.length);
#else
	printf ("%s; princ=<%s> vno=%d req.etype=%d ans.etype=%d ses.etype=%d ticket.length = %d\n",
	    failed ? "Failed" : "Success",
	    client_name, 
	    ticket->enc_part.kvno,
	    p->enctype,
	    ticket->enc_part.enctype,
	    creds->keyblock.enctype,
	    ticket->enc_part.ciphertext.length);
#endif
	goto Next;
Bad:
	fprintf(stderr,"Failed in %s - error %d (%s)\n",
	    what, code, error_message(code));
	if (!vflag) {
	    printf ("Failing keytab entry:\n");
	    if ((code = show_kt_entry(ktentry)))
		goto Success;
	}
Next:
	krb5_free_keytab_entry_contents(k5context, ktentry);
	memset(ktentry, 0, sizeof *ktentry);
#ifdef USING_HEIMDAL
	if (enctkt->realm) free_Ticket(enctkt);
	memset(enctkt, 0, sizeof *enctkt);
	if (ticket->ticket.key.keyvalue.data)
	    free_EncTicketPart(&ticket->ticket);
	krb5_free_principal(k5context,ticket->server);
	krb5_free_principal(k5context,ticket->client);
	memset(ticket, 0, sizeof *ticket);
#else
	krb5_free_ticket(k5context, ticket);
	free(data->data);
	data->data = 0;
	ticket = 0;
#endif
	krb5_free_cred_contents(k5context, creds);
	memset(creds, 0, sizeof *creds);
	free(princ_name);
	princ_name = 0;
    }
    if (!code) goto Success;
Failed:
    fprintf(stderr,"Failed in %s - error %d (%s)\n",
	what, code, error_message(code));
Success:
    if (data->data) free(data->data);
#ifdef USING_HEIMDAL
    if (enctkt->realm) free_Ticket(enctkt);
    if (ticket->ticket.key.keyvalue.data)
	free_EncTicketPart(&ticket->ticket);
    krb5_free_principal(k5context,ticket->server);
    krb5_free_principal(k5context,ticket->client);
#else
    MO(if (ticket)) krb5_free_ticket(k5context, ticket);
#endif
    krb5_free_cred_contents(k5context, creds);
    krb5_free_keytab_entry_contents(k5context, ktentry);
    if (princ_name) free(princ_name);
    if (client_name) free(client_name);
    krb5_free_principal(k5context,princ);
    if (keytab) krb5_kt_close(k5context, keytab);
    while (p = list) {
	list = p->next;
	krb5_free_principal(k5context, p->principal);
	free(p);
    }
    return failed | !!code;
}

int
main(int argc, char **argv)
{
    char *argp;
    char *names[30];
    char *keytab_fn = 0;
    int ni = 0;

    while (--argc > 0) if (*(argp = *++argv)=='-')
    while (*++argp) switch(*argp) {
    case 'v':
	++vflag;
	break;
    case 'p':
	pflag |= 2;
	break;
    case 'b':
	pflag |= 1;
	break;
    case 'k':
	if (argc <= 1) goto Usage;
	--argc;
	if (keytab_fn) {
	    fprintf(stderr,"Cannot specify more than one keytab\n");
	    goto Usage;
	}
	keytab_fn = *++argv;
	break;
    case '-':
	break;
    default:
	fprintf (stderr,"Bad switch char <%c>\n", *argp);
    Usage:
	fprintf(stderr, "Usage: verifykt [-bpv] -k keytab [principals ...]\n");
	exit(1);
    }
    else names[ni++] = argp;

    if (!keytab_fn) {
	fprintf(stderr,"Need -k keytab\n");
	goto Usage;
    }

    if (pflag & 2)
	printf ("hint for pem text (except keys):\nopenssl asn1parse -i -dump\n");
    names[ni] = 0;
    verify_keytab(keytab_fn, names);
    if (k5context) {
	krb5_free_context(k5context);
	k5context = 0;
    }
    exit(exitcode);
}
