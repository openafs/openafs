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

/*
 * test algorithm against known input.
 */

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#ifdef USING_SSL
#include <openssl/evp.h>
#endif
#ifdef USING_FAKESSL
#include "k5s_evp.h"
#endif
#ifdef USING_K5SSL
#include "k5ssl.h"
#else
#include <krb5.h>
#ifdef USING_MIT
#define krb5i_timegm	timegm
#endif
#endif

#if 0
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#if USING_SSL
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif
#endif

extern int bin_dump();

int sumcount;
int exitrc;
int did_any_matches;
int errors;
int vflag;
krb5_context k5context;

struct test_struct
{
    unsigned char data[16384];
    int data_len;
};

int
readhex(unsigned char *line, unsigned char *buf, int n)
{
    int i;
    int j;
    unsigned char *cp;
    static char foo[16384];

    cp = line;
    if (!foo['1']) {
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
    while (foo[*cp] != 0x7f && foo[1[cp]] != 0x7f) {
	j = (foo[*cp]<<4) + foo[1[cp]];
	buf[i] = j;
	cp += 2;
	++i;
	if (i >= n) break;
    }
    return i;
}

void
fprinthex(FILE *fd, char *label, unsigned char *buf, int n)
{
    fprintf (fd, "%s=", label);
    while (n-- > 0) {
	putc("0123456789ABCDEF"[(*buf)>>4], fd);
	putc("0123456789ABCDEF"[(*buf)&15], fd);
	++buf;
    }
    putc('\n', fd);
}

int
stripnl(char *line)
{
    char *cp;
    if ((cp = strchr(line, '\n')))
	*cp = 0;
    if ((cp = strchr(line, '\r')))
	*cp = 0;
    return 0;
}

void
process(char *fn)
{
    FILE *fd;
    char line[32768];
    char label[32768];
    struct test_struct ts[1];
    int keysize;
    int n;
    int flag;
#define F_GOT_DATA 1
#define F_GOT_TICKET 2
#define F_NEWIDENT 4
#define F_GOTKEYTYPE 8
#define F_GOTKEY 16
    int comment;
    int code;
    krb5_principal princ;
#if USING_HEIMDAL
    char *err;
#endif

    char *p;
    int i;

    if (fn) {
	fd = fopen(fn, "r");
    } else clearerr(fd = stdin);
    flag = 0;
    if (!fd) {
	perror(fn);
	exitrc = 1;
	return;
    }
/* need context for local realm */
    if (!k5context && (code = krb5_init_context(&k5context))) {
	fprintf(stderr,"krb5_init_context failed - %d\n", code);
	exit(1);
    }
    comment = 0;
    keysize=0;
    memset(ts, 0, sizeof ts);
    while (fgets(line, sizeof line, fd)) {
	stripnl(line);
	if (*line == '=') {
#if 0
	    comment = !comment;
#endif
	    continue;
	}
	if (comment) continue;
	if (!strncmp(line, "I=", 2)) {
	    if ((flag & (F_GOT_TICKET|F_GOT_DATA)) == F_GOT_DATA) {
printf("\n%s\n", line);
fprinthex(stdout,"DATA",ts->data,ts->data_len);
	    }
	    flag &= ~(F_GOT_TICKET|F_GOT_DATA);
	    flag |= F_NEWIDENT;
	    strcpy(label, line+2);
	}
	else if (!strncmp(line, "DATA=", 5)) {
	    n = readhex(line+5, ts->data, sizeof ts->data);
	    ts->data_len=n;
if (vflag) fprinthex(stdout,"ENCPART",ts->data,ts->data_len);
	    code = krb5_parse_name(k5context, line+5, &princ);
	    if (code) {
#if USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
		fprintf(stderr,"Error from parse_name: %d %s\n",
		    code, err);
#else /* not Heimdal */
		fprintf(stderr,"Error from parse_name: %d\n",
		    code);
#endif /* not Heimdal */
#ifndef USING_MIT
#ifndef USING_HCRYPTO
		ERR_print_errors_fp(stderr);
#endif
#endif
		continue;
	    }
	    flag |= F_GOT_DATA;

	    printf ("\ninput: <%s>\n", line+5);
#if USING_HEIMDAL
	    printf ("nt=%d; length=%d\n",
		princ->name.name_type,
		princ->name.name_string.len);
#else
	    printf ("nt=%d; length=%d\n",
		princ->type,
		princ->length);
#endif
#if USING_HEIMDAL
	    for (i = 0; i < princ->name.name_string.len; ++i)
#else
	    for (i = 0; i < princ->length; ++i)
#endif
	    {
		printf ("[%d]\n", i);
#if USING_HEIMDAL
		bin_dump(princ->name.name_string.val[i],
		    strlen(princ->name.name_string.val[i]), 0);
#else
		bin_dump(princ->data[i].data, princ->data[i].length, 0);
#endif
	    }
	    printf ("realm:\n");
#if USING_HEIMDAL
	    bin_dump(princ->realm, strlen(princ->realm), 0);
#else
	    bin_dump(princ->realm.data, princ->realm.length, 0);
#endif

	    p = 0;
	    krb5_unparse_name(k5context, princ, &p);
	    printf ("client = <%s>\n", p);
	    if (p) free (p);
	    krb5_free_principal(k5context, princ);
	}
	++did_any_matches;
    }
    if (fd != stdin)
	fclose(fd);
}

int
main(int argc, char **argv)
{
    char *argp, *fn;
    char *progname = argv[0];

    fn = 0;
    while (--argc > 0) if (*(argp = *++argv) == '-')
    while (*++argp) switch(*argp) {
    case 'v':
	++vflag;
	break;
    case '-':
	break;
    default:
    Usage:
	fprintf(stderr,"Usage: %s [-vD] [file]\n", progname);
	exit(1);
    } else if (fn) goto Usage;
	else fn = argp;

    process(fn);
    printf ("Did %d sums", sumcount);
    if (did_any_matches)
	printf ("; %d errors detected", errors);
    printf (".\n");
    exit(exitrc);
}

#if 0
struct trytime {
    char *asc;
    time_t expected;
} timetrials[] = {
    "19700101000001Z", 1,
    "19700130024223Z", 2515343,
    "19720130125034Z", 65623834,
    "19730130053056Z", 97219856,
    "19720228071833Z", 68109513,
    "20050811180645Z", 1123783605,
};

try_time()
{
    struct tm tm[1], gm[1];
    time_t r, r2;
    int i;
    int success, failures;
    char *cp;
    char temp[50];
    success = failures = 0;
    for (i = 0; i < sizeof timetrials/sizeof *timetrials; ++i) {
	memset(tm, 0, sizeof *tm);
	cp = timetrials[i].asc;
	strcpy(temp, cp+0); temp[4] = 0; tm->tm_year = atoi(temp)-1900;
	strcpy(temp, cp+4); temp[2] = 0; tm->tm_mon = atoi(temp)-1;
	strcpy(temp, cp+6); temp[2] = 0; tm->tm_mday = atoi(temp);
	strcpy(temp, cp+8); temp[2] = 0; tm->tm_hour = atoi(temp);
	strcpy(temp, cp+10); temp[2] = 0; tm->tm_min = atoi(temp);
	strcpy(temp, cp+12); temp[2] = 0; tm->tm_sec = atoi(temp);
	r = krb5i_timegm(tm); *gm = *gmtime(&r); cp = asctime(gm);
	if (r == timetrials[i].expected) {
	    ++success;
	    /* continue; */
	} else {
	    printf ("*** %s -> %d, expected %d\n",
		timetrials[i].asc, r, timetrials[i].expected);
	    ++failures;
	}
	printf ("K %s -> %d (%.24s)\n", timetrials[i].asc, r, cp);
	r2 = timegm(tm); *gm = *gmtime(&r2); cp = asctime(gm);
	printf ("S %s -> %d (%.24s)\n", timetrials[i].asc, r2, cp);
    }
    printf ("%d successes, %d failures\n", success, failures);
}
#endif
