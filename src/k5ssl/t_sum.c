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

#include "k5s_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#if !defined(TESTING)
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strcasecmp(p,q)	strcasecmp(p,q)
#endif
#if defined(USE_SSL) || defined(USE_FAKESSL)
#include "k5ssl.h"
#else
#include "krb5.h"
#endif

#ifdef USING_HEIMDAL
#define HM(h,m)	h
#else
#define HM(h,m)	m
#endif
#ifdef USING_HEIMDAL
#define KEYDATA(kb)	(kb)->keyvalue.data
#define KEYLENGTH(kb)	(kb)->keyvalue.length
#define KEYTYPE(kb)	(kb)->keytype
#else /* not Heimdal */
#define KEYDATA(kb)	(kb)->contents
#define KEYLENGTH(kb)	(kb)->length
#define KEYTYPE(kb)	(kb)->enctype
#endif /* not Heimdal */

int sumcount;
int exitrc;
int did_any_matches;
int errors;
int vflag;
krb5_context k5context;

struct test_struct
{
    int datalen;
    unsigned char data[256];
    int sumlen;
    unsigned char sum[256];
    int keylen;
    unsigned char key[256];
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
#define F_GOTKEYTYPE 8
#define F_GOTKEY 16
    int comment;
    int code;
    unsigned char digest[256];
    int digestlen;
    int cktype;
    unsigned cksumsize;
#if !USING_HEIMDAL
    krb5_data input[1];
#endif /* not Heimdal */
    krb5_checksum cksum[1];
    krb5_keyblock *key, kb[1];
#if USING_HEIMDAL
    krb5_crypto crypto = 0;
    char *err;
#else /* not Heimdal */
    krb5_boolean valid;
#endif /* not Heimdal */

    if (fn) {
	fd = fopen(fn, "r");
    } else clearerr(fd = stdin);
    flag = 0;
    if (!fd) {
	perror(fn);
	exitrc = 1;
	return;
    }
#if USING_HEIMDAL
/* need this to make heimdal work right. */
/* otherwise errors cause core dumps. */
/*
 * with mit kerberos 5, it's simpler to initialize the prng
 * directly to avoid this:
assertion "inited" failed: file "../../../krb5/src/lib/crypto/prng.c", line 100
 */

    if (!k5context && (code = krb5_init_context(&k5context))) {
	fprintf(stderr,"krb5_init_context failed - %d\n", code);
	exit(1);
    }
#else /* not Heimdal */
    if ((code = krb5_c_random_os_entropy(k5context,0,NULL))) {
	fprintf(stderr,"krb5_c_random_os_entropy failed - %d\n",code);
	exit(1);
    }
    /* code = krb5_c_random_add_entropy(k5context,
	KRB5_C_RANDSOURCE_TIMING, &seed);
    */
#endif /* not Heimdal */
    digestlen = 0;
    comment = 0;
    cktype = 1; /* CKSUMTYPE_CRC32 */
    cksumsize = 4;
    keysize=0;
    key = 0;
    memset(ts, 0, sizeof ts);
    memset(kb, 0, sizeof *kb);
    while (fgets(line, sizeof line, fd)) {
	stripnl(line);
	if (*line == '#') continue;
	if (*line == '=') {
	    comment = !comment;
	    continue;
	}
	if (comment) continue;
	if (!strncmp(line, "KEYTYPE=", 8)) {
	    flag |= F_GOTKEYTYPE;
	    KEYTYPE(kb) = atoi(line+8);
	    key = kb;
	}
	if (!strncmp(line, "KEY=", 4)) {
	    n = readhex(line+4, ts->key, sizeof ts->key);
	    ts->keylen=n;
	    flag |= F_GOTKEY;
	    KEYDATA(kb) = ts->key;
	    KEYLENGTH(kb) = n;
	    key = kb;
	}
	if (!strncmp(line, "T=", 2)) {
	    cktype = atoi(line+2);
#if USING_HEIMDAL
	    code = krb5_checksumsize(k5context, cktype, &cksumsize);
#else /* not Heimdal */
	    code = krb5_c_checksum_length(k5context, cktype, &cksumsize);
#endif /* not Heimdal */
	    if (code) {
#if USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
#endif /* Heimdal */
		fprintf (stderr,
#if USING_HEIMDAL
"%s: *** checksumtype %d not recognized - error %d %s\n",
		    label, cktype, code, err);
#else /* not Heimdal */
"%s: *** checksumtype %d not recognized - error %d\n",
		    label, cktype, code);
#endif /* not Heimdal */
		exitrc = 1;
		++errors;
		continue;
	    }
	}
	if (!strncmp(line, "I=", 2)) {
	    if ((flag & (F_GOT_SUM|F_GOT_DATA)) == F_GOT_DATA) {
printf("\n%s\n", line);
fprinthex(stdout,"DATA",ts->data,ts->datalen);
fprinthex(stdout,"SUM",digest,digestlen);
	    }
	    flag &= ~(F_GOT_SUM|F_GOT_DATA);
	    flag |= F_NEWIDENT;
	    strcpy(label, line+2);
	}
	else if (!strncmp(line, "DATA=", 5)) {
	    if (key && !KEYDATA(key)) {
#if USING_HEIMDAL
code = krb5_generate_random_keyblock(k5context,key->keytype,kb);
#else /* not Heimdal */
code = krb5_c_make_random_key(k5context,key->enctype,kb);
#endif /* not Heimdal */
    if (code) {
#if USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
    fprintf(stderr,"krb5_generate_random_keyblock failed - code %d %s\n", code, err);
#else /* not Heimdal */
    fprintf(stderr,"krb5_c_make_random_key failed - code %d\n", code);
#endif /* not Heimdal */
    exit(1);
}
memcpy(ts->key, KEYDATA(kb), ts->keylen = KEYLENGTH(kb));
fprinthex(stdout,"KEY",ts->key,ts->keylen);
flag |= F_GOTKEY;
	    }
	    n = readhex(line+5, ts->data, sizeof ts->data);
	    ts->datalen=n;
if (vflag) fprinthex(stdout,"DATA",ts->data,ts->datalen);
#if !USING_HEIMDAL
	    input->length = ts->datalen;
	    input->data = ts->data;
#endif /* not Heimdal */
	    memset(cksum, 0, sizeof *cksum);
#if 0
printf ("MAKE checksum\n");
#endif
#if USING_HEIMDAL
	    if (key)
	    code = krb5_crypto_init(k5context,
#else /* not Heimdal */
	    code = krb5_c_make_checksum(k5context,
		cktype,
#endif /* not Heimdal */
		key,
#if USING_HEIMDAL
		key->keytype,
		&crypto);
	    else crypto = 0, code = 0;
	    if (code) {
err = krb5_get_error_string(k5context); if (!err) err = "-";
		fprintf(stderr,"Error iniitializing crypto: %d %s\n",
		    code, err);
		exit(1);
	    }
	    code = krb5_create_checksum(k5context,
		crypto,
#endif /* Heimdal */
		0,
#if USING_HEIMDAL
		cktype,
		ts->data,
		ts->datalen,
#else /* not Heimdal */
		input,
#endif /* not Heimdal */
		cksum);
#if USING_HEIMDAL
	    if (crypto) krb5_crypto_destroy(k5context, crypto);
#endif /* Heimdal */
	    if (code) {
#if USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
		fprintf(stderr,"Error making checksum: %d %s\n",
		    code, err);
#else /* not Heimdal */
		fprintf(stderr,"Error making checksum: %d\n",
		    code);
#endif /* not Heimdal */
		exit(1);
	    }
#if USING_HEIMDAL
	    if (cksum->checksum.length != cksumsize)
#else /* not Heimdal */
	    if (cksum->length != cksumsize)
#endif /* not Heimdal */
	    {
		fprintf(stderr,
"Actual checksum length %d did not match expected %d\n",
#if USING_HEIMDAL
		    cksum->checksum.length, cksumsize);
#else /* not Heimdal */
		    cksum->length, cksumsize);
#endif /* not Heimdal */
exitrc = 1;
++errors;
continue;
	    }
#if USING_HEIMDAL
	    memcpy(digest, cksum->checksum.data,
		digestlen = cksum->checksum.length);
	    afs_osi_Free(cksum->checksum.data, cksum->checksum.length);
#else /* not Heimdal */
	    memcpy(digest, cksum->contents,
		digestlen = cksum->length);
	    afs_osi_Free(cksum->contents, cksum->length);
#endif /* not Heimdal */
	    ++sumcount;
	    flag |= F_GOT_DATA;
	}
	else if (!strncmp(line, "SUM=", 4)) {
	    n = readhex(line+4, ts->sum, sizeof ts->sum);
	    if (n != cksumsize)
{
fflush(stdout);
fprintf(stderr,"checksumlength was %d not %d\n", n, cksumsize);
	    exitrc = 1;
	    ++errors;
}
	    ts->sumlen = cksumsize;
	    flag |= F_GOT_SUM;
	}
if ((flag & (F_GOT_SUM|F_GOT_DATA)) != (F_GOT_SUM|F_GOT_DATA)) continue;
	++did_any_matches;
#if USING_HEIMDAL
	cksum->cksumtype = cktype;
#else /* not Heimdal */
	cksum->checksum_type = cktype;
#endif /* not Heimdal */
	
#if USING_HEIMDAL
	memcpy((cksum->checksum.data = afs_osi_Alloc(cksum->checksum.length = ts->sumlen)),
#else /* not Heimdal */
	memcpy((cksum->contents = afs_osi_Alloc(cksum->length = ts->sumlen)),
#endif /* not Heimdal */
	    ts->sum, ts->sumlen);
#if 0
printf ("VERIFY checksum\n");
#endif
#if USING_HEIMDAL
	if (key)
	code = krb5_crypto_init(k5context,
#else /* not Heimdal */
	code = krb5_c_verify_checksum(k5context,
#endif /* not Heimdal */
	    key,
#if USING_HEIMDAL
	    key->keytype,
	    &crypto);
	else code = 0, crypto = 0;
	if (!code) code = krb5_verify_checksum(k5context,
	    crypto,
#endif /* Heimdal */
	    0,
#if USING_HEIMDAL
	    ts->data,
	    ts->datalen,
	    cksum);
	if (crypto) krb5_crypto_destroy(k5context, crypto);
	if (code)
#else /* not Heimdal */
	    input,
	    cksum,
	    &valid);
	if (code || !valid)
#endif /* not Heimdal */
	{
	    ++errors;
	    exitrc = 1;
#if USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
#endif /* Heimdal */
fflush(stdout);
fprintf(stderr,"%s: can't verify input checksum", label);
#if USING_HEIMDAL
fprintf(stderr," code=%d %s\n", code, err);
#else /* not Heimdal */
if (code) 
fprintf(stderr," code=%d\n", code);
else fprintf(stderr," not valid\n");
#endif /* not Heimdal */
fprinthex(stdout,"DATA",ts->data,ts->datalen);
fprinthex(stdout,"SUM",ts->sum,ts->sumlen);
	}
#if USING_HEIMDAL
	afs_osi_Free(cksum->checksum.data, cksum->checksum.length);
#else /* not Heimdal */
	afs_osi_Free(cksum->contents, cksum->length);
#endif /* not Heimdal */
	if (digestlen != ts->sumlen
		    || memcmp(ts->sum, digest, ts->sumlen)) {
	    if (digestlen == ts->sumlen && key) {
#if 0
printf ("COMPARE failed, so verify checksum\n");
#endif
#if USING_HEIMDAL
		cksum->cksumtype = cktype;
		cksum->checksum.data = digest;
		cksum->checksum.length = digestlen;
		if (key)
		code = krb5_crypto_init(k5context,
#else /* not Heimdal */
		cksum->checksum_type = cktype;
		cksum->contents = digest;
		cksum->length = digestlen;
		code = krb5_c_verify_checksum(k5context,
#endif /* not Heimdal */
		    key,
#if USING_HEIMDAL
		    key->keytype,
		    &crypto);
		else crypto = 0, code = 0;
		if (!code) code = krb5_verify_checksum(k5context,
		    crypto,
#endif /* Heimdal */
		    0,
#if USING_HEIMDAL
		    ts->data,
		    ts->datalen,
		    cksum);
		if (code)
#else /* not Heimdal */
		    input,
		    cksum,
		    &valid);
		if (code || !valid)
#endif /* not Heimdal */
		{
		    ++errors;
		    exitrc = 1;
#if USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
#endif /* Heimdal */
fflush(stdout);
fprintf(stderr,"%s: can't verify computed checksum", label);
#if USING_HEIMDAL
fprintf(stderr," code=%d %s\n", code, err);
#else /* not Heimdal */
if (code) 
fprintf(stderr," code=%d\n", code);
else fprintf(stderr," not valid\n");
#endif /* not Heimdal */
fprinthex(stdout,"DATA",ts->data,ts->datalen);
#if USING_HEIMDAL
fprinthex(stdout,"SUM",cksum->checksum.data,cksum->checksum.length);
#else /* not Heimdal */
fprinthex(stdout,"SUM",cksum->contents,cksum->length);
#endif /* not Heimdal */
		}
	    } else {
printf ("COMPARE failed badly, so not verifying checksum\n");
		++errors;
		exitrc = 1;
fflush(stdout);
fprintf(stderr,"%s: checksums didn't agree\n", label);
fprinthex(stdout,"DATA",ts->data,ts->datalen);
fprinthex(stdout,"SUM",ts->sum,ts->sumlen);
fprinthex(stdout,"computedSUM",digest,digestlen);
	    }
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
#if 0
    Usage:
#endif
	fprintf(stderr,"Usage: %s [file]\n", progname);
	exit(1);
    } else process(fn = argp);

    if (!fn) process(fn);
    printf ("Did %d sums", sumcount);
    if (did_any_matches)
	printf ("; %d errors detected", errors);
    printf (".\n");
#if USING_HEIMDAL
    if (k5context) krb5_free_context(k5context);
#endif
    exit(exitrc);
}
