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

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#include "krb5.h"
#else
#include "k5ssl.h"
#endif

struct entry {
    struct entry *next;
    char *principal;
    int kvno, enctype, keybytes;
    unsigned char key[1];
};

krb5_context k5context;

int
write_keytab(char *fn, struct entry *entries)
{
    krb5_keytab kt;
    krb5_keytab_entry e[1];
    char *what;
    int code;
    struct entry *p;

    memset(e, 0, sizeof *e);
    what = "krb5_init_context";
    code = krb5_init_context(&k5context);
    if (code) goto Failed;

    what = "krb5_ktfile_wresolve";
    code = krb5_ktfile_wresolve(k5context, fn, &kt);
    for (p = entries; p; p = p->next) {
	what = "krb5_parse_name";
	code = krb5_parse_name(k5context, p->principal, &e->principal);
	if (code) goto Failed;
	e->vno = p->kvno;
	e->key.enctype = p->enctype;
	e->key.length = p->keybytes;
	if (code) goto Failed;
	e->key.contents = (void *) p->key;
	what = "krb5_kt_add_entry";
	code = krb5_kt_add_entry(k5context, kt, e);
	if (code) goto Failed;
	krb5_free_principal(k5context, e->principal);
	e->principal = 0;
    }
    what = "krb5_kt_close";
    code = krb5_kt_close(k5context, kt);
    if (code) goto Failed;
    return 0;
Failed:
    afs_com_err("t_wrkt", code, "Failed on %s\n", what);
    return code;
}

int
readhex(line, buf, n)
unsigned char *line, *buf;
{
    int i;
    int j;
    unsigned char *cp;
    static char foo[256];

    cp = line;
    if (!foo['1'])
    {
	for (i = 0; i < 256; ++i)
	    foo[i] = 0x7f;
	for (i = 0; i < 10; ++i)
	    foo[i+'0'] = i;
	for (i = 0; i < 6; ++i) {
	    foo[i+'a'] = i+10;
	    foo[i+'A'] = i+10;
	}
    }
    i = 0;
    while (foo[*cp] != 0x7f && foo[1[cp]] != 0x7f)
    {
	j = (foo[*cp]<<4) + foo[1[cp]];
	buf[i] = j;
	cp += 2;
	++i;
	if (i >= n) break;
    }
    return i;
}

int
main(int argc, char **argv)
{
    int f, code;
    char *argp;
    char *pflag = 0, *tflag = 0;
    struct entry *entries, *p, **pp = &entries;
    char *ep;
    int vno, len;
    krb5_enctype enctype = 0;

    f = 0;
    while (--argc > 0) if (*(argp = *++argv)=='-')
    while (*++argp) switch(*argp) {
    case 'p':	/* service principal */
	if (argc <= 1) {
	    fprintf(stderr,"-p: missing argument\n");
	    goto Usage;
	}
	--argc;
	pflag = *++argv;
	break;
    case 'v':	/* vno */
	if (argc <= 1) {
	    fprintf(stderr,"-v: missing argument\n");
	    goto Usage;
	}
	--argc;
	vno = strtol(*++argv, &ep, 0);
	if (ep == *argv) {
	    fprintf(stderr,"-v: bad number: <%s>\n", *argv);
	    exit(1);
	}
	break;
    case 't':	/* keytab */
	if (argc <= 1) {
	    fprintf(stderr,"-t: missing argument\n");
	    goto Usage;
	}
	if (tflag) {
		fprintf (stderr,"Can't specify keytab more than once\n");
	    goto Usage;
	}
	--argc;
	tflag = *++argv;
	break;
    case '-':
	break;
    default:
	fprintf (stderr,"Bad switch char <%c>\n", *argp);
    Usage:
	fprintf(stderr,
"Usage: t_wrkt [-t keytab] [[-p principal] [-v kvno] enctype hexkeybytes ...]\n");
	exit(1);
    }
    else if (enctype) {
	if (!pflag) {
	    fprintf (stderr,"Must specify principal with -p at least once\n");
	    goto Usage;
	}
	len = strlen(argp);
	p = malloc(sizeof *p + strlen(pflag) + (len>>1));
	if (!p) {
	    afs_com_err("t_wrkt", ENOMEM, "while processing arguments\n");
	    exit(1);
	}
	memset(p, 0, sizeof *p);
	p->enctype = enctype;
	p->keybytes = readhex(argp, p->key, len);
	p->kvno = vno;
	if (p->keybytes != len>>1) {
	    fprintf(stderr,"Non-hex digits in: <%s>\n", argp);
	    goto Usage;
	}
	p->principal = (void *) (p->key + p->keybytes);
	strcpy(p->principal, pflag);
	*pp = p;
	pp = &p->next;
    }
    else {
	enctype = strtol(argp, &ep, 0);
	if (ep == *argv) {
	    code = krb5_string_to_enctype(*argv, &enctype);
	    if (code) {
		fprintf(stderr,"-v: bad enctype: <%s>\n", argp);
		goto Usage;
	    }
	}
    }

    if (!entries) {
	fprintf (stderr,"Must specify at least one key\n");
	goto Usage;
    }

    code = write_keytab(tflag, entries);
    if (k5context) {
	krb5_free_context(k5context);
	k5context = 0;
    }
    while (p = entries) {
	entries = p->next;
	free(p);
    }

    exit(!!code);
}
