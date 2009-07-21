/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * The program will create a file that contains descriptions
 * of a set of rx rpc's.  The output of this program is used
 * as input to generator.c.
 *
 * Changing the output format of this program will necessitate
 * a change in generator.c
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PRIVATE static
#define IN
#define MACRO_BEGIN     do {
#define MACRO_END       } while (0)

#if defined(WIN32)
#include <io.h>
#define	close(x)		_close(x)
#endif

#define ATTR_COUNT		3
#define MAX_NO_OF_ARGUMENTS	2
#define MAX_TYPE_NAME_LEN	10
#define MAX_DIRECTION_NAME_LEN	6
#define MAX_ATTR_NAME_LEN	7
#define MAX_SIGNATURE_LEN	100000

#define NEXT_SEQ(S,M)	(((S) >= ((M)-1)) ? 0 : (S)+1)

#define MEM_CHK(x, y) if(!x) {fprintf(stderr, y); exit(1);}
#define FATAL( y ) {fprintf(stderr, y); exit(1);}
#define PrintShortUsage \
    MACRO_BEGIN	\
    	fprintf( stderr, \
"Usage: tableGen [-h] [-a] <appendFileName> -d <list of dirs> \
[-o] <outputFile> -t <list of types>\n"); \
    MACRO_END
#define PrintLongUsage \
    MACRO_BEGIN	\
        PrintShortUsage; \
    	fprintf( stderr, \
"\nCommand line options(case insensitive):\
\n\t-h = help message \
\n\t-a = set the file to append\
\n\t-d = set the list of directions (IN OUT INOUT)\
\n\t-o = set output file (mandatory)\
\n\t-t = set the list of types (char int8 short afs_int32 \
\n\t\tfloat double string ar_char ar_int8 ar_short \
\n\t\tar_int32 ar_float ar_double \
\n"); \
    MACRO_END

/* macro which sets if attributes for var/conf found */
#define SET_ATTR_FLAGS( x ) \
    MACRO_BEGIN	\
    	if (!strcmp(attrib[x], "size")) SIZE = TRUE;\
    	else if (!strcmp(attrib[x], "max")) MAX = TRUE; \
    	else if (!strcmp(attrib[x], "length")) LENGTH = TRUE;\
    	else if (!strcmp(attrib[x], "last")) LAST = TRUE;\
    MACRO_END

static char **dir;
static char **typ;
static int dir_size;
static int typ_size;
static int attr_size;

PRIVATE char *init_dir[] = { "IN", "OUT", "INOUT" };

PRIVATE char *init_typ[] = {
    "char",
    "short",
    "afs_int32",
    "varString",
    "ar_char",
    "ar_short",
    "ar_int32",
};
PRIVATE char *attrib[] = {
    "1 max",
    "1 size",
    "1 first",
    "1 last",
    "1 length",
    "2 max first",
    "2 max last",
    "2 max length",
    "2 size first",
    "2 size last",
    "2 size length",
    "2 first last",
    "2 first length",
    "3 max first last",
    "3 max first length",
    "3 size first last",
    "3 size first length"
};

/*
 * 31 bit random number generator, we don't really care how random
 * these numbers are, it is more important that identical IDL files
 * are generated no matter what platform the generator runs on
 */
static unsigned long randVal = 0x330E16;

PRIVATE double
drand32()
{
    randVal = ((randVal * 0xEECE66D) + 0xB) & 0xFFFFFFFF;
    return ((double)(randVal) / 4294967296.0);
}

/*
 * BunchArg -- prints signature for lots of arguments
 */
#define MAX_ARGS 10

PRIVATE void
BunchArg(FILE * O_FP)
{
    int num_args = ((int)((double)MAX_ARGS * drand32()));
    int i;
    int dir_index, typ_index;

    if (num_args == 0)
	num_args = 1;
    fprintf(O_FP, "%d ", num_args);

    for (i = 0; i < num_args; i++) {
	typ_index = ((int)((double)typ_size * drand32()));
	do {
	    dir_index = ((int)((double)dir_size * drand32()));
	} while (!strcmp(dir[dir_index], "INOUT")
		 && (!strcmp(typ[typ_index], "varString")));

	fprintf(O_FP, "( %s %s ) ", dir[dir_index], typ[typ_index]);
    }
    fprintf(O_FP, "\n");
}

/*
 * SingleArg -- prints signature for single argument of given type
 */
PRIVATE void
SingleArg(O_FP, typ_index)
     FILE *O_FP;
     IN int typ_index;
{
    int dir_index;

    /*
     * choose random argument direction, cannot use ref string
     * pointers for output parameters
     */
    do {
	dir_index = ((int)((double)dir_size * drand32()));
    } while (!strcmp(dir[dir_index], "INOUT")
	     && (!strcmp(typ[typ_index], "varString")));

    fprintf(O_FP, "1 ( %s %s )\n", dir[dir_index], typ[typ_index]);
}

/*
 * DoubleArg -- prints signature for two arguments of given types
 */
PRIVATE void
DoubleArg(O_FP, typ_index1, typ_index2)
     FILE *O_FP;
     IN int typ_index1;
     IN int typ_index2;
{
    int dir_index1;
    int dir_index2;

    /*
     * choose random argument direction, cannot use ref string
     * pointers for output parameters
     */
    do {
	dir_index1 = ((int)((double)dir_size * drand32()));
    } while (!strcmp(dir[dir_index1], "INOUT")
	     && (!strcmp(typ[typ_index1], "varString")));
    do {
	dir_index2 = ((int)((double)dir_size * drand32()));
    } while (!strcmp(dir[dir_index2], "INOUT")
	     && (!strcmp(typ[typ_index2], "varString")));

    fprintf(O_FP, "2 ( %s %s ) ", dir[dir_index1], typ[typ_index1]);
    fprintf(O_FP, "( %s %s )\n", dir[dir_index2], typ[typ_index2]);
}

/*
 * ProcessCmdLine -- processes the command line args 
 */
PRIVATE void
ProcessCmdLine(argc, argv, apFileNamePP, outputFileP)
     int argc;
     char **argv;
     char **apFileNamePP;
     char **outputFileP;
{
    int i, n;
    char **p;

    dir_size = 0;
    typ_size = 0;
    *outputFileP = NULL;
    if (argc == 1) {
	PrintShortUsage;
	exit(1);
    }
    /* command line processing */
    while (--argc > 0) {
	if ((*++argv)[0] == '-') {
	    switch ((*argv)[1]) {

	    case 'a':
	    case 'A':		/* input table file */
		*apFileNamePP = *++argv;
		--argc;
		break;
	    case 'd':
	    case 'D':
		n = 0;
		p = argv;
		while (argc > 1 && (*++p)[0] != '-') {
		    argc--;
		    n++;
		}
		if (n == 0)
		    FATAL("ProcessCmdLine: must give dir with -d\n");
		dir_size = n;
		dir = (char **)malloc(sizeof(char *) * n);
		MEM_CHK(dir, "ProcessCmdLine: out of mem dir\n");
		for (i = 0; i < n; i++)
		    *(dir + i) = *++argv;
		break;
	    case 'h':		/* display help */
	    case 'H':
		PrintLongUsage;
		exit(0);
		break;
	    case 'o':
	    case 'O':
		*outputFileP = *(++argv);
		--argc;
		break;
	    case 't':
	    case 'T':
		n = 0;
		p = argv;
		while (argc > 1 && (*++p)[0] != '-') {
		    --argc;
		    n++;
		}
		if (n == 0)
		    FATAL("ProcessCmdLine: must give typ with -t\n");
		typ_size = n;
		typ = (char **)malloc(sizeof(char *) * n);
		MEM_CHK(typ, "ProcessCmdLine: out of mem typ\n");
		for (i = 0; i < n; i++)
		    *(typ + i) = *++argv;
		break;
	    default:
		PrintLongUsage;
		exit(1);
		break;
	    }
	} else {
	    PrintShortUsage;
	    exit(1);
	}
    }
    if (*outputFileP == 0)
	FATAL("Please set output filename(s) using -o switch\n");

    if (dir_size == 0) {
	dir_size = sizeof(init_dir) / sizeof(init_dir[0]);
	dir = &init_dir[0];
    }

    if (typ_size == 0) {
	typ_size = sizeof(init_typ) / sizeof(init_typ[0]);
	typ = &init_typ[0];
    }

    attr_size = sizeof(attrib) / sizeof(attrib[0]);
}

void
main(argc, argv)
     int argc;
     char **argv;
{
    int i, j;
    char *apFileName = NULL;
    char *outputFile;
    FILE *A_FP;
    FILE *O_FP;
    char max_buf[MAX_SIGNATURE_LEN];

    ProcessCmdLine(argc, argv, &apFileName, &outputFile);

    /* open the files */
    O_FP = fopen(outputFile, "w");
    MEM_CHK(O_FP, "main: Unable to open output file\n");
    if (apFileName) {
	A_FP = fopen(apFileName, "r");
	MEM_CHK(A_FP, "main: Unable to open append file\n");
	while (fgets(max_buf, MAX_SIGNATURE_LEN, A_FP)) {
	    fputs(max_buf, O_FP);
	}
	fclose(A_FP);
    }

    for (i = 0; i < typ_size; i++) {
	SingleArg(O_FP, i);
	for (j = 0; j < typ_size; j++) {
	    DoubleArg(O_FP, i, j);
	}
    }
    for (i = 0; i < 100; i++) {
	BunchArg(O_FP);
    }

    fclose(O_FP);
    exit(0);
}
