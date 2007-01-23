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
#define afs_osi_Alloc(n)    malloc(n)
#define afs_osi_Free(p,n)   free(p)
#define afs_strcasecmp(p,q) strcasecmp(p,q)
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

int s2kcount;
int exitrc;
int did_any_matches;
int errors;
int vflag;
krb5_context k5context;

struct test_struct
{
    int saltlen;
    unsigned char salt[256];
    int paramlen;
    unsigned char param[256];
    int stringlen;
    unsigned char string[256];
    int keylen;
    unsigned char key[256];
    int keytype;
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
#define F_GOT_STRING 1
#define F_GOT_SALT 2
#define F_GOT_PARAM 4
#define F_GOT_KEY 8
#define F_NEWIDENT 16
#define F_GOTKEYTYPE 32
    int comment;
    int code;
    int cktype;
    krb5_data input[1], params[1];
#if defined(USING_HEIMDAL) || defined(USE_SSL)
    krb5_salt salt[1];
#else
    krb5_data salt[1];
#endif /* not Heimdal */
    krb5_keyblock computed_key[1], actual_key[1];
#ifdef USING_HEIMDAL
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
#ifdef USING_HEIMDAL
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
    comment = 0;
    keysize=0;
    memset(ts, 0, sizeof ts);
    memset(actual_key, 0, sizeof *actual_key);
    while (fgets(line, sizeof line, fd)) {
	stripnl(line);
	if (*line == '#') continue;
	if (*line == '=') {
	    comment = !comment;
	    continue;
	}
	if (comment || !*line) continue;
	if (!strncmp(line, "KEYTYPE=", 8)) {
	    flag &= ~(F_GOT_PARAM|F_GOT_SALT);
	    flag |= F_GOTKEYTYPE;
	    ts->keytype = atoi(line+8);
	}
	else if (!strncmp(line, "KEY=", 4)) {
	    n = readhex(line+4, ts->key, sizeof ts->key);
	    ts->keylen=n;
	    flag |= F_GOT_KEY;
	    KEYTYPE(actual_key) = ts->keytype;
	    KEYDATA(actual_key) = ts->key;
	    KEYLENGTH(actual_key) = n;
	}
	else if (!strncmp(line, "I=", 2)) {
	    if ((flag & (F_GOT_KEY|F_GOT_STRING)) == F_GOT_STRING) {
printf("\nI=%s\n", label);
fprinthex(stdout,"STRING",ts->string,ts->stringlen);
fprinthex(stdout,"KEY",KEYDATA(computed_key),KEYLENGTH(computed_key));
	    }
	    flag &= ~(F_GOT_KEY|F_GOT_STRING);
	    flag |= F_NEWIDENT;
	    memcpy(label, line+2, strlen(line+2)+1);
	}
	else if (!strncmp(line, "STRING=", 7)) {
	    n = readhex(line+7, ts->string, sizeof ts->string);
	    ts->stringlen=n;
if (vflag) fprinthex(stdout,"STRING",ts->string,ts->stringlen);
	    input->length = ts->stringlen;
	    input->data = ts->string;
	    if (flag & F_GOT_PARAM) {
		params->length = ts->paramlen;
		params->data = ts->param;
	    }
	    memset(salt, 0, sizeof *salt);
#ifdef USE_SSL
	    salt->s2kdata.data = ts->salt;
	    salt->s2kdata.length = ts->saltlen;
	    if (flag & F_GOT_PARAM) {
		salt->s2kparams = *params;
	    }
#else
#ifdef USING_HEIMDAL
	    salt->salttype = KRB5_PW_SALT;
	    if (ts->keytype < 4U && ts->paramlen && ts->param[1] == 1) {
		salt->salttype = KRB5_AFS3_SALT;
	    }
	    salt->saltvalue.data = ts->salt;
	    salt->saltvalue.length = ts->saltlen;
#else
	    salt->data = ts->salt;
	    salt->length = ts->saltlen;
#endif
#endif
#if 0
printf ("MAKE computed key\n");
#endif
#ifdef USING_HEIMDAL
	    if (flag & F_GOT_PARAM)
		code = krb5_string_to_key_data_salt_opaque(
		    k5context,
		    ts->keytype,
		    *input,
		    *salt,
		    *params,
		    computed_key);
	    else
		code = krb5_string_to_key_data_salt(
		    k5context,
		    ts->keytype,
		    *input,
		    *salt,
		    computed_key);
#else /* not Heimdal */
#ifdef USE_SSL
	    code = krb5_c_string_to_key(k5context,
		ts->keytype,
		input,
		salt,
		computed_key);
#else
	    salt->magic = KV5M_DATA;
#if 0 || defined(ENCTYPE_CAST_CBC_SHA)
	    if ((flag & F_GOT_PARAM)
		    && (ts->keytype == ENCTYPE_CAST_CBC_SHA
			|| ts->keytype == ENCTYPE_RC6_CBC_SHA))
		switch((char)(*ts->param)) {
		case -3:
		    salt->magic = KV5M_GREX_SALT; break;
		case -2:
		    salt->magic = KV5M_CRYPT_SALT; break;
		case 0:
		    salt->magic = KV5M_DATA; break;
		}
	    else salt->magic = KV5M_DATA;
#endif
	    if (flag & F_GOT_PARAM)
		code = krb5_c_string_to_key_with_params(k5context,
		    ts->keytype,
		    input,
		    salt,
		    params,
		    computed_key);
	    else
		code = krb5_c_string_to_key(k5context,
		    ts->keytype,
		    input,
		    salt,
		    computed_key);
#endif
#endif /* not Heimdal */
	    ++s2kcount;
	    if (code) {
#ifdef USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
		fprintf(stderr,"%s: Error doing string2key: %d %s\n",
		    label, code, err);
#else /* not Heimdal */
		fprintf(stderr,"%s: Error doing string2key: %d\n",
		    label, code);
#endif /* not Heimdal */
		++errors;
		exitrc = 1;
	    } else {
		flag |= F_GOT_STRING;
	    }
	}
	else if (!strncmp(line, "SALT=", 5)) {
	    n = readhex(line+5, ts->salt, sizeof ts->salt);
	    ts->saltlen = n;
	    flag |= F_GOT_SALT;
	}
	else if (!strncmp(line, "PARAM=", 6)) {
	    n = readhex(line+6, ts->param, sizeof ts->param);
	    ts->paramlen = n;
	    flag |= F_GOT_PARAM;
	} else {
	    fprintf(stderr,"Don't understand: %s\n", line);
	    exitrc = 1;
	}
	if ((flag & (F_GOT_KEY|F_GOT_STRING)) != (F_GOT_KEY|F_GOT_STRING))
	    continue;
	++did_any_matches;
	if (KEYTYPE(computed_key) != KEYTYPE(actual_key)
		|| KEYLENGTH(computed_key) != KEYLENGTH(actual_key)
		|| memcmp(KEYDATA(computed_key), KEYDATA(actual_key),
		    KEYLENGTH(actual_key))) {
printf ("COMPARE failed\n");
	    ++errors;
	    exitrc = 1;
fflush(stdout);
fprintf(stderr,"%s: keys didn't agree\n", label);
fprinthex(stdout,"STRING",ts->string,ts->stringlen);
fprinthex(stdout,"SALT",ts->salt,ts->saltlen);
fprinthex(stdout,"PARAM",ts->param,ts->paramlen);
fprintf(stderr,"computedKey.type=%d actualKey.type=%d\n",
KEYTYPE(computed_key), KEYTYPE(actual_key));
fprinthex(stdout,"computedKEY",KEYDATA(computed_key),KEYLENGTH(computed_key));
fprinthex(stdout,"actualKEY",KEYDATA(actual_key),KEYLENGTH(actual_key));
	    flag &= ~(F_GOT_KEY|F_GOT_STRING);
	}
    }
    if ((flag & (F_GOT_KEY|F_GOT_STRING)) == F_GOT_STRING) {
printf("\nI=%s\n", label);
fprinthex(stdout,"STRING",ts->string,ts->stringlen);
fprinthex(stdout,"KEY",KEYDATA(computed_key),KEYLENGTH(computed_key));
    }
    if (fd != stdin)
	fclose(fd);
}

int
main(int argc, char**argv)
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
    printf ("Did %d s2k's", s2kcount);
    if (did_any_matches)
	printf ("; %d errors detected", errors);
    printf (".\n");
#ifdef USING_HEIMDAL
    if (k5context) krb5_free_context(k5context);
#endif
    exit(exitrc);
}
