/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * exit status:		 0 ==> success
 *			-1 ==> error
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <mit-cpyright.h>
#include <stdio.h>
#include <errno.h>
#include <des.h>

#define MIN_ARGC	0	/* min # args, not incl flags */
#define MAX_ARGC	99	/* max # args, not incl flags */

extern char *errmsg();
extern int des_key_sched();
char *progname;
int sflag;
int vflag;
int kflag;
int mflag;
int pid;
extern int des_debug;

afs_int32 dummy[2];
unsigned char dum_c[8] = { 0x80, 1, 1, 1, 1, 1, 1, 1 };
des_key_schedule KS;
des_cblock kk;

main(argc, argv)
     int argc;
     char *argv[];
{
    /*  Local Declarations */

    int i;
    progname = argv[0];		/* salt away invoking program */

    while (--argc > 0 && (*++argv)[0] == '-')
	for (i = 1; argv[0][i] != '\0'; i++) {
	    switch (argv[0][i]) {

		/* debug flag */
	    case 'd':
		des_debug = 1;
		continue;

		/* keys flag */
	    case 'k':
		kflag = 1;
		continue;

		/* test ANSI msb only key */
	    case 'm':
		mflag = 1;
		continue;

	    default:
		printf("%s: illegal flag \"%c\" ", progname, argv[0][i]);
		exit(1);
	    }
	};

    if (argc < MIN_ARGC || argc > MAX_ARGC) {
	printf("Usage: xxx [-xxx]  xxx xxx\n");
	exit(1);
    }

    /*  argv[0] now points to first non-option arg, if any */


    if (des_debug) {
	if (mflag) {
	    fprintf(stderr, "\nChecking a key 0x 80 01 01 01 01 01 01 01 ");
	    fprintf(stderr, "\nKey = ");
	    des_key_sched(dum_c, KS);
	    des_cblock_print(dum_c);
	    return;
	}

	if (kflag) {
	    printf("\nCHecking a weak key...");
	    dummy[0] = 0x01fe01fe;
	    dummy[1] = 0x01fe01fe;
	    des_key_sched(dummy, KS);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKey[0] = %x Key[1] = %x", dummy[0], dummy[1]);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKey[0] = %X Key[1] = %X", dummy[0], dummy[1]);
#endif

	    dummy[0] = 0x01010101;
	    dummy[1] = 0x01010101;
#ifdef BSDUNIX
	    fprintf(stderr, "\nKey[0] = %x Key[1] = %x", dummy[0], dummy[1]);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKey[0] = %X Key[1] = %X", dummy[0], dummy[1]);
#endif
	    des_key_sched(dummy, KS);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKS= %x", *(afs_int32 *) KS);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKS= %X", *(afs_int32 *) KS);
#endif
	    dummy[0] = 0x01010101;
	    dummy[1] = 0x01010101;
#ifdef BSDUNIX
	    fprintf(stderr, "\nKey[0] = %x Key[1] = %x", dummy[0], dummy[1]);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKey[0] = %X Key[1] = %X", dummy[0], dummy[1]);
#endif
	    des_key_sched(dummy, KS);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKS= %x", *(afs_int32 *) KS);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKS= %X", *(afs_int32 *) KS);
#endif

	    dummy[0] = 0x80808080;
	    dummy[1] = 0x80808080;
#ifdef BSDUNIX
	    fprintf(stderr, "\nKey[0] = %x Key[1] = %x", dummy[0], dummy[1]);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKey[0] = %X Key[1] = %X", dummy[0], dummy[1]);
#endif
	    des_key_sched(dummy, KS);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKS[0]= %x", *(afs_int32 *) KS);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKS[0]= %X", *(afs_int32 *) KS);
#endif

	    printf("\nstring to key 'a'");
	    des_string_to_key("a", dummy);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKey[0] = %x Key[1] = %x", dummy[0], dummy[1]);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKey[0] = %X Key[1] = %X", dummy[0], dummy[1]);
#endif
	    des_key_sched(dummy, KS);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKS= %x", *(afs_int32 *) KS);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKS= %X", *(afs_int32 *) KS);
#endif

	    printf("\nstring to key 'c'");
	    des_string_to_key("c", dummy);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKey[0] = %x Key[1] = %x", dummy[0], dummy[1]);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKey[0] = %X Key[1] = %X", dummy[0], dummy[1]);
#endif
	    des_key_sched(dummy, KS);
#ifdef BSDUNIX
	    fprintf(stderr, "\nKS= %x", *(afs_int32 *) KS);
#endif
#ifdef CROSSMSDOS
	    fprintf(stderr, "\nKS= %X", *(afs_int32 *) KS);
#endif
	}

	printf("\nstring to key 'e'");
	des_string_to_key("e", dummy);
#ifdef BSDUNIX
	fprintf(stderr, "\nKey[0] = %x Key[1] = %x", dummy[0], dummy[1]);
#endif
#ifdef CROSSMSDOS
	fprintf(stderr, "\nKey[0] = %X Key[1] = %X", dummy[0], dummy[1]);
#endif
	des_key_sched(dummy, KS);
#ifdef BSDUNIX
	fprintf(stderr, "\nKS= %x", KS[0]);
#endif
#ifdef CROSSMSDOS
	fprintf(stderr, "\nKS= %X", KS[0]);
#endif

	printf("\ndes_string_to_key '%s'", argv[0]);
	des_string_to_key(argv[0], dummy);
#ifdef notdef
	des_string_to_key(argv[0], dummy);

	for (i = 0; i < 1; i++)
	    des_key_sched(dummy, KS);
    } else {
	for (i = 0; i < 1000; i++) {
	    des_string_to_key(argv[0], kk);
	    des_key_sched(kk, KS);
	}
#endif
    }
}
