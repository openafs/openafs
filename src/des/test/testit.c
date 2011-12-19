/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * exit returns	 0 ==> success
 *		-1 ==> error
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <mit-cpyright.h>
#include <stdio.h>
#include <errno.h>
#include <des.h>


#define MIN_ARGC	0	/* min # args, not incl flags */
#define MAX_ARGC	2	/* max # args, not incl flags */

/* MIN_ARGC == MAX_ARGC ==> required # args */

extern char *errmsg();
extern void des_string_to_key();
extern int des_key_sched();
extern int des_ecb_encrypt();
extern int des_cbc_encrypt();
extern int des_pcbc_encrypt();

char *progname;
int sflag;
int vflag;
int tflag;
int nflag = 1000;
int cflag;
int des_debug;
des_key_schedule KS;
unsigned char cipher_text[64];
unsigned char clear_text[64] = "Now is the time for all ";
unsigned char clear_text2[64] = "7654321 Now is the time for ";
unsigned char clear_text3[64] = { 2, 0, 0, 0, 1, 0, 0, 0 };
unsigned char *input;

/* 0x0123456789abcdef */
des_cblock default_key = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };
des_cblock s_key;
des_cblock default_ivec = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef };
unsigned char *ivec;
des_cblock zero_key = { 1 };
int i, j;

main(argc, argv)
     int argc;
     char *argv[];
{
    /*  Local Declarations */

    long in_length;

    progname = argv[0];		/* salt away invoking program */

    while (--argc > 0 && (*++argv)[0] == '-')
	for (i = 1; argv[0][i] != '\0'; i++) {
	    switch (argv[0][i]) {

		/* debug flag */
	    case 'd':
		des_debug = 1;
		continue;

		/* verbose flag */
	    case 'v':
		vflag = 1;
		continue;

		/* cbc flag */
	    case 'c':
		cflag = 1;
		continue;

		/* string to key only flag */
	    case 's':
		sflag = 1;
		continue;

		/* test flag - use known key and cleartext */
	    case 't':
		tflag = 1;
		continue;

		/* iteration count */
	    case 'n':
		sscanf(&argv[0][i + 1], "%d", &nflag);
		argv[0][i + 1] = '\0';	/* force it to stop */
		break;

	    default:
		printf("%s: illegal flag \"%c\" ", progname, argv[0][i]);
		exit(1);
	    }
	};

    if (argc < MIN_ARGC || argc > MAX_ARGC) {
	printf("Usage: xxx [-xxx]  xxx xxx\n");
	exit(1);
    }

    /* argv[0] now points to first non-option arg, if any */

    if (tflag) {
	/* use known input and key */
	des_key_sched(default_key, KS);
	input = clear_text;
	ivec = (unsigned char *)default_ivec;
    } else {
	/*des_string_to_key(argv[0],s_key); */
	des_string_to_key("test", s_key);
	if (vflag) {
	    input = (unsigned char *)s_key;
	    fprintf(stdout, "\nstring = %s, key = ", argv[0]);
	    for (i = 0; i <= 7; i++)
		fprintf(stdout, "%02x ", *input++);
	}
	des_string_to_key("test", s_key);
	if (vflag) {
	    input = (unsigned char *)s_key;
	    fprintf(stdout, "\nstring = %s, key = ", argv[0]);
	    for (i = 0; i <= 7; i++)
		fprintf(stdout, "%02x ", *input++);
	}
	des_key_sched(s_key, KS);
	input = (unsigned char *)argv[1];
	ivec = (unsigned char *)argv[2];
    }


    if (cflag) {
	fprintf(stdout, "\nclear %s\n", input);
	in_length = strlen(input);
	des_cbc_encrypt(input, cipher_text, (long)in_length, KS, ivec, 1);
	fprintf(stdout, "\n\nencrypted ciphertext = (low to high bytes)");
	for (i = 0; i <= 7; i++) {
	    fprintf(stdout, "\n");
	    for (j = 0; j <= 7; j++)
		fprintf(stdout, "%02x ", cipher_text[i * 8 + j]);
	}
	des_cbc_encrypt(cipher_text, clear_text, (long)in_length, KS, ivec,
			0);
	fprintf(stdout, "\n\ndecrypted clear_text = %s", clear_text);

	fprintf(stdout, "\nclear %s\n", input);
	input = clear_text2;
	des_cbc_cksum(input, cipher_text, (long)strlen(input), KS, ivec, 1);
	fprintf(stdout, "\n\nencrypted cksum = (low to high bytes)\n");
	for (j = 0; j <= 7; j++)
	    fprintf(stdout, "%02x ", cipher_text[j]);

	/* test out random number generator */
	for (i = 0; i <= 7; i++) {
	    des_random_key(cipher_text);
	    des_key_sched(cipher_text, KS);
	    fprintf(stdout, "\n\nrandom key = (low to high bytes)\n");
	    for (j = 0; j <= 7; j++)
		fprintf(stdout, "%02x ", cipher_text[j]);
	}
    } else {
	if (vflag)
	    fprintf(stdout, "\nclear %s\n", input);
	do_encrypt(input, cipher_text);
	do_decrypt(clear_text, cipher_text);
    }
}

flip(array)
     char *array;
{
    int old, new, i, j;
    /* flips the bit order within each byte from 0 lsb to 0 msb */
    for (i = 0; i <= 7; i++) {
	old = *array;
	new = 0;
	for (j = 0; j <= 7; j++) {
	    if (old & 01)
		new = new | 01;
	    if (j < 7) {
		old = old >> 1;
		new = new << 1;
	    }
	}
	*array = new;
	array++;
    }
}

do_encrypt(in, out)
     char *in;
     char *out;
{
    for (i = 1; i <= nflag; i++) {
	des_ecb_encrypt(in, out, KS, 1);
	if (vflag) {
	    fprintf(stdout, "\nclear %s\n", in);
	    for (j = 0; j <= 7; j++)
		fprintf(stdout, "%02 X ", in[j] & 0xff);
	    fprintf(stdout, "\tcipher ");
	    for (j = 0; j <= 7; j++)
		fprintf(stdout, "%02X ", out[j] & 0xff);
	}
    }
}

do_decrypt(in, out)
     char *out;
     char *in;
    /* try to invert it */
{
    for (i = 1; i <= nflag; i++) {
	des_ecb_encrypt(out, in, KS, 0);
	if (vflag) {
	    fprintf(stdout, "\nclear %s\n", in);
	    for (j = 0; j <= 7; j++)
		fprintf(stdout, "%02X ", in[j] & 0xff);
	    fprintf(stdout, "\tcipher ");
	    for (j = 0; j <= 7; j++)
		fprintf(stdout, "%02X ", out[j] & 0xff);
	}
    }
}
