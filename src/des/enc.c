/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include "mit-cpyright.h"
#include "des.h"
#ifdef BSDUNIX
#include <sys/file.h>
#endif
#include <stdio.h>

Key_schedule KEYSCHED;
C_Block key = { 0, 1, 2, 3, 4, 5, 6, 7 };
C_Block sum;
char inbuf[512 + 8];		/* leave room for cksum and len */
char oubuf[512 + 8];
int debug;
int ind;
int oud;
afs_int32 orig_size;

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char *argv[];
{
    int encrypt;
    afs_int32 length;
    int *p;
    afs_int32 ivec[2];
    if (argc != 4) {
	fprintf(stderr, "%s: Usage: %s infile outfile mode.\n", argv[0],
		argv[0]);
	exit(1);
    }
    if (!strcmp(argv[3], "e"))
	encrypt = 1;
    else if (!strcmp(argv[3], "d"))
	encrypt = 0;
    else {
	fprintf(stderr, "%s: Mode must be e (encrypt) or d (decrypt).\n",
		argv[0]);
	exit(1);
    }
    if ((ind = open(argv[1], O_RDONLY, 0666)) < 0) {
	fprintf(stderr, "%s: Cannot open %s for input.\n", argv[0], argv[1]);
	exit(1);
    }
    if (!strcmp(argv[2], "-"))
	oud = dup(1);
    else if ((oud = open(argv[2], O_CREAT | O_WRONLY, 0666)) < 0) {
	fprintf(stderr, "%s: Cannot open %s for output.\n", argv[0], argv[2]);
	exit(1);
    }
#ifdef notdef
    (void)freopen("/dev/tty", "r", stdin);
    (void)freopen("/dev/tty", "w", stdout);
#endif
    read_password(key, "\n\07\07Enter Key> ", 1);
    if (key_sched(key, KEYSCHED) < 0) {
	fprintf(stderr, "%s: Key parity error\n", argv[0]);
	exit(1);
    }
    ivec[0] = 0;
    ivec[1] = 0;
    memcpy(sum, key, sizeof(C_Block));
    for (;;) {
	if ((length = read(ind, inbuf, 512)) < 0) {
	    fprintf(stderr, "%s: Error reading from input.\n", argv[0]);
	    exit(1);
	} else if (length == 0) {
	    fprintf(stderr, "\n");
	    break;
	}
	if (encrypt) {
#ifdef notdef
	    sum = quad_cksum(inbuf, NULL, length, 1, sum);
#endif
	    quad_cksum(inbuf, sum, length, 1, sum);
	    orig_size += length;
	    fprintf(stderr, "\nlength = %d tot length = %d quad_sum = %X %X",
		    length, orig_size, *(afs_uint32 *) sum,
		    *((afs_uint32 *) sum + 1));
	    fflush(stderr);
	}
	pcbc_encrypt(inbuf, oubuf, (afs_int32) length, KEYSCHED, ivec,
		     encrypt);
	if (!encrypt) {
#ifdef notdef
	    sum = quad_cksum(oubuf, NULL, length, 1, sum);
#endif
	    quad_cksum(oubuf, sum, length, 1, sum);
	    orig_size += length;
	    fprintf(stderr, "\nlength = %d tot length = %d quad_sum = %X ",
		    length, orig_size, *(afs_uint32 *) sum,
		    *((afs_uint32 *) sum + 1));
	}
	length = (length + 7) & ~07;
	write(oud, oubuf, length);
	if (!encrypt)
	    p = (int *)&oubuf[length - 8];
	else
	    p = (int *)&inbuf[length - 8];
	ivec[0] = *p++;
	ivec[1] = *p;
    }

    fprintf(stderr, "\ntot length = %d quad_sum = %X\n", orig_size, sum);
    /* if encrypting, now put the original length and checksum in */
}
