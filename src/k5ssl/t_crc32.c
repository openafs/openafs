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

/*
 * test algorithm against known input.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef USING_SSL
#include <openssl/evp.h>
#else
#include "k5s_evp.h"
#endif
#include <fcntl.h>

extern const EVP_MD *EVP_crc32();

char **cipher_name();
int sumcount;
int exitrc;
int did_any_matches;
int errors;
int vflag;

struct test_struct
{
    int datalen;
    unsigned char data[256];
    int sumlen;
    unsigned char sum[256];
};

int
readhex(unsigned char *line, unsigned char *buf, int n)
{
    int i;
    int j;
    unsigned char *cp;
    static char foo[256];

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
    char line[512];
    char label[512];
    struct test_struct ts[1];
    int keysize;
    int n;
    int flag;
#define F_GOT_DATA 1
#define F_GOT_SUM 2
#define F_NEWIDENT 4
    int comment;
    EVP_MD_CTX ctx[1];
    int code;
    unsigned char digest[256];
    int digestlen;
    char *what;

    if (fn) {
	fd = fopen(fn, "r");
    } else clearerr(fd = stdin);
    flag = 0;
    if (!fd) {
	perror(fn);
	exitrc = 1;
	return;
    }
    comment = 0;
    keysize=0;
    memset(ts, 0, sizeof ts);
    while (fgets(line, sizeof line, fd)) {
	stripnl(line);
	if (*line == '=') {
	    comment = !comment;
	    continue;
	}
	if (comment) continue;
	if (!strncmp(line, "I=", 2)) {
	    if ((flag & (F_GOT_SUM|F_GOT_DATA)) == F_GOT_DATA) {
printf("\n%s\n", line);
fprinthex(stdout,"T",ts->data,ts->datalen);
fprinthex(stdout,"SUM",digest,digestlen);
	    }
	    flag = F_NEWIDENT;
	    memcpy(label, line+2, strlen(label)+1);
	}
	else if (!strncmp(line, "DATA=", 5)) {
	    n = readhex(line+5, ts->data, sizeof ts->data);
	    ts->datalen=n;
if (vflag) fprinthex(stdout,"T",ts->data,ts->datalen);
	    EVP_MD_CTX_init(ctx);
	    what = "EVP_DigestInit_ex";
#define ERRCHK(f)   if ((code = !f)) goto Out;
	    ERRCHK(EVP_DigestInit_ex(ctx, EVP_crc32(), NULL));
	    ERRCHK(EVP_DigestUpdate(ctx, "uysonue", 7));
	    ERRCHK(EVP_DigestUpdate(ctx, ts->data, ts->datalen));
	    ERRCHK(EVP_DigestFinal_ex(ctx, digest, &digestlen))
	Out:
	    if (code == 1) {
		fprintf(stderr,"While doing %s:\n", what);
		ERR_print_errors_fp(stderr);
		exit(1);
	    }
	    ++sumcount;
	    EVP_MD_CTX_cleanup(ctx);
	    {
		int i;
		int t;
		for (i = 0; i < digestlen; ++i)
		    digest[i] ^= 0xff;
		for (i = 0; i < digestlen/2; ++i) {
		    t = digest[i];
		    digest[i] = digest[digestlen + ~i];
		    digest[digestlen + ~i] = t;
		}
	    }
	    flag |= F_GOT_DATA;
	}
	else if (!strncmp(line, "SUM=", 4)) {
	    n = readhex(line+4, ts->sum, sizeof ts->sum);
	    if (n != 4)
{
fflush(stdout);
fprintf(stderr,"sum was %d not %d\n", n, 4);
exitrc = 1;
}
	    ts->sumlen = 4;
	    flag |= F_GOT_SUM;
	}
if ((flag & (F_GOT_SUM|F_GOT_DATA)) != (F_GOT_SUM|F_GOT_DATA)) continue;
	++did_any_matches;
	if (digestlen != ts->sumlen
		|| memcmp(ts->sum, digest, ts->sumlen)) {
	    exitrc = 1;
fflush(stdout);
fprintf(stderr,"%s: checksums didn't agree\n", label);
fprinthex(stdout,"T",ts->data,ts->datalen);
fprinthex(stdout,"SUM",ts->sum,ts->sumlen);
fprinthex(stdout,"computedSUM",digest,digestlen);
	}
	flag = 0;
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
	fprintf(stderr,"Usage: %s [file]\n", progname);
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
