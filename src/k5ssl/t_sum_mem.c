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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "krb5.h"

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

void
fprinthex(fd, label, buf, n)
FILE *fd;
unsigned char *buf;
char *label;
{
    fprintf (fd, "%s=", label);
    while (n-- > 0)
    {
	putc("0123456789ABCDEF"[(*buf)>>4], fd);
	putc("0123456789ABCDEF"[(*buf)&15], fd);
	++buf;
    }
    putc('\n', fd);
}

int
stripnl(line)
char *line;
{
    char *cp;
    if ((cp = strchr(line, '\n')))
	*cp = 0;
    if ((cp = strchr(line, '\r')))
	*cp = 0;
    return 0;
}

/* memory representation of AES sum input */

struct aes_sum_table_struct
{
char* v; 
}; 

static const struct aes_sum_table_struct aes_sum_table[] =
{
 {"===="}, {"sha1/aes128 test vectors"}, {"===="}, {""}, {"KEYTYPE=17"}, {"T=15"}, {"KEY=294048AD714CD2A6C136F81E26B9B6F3"}, {""}, {"I=SHA1-AES128.2"}, {"DATA="}, {"SUM=60207B4B75803E09B76A3AA8"}, {""}, {"I=SHA1-AES128.3"}, {"DATA=00"}, {"SUM=1A742B164D59C19279780965"}, {""}, {"I=SHA1-AES128.58"}, {"DATA=0102"}, {"SUM=6C0FAB19D6477D7937836FE4"}, {""}, {"I=SHA1-AES128.59"}, {"DATA=7465737430313233343536373839"}, {"SUM=9D4BFF3F2E763E6372361C95"}, {"===="}, {"sha1/aes256 test vectors"}, {"===="}, {""}, {"KEYTYPE=18"}, {"T=16"}, {"USAGE=99"}, {"KEY=1BCE668186A0148AB8ABAEAAF023EB2D6946ADDED6E2C6A905C23DC49E70A787"}, {""}, {"I=SHA1-AES256.2"}, {"DATA="}, {"SUM=0DC80472E36BE62CA3FB3015"}, {""}, {"I=SHA1-AES256.3"}, {"DATA=00"}, {"SUM=02E17B0224944D83169A61AD"}, {""}, {"I=SHA1-AES256.58"}, {"DATA=0102"}, {"SUM=3C0E917A497BDA53F8B3EC71"}, {""}, {"I=SHA1-AES256.59"}, {"DATA=7465737430313233343536373839"}, {"SUM=7F98E391CFDC732150C46740"},
};

#define aes_sum_table_len() sizeof(aes_sum_table) / sizeof(struct aes_sum_table_struct)



/* memory representation of 3DES sum input */

struct des3_sum_table_struct
{
char* v; 
}; 

static const struct des3_sum_table_struct des3_sum_table[] =
{
 {"===="}, {"	sha1 test vectors"}, {"===="}, {""}, {"T=12"}, {"KEYTYPE=16"}, {"KEY=3D25CB29E3618FEAFDE938C88A137F6DF15401EF68F72A73"}, {""}, {"I=HMAC-SHA1.1"}, {"DATA="}, {"SUM=0306B727E7EED3517339A3609C7FBFC2D27A25B6"}, {""}, {"I=HMAC-SHA1.2"}, {"DATA=61"}, {"SUM=F4291FAAE5B0E3CB1B04F0798B690EF42F240379"}, {""}, {"I=HMAC-SHA1.3"}, {"DATA=616263"}, {"SUM=655F03DF8D61E9855A692D91A746B1BBA090BF26"}, {""}, {"I=HMAC-SHA1.4"}, {"DATA=6D65737361676520646967657374"}, {"SUM=FCCFF71B7349BA453E3C7A9C657F25FDB2C9D908"}, {""}, {"I=HMAC-SHA1.5"}, {"DATA=6162636465666768696A6B6C6D6E6F707172737475767778797A"}, {"SUM=D7F1E2BE3A25EFC7504F4C8BDADD69DDECE97CE0"}, {""}, {"I=HMAC-SHA1.6"}, {"DATA=4142434445464748494A4B4C4D4E4F505152535455565758595A6162636465666768696A6B6C6D6E6F707172737475767778797A30313233343536373839"}, {"SUM=71EAEC7C0D3BC6D97966371DB9B9D7EDFDD66F9C"}, {""}, {"I=HMAC-SHA1.7"}, {"DATA=3132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930"}, {"SUM=030C3FBDFE61B8F818878CDC771E9CFE393F6530"}, {""},
};

#define des3_sum_table_len() sizeof(des3_sum_table) / sizeof(struct des3_sum_table_struct)

enum sum_tables_enum { SUM_AES, SUM_DES3 };

long get_sum_table_len(enum sum_tables_enum which_t) {
  long len = -1;
  switch(which_t) {
  case SUM_AES:
    len = aes_sum_table_len();
    break;
  case SUM_DES3:
    len = des3_sum_table_len();
    break;    
  }
  return len;
}

char* get_sum_table_line(enum sum_tables_enum which_t, long ix) {
  char **line;
  switch(which_t) {
  case SUM_AES:
    line = &(aes_sum_table[ix].v);
    break;
  case SUM_DES3:
    line = &(des3_sum_table[ix].v);
    break;    
  }
  return *line;
}

void
t_sum_process(enum sum_tables_enum which_t)
{
    char *line;
    char label[512];
    struct test_struct ts[1];
    int keysize;
    int n;
    int ix;
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
    int cksumsize;
    krb5_checksum cksum[1];
    krb5_keyblock *key, kb[1];
    krb5_crypto crypto = 0;
    char *err;

    if (!k5context && (code = krb5_init_context(&k5context)))
      {
	fprintf(stderr,"krb5_init_context failed - %d\n", code);
	exit(1);
      }

    digestlen = 0;
    comment = 0;
    cktype = 1;	/* CKSUMTYPE_CRC32 */
    cksumsize = 4;
    keysize=0;
    key = 0;
    flag = 0;

    sumcount = 0;
    errors = 0;
    did_any_matches = 0;

    memset(ts, 0, sizeof ts);
    memset(kb, 0, sizeof *kb);

    long st_len = get_sum_table_len(which_t);
     for(ix = 0; ix < st_len; ++ix) 
    {
        line = get_sum_table_line(which_t, ix);

	if (*line == '#') continue;
	if (*line == '=')
	{
#if 0
	    comment = !comment;
#endif
	    continue;
	}
	if (comment) continue;
	if (!strncmp(line, "KEYTYPE=", 8))
	{
	    flag |= F_GOTKEYTYPE;
	    kb->keytype = atoi(line+8);
	    key = kb;
	}
	if (!strncmp(line, "KEY=", 4))
	{
	    n = readhex(line+4, ts->key, sizeof ts->key);
	    ts->keylen=n;
	    flag |= F_GOTKEY;

	    kb->keyvalue.data = ts->key;
	    kb->keyvalue.length = n;
	    key = kb;
	}
	if (!strncmp(line, "T=", 2))
	{
	    cktype = atoi(line+2);
	    code = krb5_checksumsize(k5context, cktype, &cksumsize);
	    if (code)
	    {
		fprintf (stderr,
			 "*** checksumtype %d not recognized - error %d\n",
			 cktype, code);
		exit(1);
	    }
	}
	if (!strncmp(line, "I=", 2))
	{
	    if ((flag & (F_GOT_SUM|F_GOT_DATA)) == F_GOT_DATA) {
		printf("\n%s\n", line);
		fprinthex(stdout,"DATA",ts->data,ts->datalen);
		fprinthex(stdout,"SUM",digest,digestlen);
	    }
	    flag &= ~(F_GOT_SUM|F_GOT_DATA);
	    flag |= F_NEWIDENT;
	    strcpy(label, line+2);
	}
	else if (!strncmp(line, "DATA=", 5))
	{
	    if (key && !key->keyvalue.data)
	    {
	        code = krb5_generate_random_keyblock(k5context,key->keytype,kb);
		if (code) {
		    fprintf(stderr,"krb5_c_make_random_key failed - code %d\n", code);
		    exit(1);
		}
		memcpy(ts->key, kb->keyvalue.data, ts->keylen = kb->keyvalue.length);
		fprinthex(stdout,"KEY",ts->key,ts->keylen);
		flag |= F_GOTKEY;
	    }
	    n = readhex(line+5, ts->data, sizeof ts->data);
	    ts->datalen=n;
	    if (vflag) fprinthex(stdout,"DATA",ts->data,ts->datalen);
	    memset(cksum, 0, sizeof *cksum);
#if 0
	    printf ("MAKE checksum\n");
#endif
	    
	    if (key)
	      code = krb5_crypto_init(k5context,
				      key,
				      key->keytype,
				      &crypto);
	    else crypto = 0, code = 0;
	    if (code)
	      {
		err = krb5_get_error_string(k5context); if (!err) err = "-";
		fprintf(stderr,"Error iniitializing crypto: %d %s\n",
			code, err);
		exit(1);
	      }
	    code = krb5_create_checksum(k5context,
					crypto,
					0,
					cktype,
					ts->data,
					ts->datalen,
					cksum);
			if (crypto) krb5_crypto_destroy(k5context, crypto);
			if (code)
			  {
			    fprintf(stderr,"Error making checksum: %d\n",
				    code);
			    exit(1);
			}
			if (cksum->checksum.length != cksumsize)
			{
				fprintf(stderr,
"Actual checksum length %d did not match expected %d\n",
					cksum->checksum.length, cksumsize);
exitrc = 1;
++errors;
continue;
			}
			memcpy(digest, cksum->checksum.data,
			       digestlen = cksum->checksum.length);
			free(cksum->checksum.data);

	    ++sumcount;
	    flag |= F_GOT_DATA;
	}
	else if (!strncmp(line, "SUM=", 4))
	{
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
	cksum->cksumtype = cktype;

	memcpy((cksum->checksum.data = malloc(cksum->checksum.length = ts->sumlen)),
	       ts->sum, ts->sumlen);
#if 0
	printf ("VERIFY checksum\n");
#endif
	if (key)
	  code = krb5_crypto_init(k5context,
				      key,
				  key->keytype,
				  &crypto);
	else code = 0, crypto = 0;
	if (!code) code = krb5_verify_checksum(k5context,
					       crypto,
					       0,
					       ts->data,
					       ts->datalen,
					       cksum);
	if (crypto) krb5_crypto_destroy(k5context, crypto);
	if (code)
	{
	    ++errors;
	    exitrc = 1;
	    fflush(stdout);
	    fprintf(stderr,"%s: can't verify input checksum", label);
	    if (code) 
		fprintf(stderr," code=%d\n", code);
	    else fprintf(stderr," not valid\n");
	    fprinthex(stdout,"DATA",ts->data,ts->datalen);
	    fprinthex(stdout,"SUM",ts->sum,ts->sumlen);
	}
	free(cksum->checksum.data);
	if (digestlen != ts->sumlen
	    || memcmp(ts->sum, digest, ts->sumlen))
	{
	    if (digestlen == ts->sumlen && key)
	    {
#if 0
		printf ("COMPARE failed, so verify checksum\n");
#endif

		cksum->cksumtype = cktype;
		cksum->checksum.data = digest;
		cksum->checksum.length = digestlen;
		if (key)
		  code = krb5_crypto_init(k5context,
					  key,
					  key->keytype,
					  &crypto);
		else crypto = 0, code = 0;
		if (!code) code = krb5_verify_checksum(k5context,
						       crypto,						       
						       0,
						       ts->data,
						       ts->datalen,
						       cksum);
		if (code)
		{
		    ++errors;
		    exitrc = 1;
		    fflush(stdout);
		    fprintf(stderr,"%s: can't verify computed checksum", label);
		    if (code) 
			fprintf(stderr," code=%d\n", code);
		    else fprintf(stderr," not valid\n");
		    fprinthex(stdout,"DATA",ts->data,ts->datalen);
		    fprinthex(stdout,"SUM",cksum->checksum.data,cksum->checksum.length);
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

}

int
main(argc, argv)
int argc;
char **argv;
{
    char *argp;
    char *progname = argv[0];

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
		fprintf(stderr,"Usage: %s\n", progname);
		exit(1);
	};

    t_sum_process(SUM_AES);

    printf ("Did %d sums for AES", sumcount);
    if (did_any_matches)
	printf ("; %d errors detected", errors);
    printf (".\n");


    t_sum_process(SUM_DES3);

    printf ("Did %d sums for DES3", sumcount);
    if (did_any_matches)
	printf ("; %d errors detected", errors);
    printf (".\n");

    exit(exitrc);
}
