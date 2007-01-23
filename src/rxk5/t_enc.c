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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_CK_H
#include <sys/types.h>
#include "k5ssl.h"
#else
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include "krb5.h"
#endif

char **cipher_name();
int ecount, dcount;
int exitrc;
int did_any_matches;
int errors;
int lied;
int vflag;
int Dflag;
int Fflag;
krb5_context k5context;

#define MAXPT 512
struct test_struct
{
	int enctype;
	int keylen;
	unsigned char key[MAXPT];
	unsigned char ivec[MAXPT];
	int iveclen;
	int ptlen;
	int ctlen;
	unsigned char pt[MAXPT];
	unsigned char ct[MAXPT];
	unsigned char ivec2[MAXPT];
	int usage;
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

int
fprinthex(fd, label, buf, n)
	FILE *fd;
	char *label;
	unsigned char *buf;
{
	fprintf (fd, "%s=", label);
	while (n-- > 0)
	{
		putc("0123456789ABCDEF"[(*buf)>>4], fd);
		putc("0123456789ABCDEF"[(*buf)&15], fd);
		++buf;
	}
	putc('\n', fd);
	return 0;
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

int
find_last_zero(cp, n)
	unsigned char *cp;
	unsigned int n;
{
	while (n && !cp[--n])
		;
	return n;
}

unsigned char zeros[MAXPT];

void
process(fn)
	char *fn;
{
	FILE *fd;
	char line[512];
	char label[512], newlabel[512];
	struct test_struct ts[1];
	int enctype, usage;
	int oldenctype;
	int n;
	int kf;
	unsigned char pt[MAXPT], ct[MAXPT], pt_from_ct[MAXPT];
	int ptlen, ctlen, pt_from_ctlen;
	int code;
	int flags;
#define F_GOT_KEY 1
#define F_GOT_PLAINTEXT 2
#define F_GOT_CIPHERTEXT 4
#define F_NEWIDENT 8
#define F_NEWKEY 0x10
#define F_NEWENCTYPE 0x20
#define F_DID_ENCRYPT 0x40
#define F_DID_DECRYPT 0x80
#define F_DID_ENCRYPT_LAST 0x100
#define F_DID_DECRYPT_LAST 0x200
/* 0x400 */
#define F_GOTIV 0x800
#define F_NEWIV 0x1000
#define F_GOT_PLAINTEXT_FIRST 0x2000
#define F_PRINT 0x4000
#define F_EOF 0x8000
#define F_NOTHING 2<<20

#define F_GOT_EVERYTHING (F_GOT_KEY|F_GOT_PLAINTEXT)
#define F_DID_EVERYTHING (F_GOT_EVERYTHING|F_DID_ENCRYPT|F_DID_DECRYPT)
	int comment;
	krb5_data ivecdata[1], *ivec, plaindata[1];
	unsigned char ivectemp[512];
#ifdef USING_HEIMDAL
	krb5_data encdata[1];
#else /* not Heimdal */
	size_t enclen;
	krb5_enc_data encdata[1];
#endif /* not Heimdal */
	int i, j;
	krb5_keyblock key[1];
#ifdef USING_HEIMDAL
	krb5_crypto crypto;
	char *err, *why;
#endif /* Heimdal */

	ptlen = ctlen = 0;
	if (fn)
	{
		fd = fopen(fn, "r");
	} else clearerr(fd = stdin);
	flags = 0;
	if (!fd)
	{
		perror(fn);
		exitrc = 1;
		return;
	}
#ifdef USING_HEIMDAL
/* need this to make heimdal work right.
 * otherwise errors cause core dumps.
 */
#if 0
/* with mit kerberos 5, it's simplier to
 * initialize the random number generator to avoid this:
assertion "inited" failed: file "../../../krb5/src/lib/crypto/prng.c", line 100
 */
#endif

	if (!k5context && (code = krb5_init_context(&k5context)))
	{
		fprintf(stderr,"krb5_init_context failed - %d\n", code);
		exit(1);
	}
#else
	if ((code = krb5_c_random_os_entropy(k5context,0,NULL)))
	{
		fprintf(stderr,"krb5_c_random_os_entropy failed - %d\n",code);
		exit(1);
	}
	/* code = krb5_c_random_add_entropy(k5context,
		KRB5_C_RANDSOURCE_TIMING, &seed);
	*/
#endif /* not Heimdal */
	comment = 0;
	i = 0;
	oldenctype = 0;
	enctype = usage = 0;
	memset(ts, 0, sizeof ts);
	memset(key, 0, sizeof *key);
	kf = 0;
	*label = *newlabel = 0;
	while (kf != F_EOF)
	{
		if (fgets(line, sizeof line, fd))
		{
			if (*newlabel)
			{
				strcpy(label, newlabel);
				*newlabel = 0;
			}
			stripnl(line);
if (vflag) {
fprintf(stdout,"# %#x <%s>\n", flags, line);
}
			if (*line == '#') continue;
			if (*line == '=')
			{
				comment = !comment;
				continue;
			}
			if (comment == 1) continue;
			if (!strncmp(line, "USAGE=", 6))
			{
				usage = atoi(line+6);
				continue;
			}
			if (!strncmp(line, "ENCTYPE=", 8))
			{
				enctype = atoi(line+8);
				flags = F_NEWENCTYPE;
				continue;
			}

			kf = 0;
			if (!strncmp(line, "IV=", 3))
				kf = F_GOTIV;
			else if (!strncmp(line, "KEY=", 4))
				kf = F_GOT_KEY;
			else if (!strncmp(line, "PT=", 3))
				kf = F_GOT_PLAINTEXT;
			else if (!strncmp(line, "CT=", 3))
				kf = F_GOT_CIPHERTEXT;
			else if (!strncmp(line, "I=", 2))
			{
				flags &= ~(F_PRINT
					|F_DID_ENCRYPT|F_DID_ENCRYPT_LAST
					|F_DID_DECRYPT|F_DID_DECRYPT_LAST);
				flags |= F_NEWIDENT;
				strcpy(newlabel, line+2);
				kf = F_NOTHING;
			}
			else if (!*line)
				kf = F_NOTHING;
			else
			{
				if (!comment)
				fprintf (stderr,"Line <%s> is not understood\n",
					line);
				continue;
			}
		} else kf = F_EOF;

		comment = 0;

		if (kf & (flags|F_EOF|F_NOTHING))
		{
			switch (flags & (F_DID_EVERYTHING|F_PRINT))
			{
			case F_GOT_KEY|F_GOT_PLAINTEXT|F_DID_ENCRYPT:
			case F_GOT_KEY|F_GOT_CIPHERTEXT|F_DID_DECRYPT:
				flags |= F_PRINT;
if (ts->enctype != oldenctype)
fprintf(stdout,"ENCTYPE=%d\n",ts->enctype);
oldenctype = ts->enctype;
printf("\nI=%s\n", label);
fprinthex(stdout,"KEY",ts->key,ts->keylen);
fprintf(stdout,"USAGE=%d\n",ts->usage);
fprinthex(stdout,"IV",ts->ivec,ts->iveclen);
if (flags & F_GOT_PLAINTEXT_FIRST)
{
fprinthex(stdout, "PT", ts->pt, ts->ptlen);
fprinthex(stdout, "CT", ct, ctlen);
} else {
fprinthex(stdout, "CT", ts->ct, ts->ctlen);
fprinthex(stdout, "PT", pt, ts->ptlen);
}
			}
		}
#if 0
printf ("# enctype=%#x kf=%#x flags=%#x\n", enctype, kf, flags);
if (!enctype) printf ("# !enctype\n");
if (!(kf & (F_GOT_PLAINTEXT|F_GOT_CIPHERTEXT))) printf ("# !need key\n");
if ((flags & F_GOT_KEY)) printf ("# have key\n");
#endif
		if (enctype
			&& (kf & (F_GOT_PLAINTEXT|F_GOT_CIPHERTEXT))
			&& !(flags & F_GOT_KEY))
		{
			krb5_keyblock kb[1];
			memset(kb, 0, sizeof *kb);
#ifdef USING_HEIMDAL
			code = krb5_generate_random_keyblock(k5context,enctype,kb);
#else /* not Heimdal */
			code = krb5_c_make_random_key(k5context,enctype,kb);
#endif /* not Heimdal */
			if (code)
			{
				fflush(stdout);
#ifdef USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
#endif /* Heimdal */
				fprintf(stderr,
#ifdef USING_HEIMDAL
"krb5_generate_random_keyblock failed - code %d %s\n",
					code, err);
#else /* not Heimdal */
"krb5_c_make_random_key failed - code %d\n",
					code);
#endif /* not Heimdal */
				exit(1);
			}
#ifdef USING_HEIMDAL
			memcpy(ts->key, kb->keyvalue.data, ts->keylen = kb->keyvalue.length);
#else /* not Heimdal */
			memcpy(ts->key, kb->contents, ts->keylen = kb->length);
#endif /* not Heimdal */
			ts->usage = usage;
			ts->enctype = enctype;
#ifdef USING_HEIMDAL
			key->keytype = ts->enctype;
			key->keyvalue.data = ts->key;
			key->keyvalue.length = ts->keylen;
#else /* not Heimdal */
			key->enctype = ts->enctype;
			key->contents = ts->key;
			key->length = ts->keylen;
#endif /* not Heimdal */
fprinthex(stdout,"KEY",ts->key,ts->keylen);
			flags |= F_GOT_KEY;
		}
		switch(kf)
		{
		case F_GOTIV:
		case F_GOT_KEY:
			flags &= ~(F_DID_ENCRYPT|F_DID_DECRYPT);
			switch(kf)
			{
			case F_GOT_KEY:
				flags |= F_NEWKEY;
				n = readhex(line+4, ts->key, sizeof ts->key);
				ts->keylen = n;
				ts->usage = usage;
				ts->enctype = enctype;
				flags |= F_GOT_KEY;
if (vflag) {
fprintf(stdout,"USAGE=%d\n",ts->usage);
fprintf(stdout,"ENCTYPE=%d\n",ts->enctype);
fprinthex(stdout,"KEY",ts->key,ts->keylen);
}
				break;
			case F_GOTIV:
				flags |= F_NEWIV;
				n = readhex(line+3, ts->ivec, sizeof ts->ivec);
				ts->iveclen = n;
				flags |= F_GOTIV;
if (vflag) {
fprinthex(stdout,"IV",ts->ivec,ts->iveclen);
}
				break;
			}
			flags &= ~(F_GOT_CIPHERTEXT|F_GOT_PLAINTEXT);
			flags &= ~(F_DID_ENCRYPT|F_DID_DECRYPT);
#ifdef USING_HEIMDAL
			key->keytype = ts->enctype;
			key->keyvalue.data = ts->key;
			key->keyvalue.length = ts->keylen;
#else /* not Heimdal */
			key->enctype = ts->enctype;
			key->contents = ts->key;
			key->length = ts->keylen;
#endif /* not Heimdal */
			break;
		case F_NOTHING:
			flags &= ~(F_GOT_CIPHERTEXT|F_GOT_PLAINTEXT);
			flags &= ~(F_DID_ENCRYPT|F_DID_DECRYPT);
			break;
		case F_GOT_PLAINTEXT:
			flags &= ~(F_DID_ENCRYPT);
			flags |= F_GOT_PLAINTEXT;
			n = readhex(line+3, ts->pt, sizeof ts->pt);
			if ((flags & F_GOT_CIPHERTEXT) && ts->ctlen != n)
				flags &= ~(F_GOT_CIPHERTEXT|F_DID_DECRYPT);
			ts->ptlen = n;
if (vflag) fprinthex(stdout,"PT",ts->pt,ts->ptlen);
			if (flags & F_GOT_CIPHERTEXT)
				flags &= ~F_GOT_PLAINTEXT_FIRST;
			else
				flags |= F_GOT_PLAINTEXT_FIRST;
			break;
		case F_GOT_CIPHERTEXT:
			flags &= ~(F_DID_DECRYPT);
			flags |= F_GOT_CIPHERTEXT;
			n = readhex(line+3, ts->ct, sizeof ts->ct);
			if ((flags & F_GOT_PLAINTEXT) && ts->ptlen != n)
				flags &= ~(F_GOT_PLAINTEXT|F_DID_ENCRYPT);
			ts->ctlen = n;
if (vflag) fprinthex(stdout,"CT",ts->ct,ts->ctlen);
			break;
		}
		for (j = 0; j < 2; ++j)
		switch(j ^ !(flags & F_GOT_PLAINTEXT_FIRST))
		{
		case 0:
			if ((flags & (F_GOT_KEY|F_GOT_PLAINTEXT|F_DID_ENCRYPT))
				!= (F_GOT_KEY|F_GOT_PLAINTEXT))
				continue;
			if ((flags & (F_GOT_PLAINTEXT_FIRST|F_GOT_CIPHERTEXT)) ==
				F_GOT_CIPHERTEXT)
			{	/* lie, we can't decrypt this anyways */
				++lied;
				flags |= F_DID_ENCRYPT;
				continue;
			}
			++ecount;
			if (ts->iveclen)
			{
				memcpy(ivecdata->data = ivectemp,
					ts->ivec,
					ivecdata->length = ts->iveclen);
				ivec = ivecdata;
			} else ivec = 0;
			memset(ct, 0, sizeof ct);
#ifndef USING_HEIMDAL
			memset(plaindata, 0, sizeof *plaindata);
			plaindata->data = ts->pt;
			plaindata->length = ts->ptlen;
#endif /* not Heimdal */
			memset(encdata, 0, sizeof *encdata);
#ifdef USING_HEIMDAL
			crypto = 0;
			why = "krb5_crypto_init";
			code = krb5_crypto_init(k5context, key, key->keytype, &crypto);
			if (code) crypto = 0; else
			(why = "krb5_encrypt_ivec"),
			(code = krb5_encrypt_ivec(k5context, crypto, ts->usage,
				ts->pt, ts->ptlen, encdata, ivec ? ivec->data : 0));
			if (crypto) krb5_crypto_destroy(k5context, crypto);
			crypto = 0;
#else /* not Heimdal */
			code = krb5_c_encrypt_length(k5context, key->enctype, plaindata->length, &enclen);
if (code) {{
fflush(stdout);
fprintf(stderr,"<%s> krb5_c_encrypt_length failed - %d\n", label, code);
}}
			encdata->enctype = enctype;
			encdata->ciphertext.data = ct;
			encdata->ciphertext.length = sizeof ct;
			if (Fflag)
				encdata->ciphertext.length = enclen;
			code = krb5_c_encrypt(k5context, key, ts->usage, ivec, plaindata, encdata);
#endif /* not Heimdal */
if (code) {{
char label[40];
fflush(stdout);
#ifdef USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
#endif /* Heimdal */
fprintf (stderr,"        %d usage=%d enctype=%d\n", i, usage, enctype);
sprintf (label,"     key%d",i); fprinthex(stderr,label,ts->key,ts->keylen);
sprintf (label,"    ivec%d",i); fprinthex(stderr,label,ts->ivec,ts->iveclen);
sprintf (label,"   plain%d",i); fprinthex(stderr,label,ts->pt,ts->ptlen);
}
#ifdef USING_HEIMDAL
fprintf(stderr,"<%s> krb5_encrypt_ivec failed - %d %s; %s\n", label, code, err, why);
#else /* not Heimdal */
fprintf(stderr,"<%s> krb5_c_encrypt failed - %d\n", label, code);
#endif /* not Heimdal */
++errors;
exitrc = 1;
continue;
}
#ifdef USING_HEIMDAL
			memcpy(ct, encdata->data, ctlen = encdata->length);
			free(encdata->data);
			encdata->data = 0;
#else /* not Heimdal */
			ctlen = encdata->ciphertext.length;
if (ctlen != enclen) {
fprintf(stderr, "<%s> for plain=%d bytes; krb5_c_encrypt_length returned %d bytes, but krb5_c_encrypt actually returned %d bytes\n",
label, plaindata->length, (int) enclen, ctlen);
++errors;
exitrc = 1;
}
			if (memcmp(ct+ctlen, zeros, sizeof ct-ctlen))
{
fprintf(stderr,"%s: encrypt left junk after ciphertext\n", label);
fprinthex(stderr,"junk",ct+ctlen, find_last_zero(ct+ctlen, sizeof ct-ctlen));
/* exitrc = 1; */
}
#endif /* not Heimdal */
if (Dflag)
{
char label[40];
fprintf (stdout,"        %d usage=%d enctype=%d\n", i, usage, enctype);
sprintf (label,"     key%d",i); fprinthex(stdout,label,ts->key,ts->keylen);
sprintf (label,"    ivec%d",i); fprinthex(stdout,label,ts->ivec,ts->iveclen);
sprintf (label,"   plain%d",i); fprinthex(stdout,label,ts->pt,ts->ptlen);
sprintf (label,"      to%d",i); fprinthex(stdout,label,ct,ctlen);
}
if (vflag) fprinthex(stdout,"computedCT",ct,ctlen);
			memset(pt_from_ct, 0, sizeof pt_from_ct);
			++dcount;
#ifndef USING_HEIMDAL
			memset(encdata, 0, sizeof *encdata);
			encdata->enctype = enctype;
			encdata->ciphertext.data = ct;
			encdata->ciphertext.length = ctlen;
#endif /* not Heimdal */
			memset(plaindata, 0, sizeof *plaindata);
#ifndef USING_HEIMDAL
			plaindata->data = pt_from_ct;
			plaindata->length = sizeof pt_from_ct;
			if (Fflag)
			{
				plaindata->length = ctlen;
			}
#if 0
{char label[40]; sprintf (label,"B)cipher%d",i); fprinthex(stderr,label,ct,ctlen); }
#endif
#endif /* not Heimdal */
			if (ts->iveclen)
			{
				memcpy(ivecdata->data = ivectemp,
					ts->ivec,
					ivecdata->length = ts->iveclen);
				ivec = ivecdata;
			} else ivec = 0;
#ifdef USING_HEIMDAL
			crypto = 0;
			why = "krb5_crypto_init";
			code = krb5_crypto_init(k5context, key, key->keytype, &crypto);
			if (code) crypto = 0; else
			(why = "krb5_decrypt_ivec"),
			(code = krb5_decrypt_ivec(k5context, crypto, ts->usage,
				ct, ctlen,
				plaindata, ivec ? ivec->data : 0));
			if (crypto) krb5_crypto_destroy(k5context, crypto);
			crypto = 0;
#else /* not Heimdal */
			code = krb5_c_decrypt(0, key, ts->usage, ivec, encdata, plaindata);
#endif /* not Heimdal */
if (code) {{
char label[40];
fflush(stdout);
#ifdef USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
#endif /* Heimdal */
fprintf (stderr,"        %d usage=%d enctype=%d\n", i, usage, enctype);
sprintf (label,"     key%d",i); fprinthex(stderr,label,ts->key,ts->keylen);
sprintf (label,"    ivec%d",i); fprinthex(stderr,label,ts->ivec,ts->iveclen);
}
#ifdef USING_HEIMDAL
sprintf (label,"  cipher%d",i); fprinthex(stderr,label,ct,ts->ctlen);
fprintf(stderr,"%s: krb5_decrypt_ivec (after encrypt) failed - %d %s; %s\n", label, code, err, why);
#else /* not Heimdal */
sprintf (label,"  cipher%d",i); fprinthex(stderr,label,ct,ctlen);
fprintf(stderr,"%s: krb5_c_decrypt (after encrypt) failed - %d\n", label, code);
#endif /* not Heimdal */
++errors;
exitrc = 1;
continue;
}
#ifdef USING_HEIMDAL
			memcpy(pt_from_ct, plaindata->data, pt_from_ctlen = plaindata->length);
			free(plaindata->data);
			plaindata->data = 0;
#else /* not Heimdal */
			pt_from_ctlen = plaindata->length;
			if (memcmp(pt_from_ct+pt_from_ctlen,
				zeros, sizeof pt_from_ct-pt_from_ctlen))
{
fprintf(stderr,"%s: decrypt (after encrypt) left junk after plaintext\n", label);
fprinthex(stderr,"junk",pt_from_ct+pt_from_ctlen, find_last_zero(pt_from_ct+pt_from_ctlen, sizeof pt_from_ct-pt_from_ctlen));
/* exitrc = 1; */
}
#endif /* not Heimdal */
if (Dflag)
{
char label[40];
fprintf (stdout,"        %d usage=%d enctype=%d\n", i, usage, enctype);
sprintf (label,"     key%d",i); fprinthex(stdout,label,ts->key,ts->keylen);
sprintf (label,"    ivec%d",i); fprinthex(stdout,label,ts->ivec,ts->iveclen);
sprintf (label,"  cipher%d",i); fprinthex(stdout,label,ct,ctlen);
sprintf (label,"   plain%d",i); fprinthex(stdout,label,pt_from_ct,pt_from_ctlen);
}
if (vflag) fprinthex(stdout,"computedPTfromcomputedCT",pt_from_ct,pt_from_ctlen);
			did_any_matches = 1;
			flags |= F_DID_ENCRYPT;
			if (pt_from_ctlen >= ts->ptlen
					&& !memcmp(pt_from_ct, ts->pt,
						ts->ptlen)
					&& !memcmp(pt_from_ct+ts->ptlen,
						zeros,
						pt_from_ctlen - ts->ptlen))
				;
else {
fflush(stdout);
fprintf(stderr,"%s: decrypt of encrypt results did not match expected\n", label);
fprintf(stderr,"ENCTYPE=%d\n",ts->enctype);
fprinthex(stderr,"KEY",ts->key,ts->keylen);
fprinthex(stderr,"IV",(flags & F_DID_ENCRYPT_LAST) ? ts->ivec2 : ts->ivec,ts->iveclen);
fprintf(stderr,"USAGE=%d\n",ts->usage);
fprinthex(stderr,"origPT",ts->pt,ts->ptlen);
fprinthex(stderr,"CT",ct,ctlen);
fprinthex(stderr,"finalPT",pt_from_ct,pt_from_ctlen);
exitrc = 1;
++errors;
}
			continue;
		case 1:
			if ((flags & (F_GOT_KEY|F_GOT_CIPHERTEXT|F_DID_DECRYPT))
				!= (F_GOT_KEY|F_GOT_CIPHERTEXT))
				continue;
			memset(pt, 0, sizeof pt);
			++dcount;
#ifndef USING_HEIMDAL
			memset(encdata, 0, sizeof *encdata);
			encdata->enctype = enctype;
			encdata->ciphertext.data = ts->ct;
			encdata->ciphertext.length = ts->ctlen;
#endif /* not Heimdal */
			memset(plaindata, 0, sizeof *plaindata);
#ifndef USING_HEIMDAL
			plaindata->data = pt;
			plaindata->length = sizeof pt;
#endif /* not Heimdal */
			if (ts->iveclen)
			{
				memcpy(ivecdata->data = ivectemp,
					ts->ivec,
					ivecdata->length = ts->iveclen);
				ivec = ivecdata;
			} else ivec = 0;
#ifdef USING_HEIMDAL
			crypto = 0;
			why = "krb5_crypto_init";
			code = krb5_crypto_init(k5context, key, key->keytype, &crypto);
			if (code) crypto = 0; else
			(why = "krb5_decrypt_ivec"),
			(code = krb5_decrypt_ivec(k5context, crypto, ts->usage,
				ts->ct, ts->ctlen,
				plaindata, ivec ? ivec->data : 0));
			if (crypto) krb5_crypto_destroy(k5context, crypto);
			crypto = 0;
#else /* not Heimdal */
			code = krb5_c_decrypt(0, key, ts->usage, ivec, encdata, plaindata);
#endif /* not Heimdal */
if (code) {{
char label[40];
fflush(stdout);
#ifdef USING_HEIMDAL
err = krb5_get_error_string(k5context); if (!err) err = "-";
#endif /* Heimdal */
fprintf (stderr,"        %d usage=%d enctype=%d\n", i, usage, enctype);
sprintf (label,"     key%d",i); fprinthex(stderr,label,ts->key,ts->keylen);
sprintf (label,"    ivec%d",i); fprinthex(stderr,label,ts->ivec,ts->iveclen);
sprintf (label,"  cipher%d",i); fprinthex(stderr,label,ts->ct,ts->ctlen);
}
#ifdef USING_HEIMDAL
fprintf(stderr,"%s: krb5_decrypt_ivec failed - %d %s; %s\n", label, code, err, why);
#else /* not Heimdal */
fprintf(stderr,"%s: krb5_c_decrypt failed - %d\n", label, code);
#endif /* not Heimdal */
++errors;
exitrc = 1;
continue;
}
#ifdef USING_HEIMDAL
			memcpy(pt, plaindata->data, ptlen = plaindata->length);
			free(plaindata->data);
			plaindata->data = 0;
#else /* not Heimdal */
			ptlen = plaindata->length;
			if (memcmp(pt+ptlen, zeros, sizeof pt-ptlen))
{
fprintf(stderr,"%s: decrypt left junk after plaintext\n", label);
fprinthex(stderr,"junk",pt+ptlen, find_last_zero(pt+ptlen, sizeof pt-ptlen));
/* exitrc = 1; */
}
#endif /* not Heimdal */
if (Dflag)
{
char label[40];
fprintf (stdout,"        %d usage=%d enctype=%d\n", i, usage, enctype);
sprintf (label,"     key%d",i); fprinthex(stdout,label,ts->key,ts->keylen);
sprintf (label,"ivec%d",i); fprinthex(stdout,label,ts->ivec,ts->iveclen);
sprintf (label,"from%d",i); fprinthex(stdout,label,ts->ct,ts->ctlen);
sprintf (label,"  to%d",i); fprinthex(stdout,label,pt,ptlen);
}
if (vflag) fprinthex(stdout,"computedPT",pt,ptlen);
			flags |= F_DID_DECRYPT;
			continue;
		}
		if ((flags & F_DID_EVERYTHING) == F_DID_EVERYTHING)
		{
			did_any_matches = 1;
			for (j = 0; j < 2; ++j)
			switch(j ^ !(flags & F_GOT_PLAINTEXT_FIRST))
			{
			case 0:
				if (!(flags & F_GOT_PLAINTEXT_FIRST))
					continue;
#if 0
				if (!memcmp(ct, ts->ct, ts->ctlen))
					continue;
{
fflush(stdout);
fprintf(stderr,"%s: encrypt results did not match expected\n", label);
fprintf(stderr,"ENCTYPE=%d\n",ts->enctype);
fprinthex(stderr,"KEY",ts->key,ts->keylen);
fprinthex(stderr,"IV",(flags & F_DID_ENCRYPT_LAST) ? ts->ivec2 : ts->ivec,ts->iveclen);
fprintf(stderr,"USAGE=%d\n",ts->usage);
fprinthex(stderr,"PT",ts->pt,ts->ptlen);
fprinthex(stderr,"CT",ct,ctlen);
fprinthex(stderr,"expectedCT",ts->ct,ts->ctlen);
exitrc = 1;
++errors;
}
#else
				if (ctlen == ts->ctlen) continue;
{
fflush(stdout);
fprintf(stderr,"%s: encrypt results did not match expected\n", label);
fprintf(stderr,"# expected length(CT) = %d\n", ts->ctlen);
fprintf(stderr,"# actual length(CT) = %d\n", ctlen);
exitrc = 1;
++errors;
}
#endif
				break;
			case 1:
				did_any_matches = 1;
				if (ptlen > ts->ptlen
					&& !memcmp(pt, ts->pt, ts->ptlen)
					&& !memcmp(pt+ptlen,
						zeros,
						ptlen - ts->ptlen))
					continue;
{
fflush(stdout);
fprintf(stderr,"%s: decrypt results did not match expected\n", label);
fprintf(stderr,"ENCTYPE=%d\n",ts->enctype);
fprinthex(stderr,"KEY",ts->key,ts->keylen);
fprinthex(stderr,"IV",(flags & F_DID_DECRYPT_LAST) ? ts->ivec2 : ts->ivec,ts->iveclen);
fprintf(stderr,"USAGE=%d\n",ts->usage);
fprinthex(stderr,"CT",ts->ct,ts->ctlen);
fprinthex(stderr,"PT",pt,ptlen);
fprinthex(stderr,"expectedPT",ts->pt,ts->ptlen);
exitrc = 1;
++errors;
}
				break;
			}
			flags &= ~(F_DID_DECRYPT|F_DID_ENCRYPT);
		}
	}
	if (fd != stdin)
		fclose(fd);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *argp, *fn;
	char *progname = argv[0];
	FILE *out;

	fn = 0;
	while (--argc > 0) if (*(argp = *++argv) == '-')
	while (*++argp) switch(*argp) {
#if 0
	case 'd': case 'e':
		mode = *argp;
		break;
#endif
	case 'F':
		++Fflag;
		break;
	case 'v':
		++vflag; break;
	case 'D':
		++Dflag;
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

	if (!fn)
		process(fn);
	out = stdout;
	if (!did_any_matches) out = stderr;
	
	fprintf (out, "Did %d/%d e/d's", ecount, dcount);
	if (did_any_matches || errors)
		fprintf (out, "; %d errors detected", errors);
	if (lied)
	{
		fprintf (out, "; (decrypt-only because not invertible)");
	}
	fprintf (out, ".\n");
	exit(exitrc);
}
