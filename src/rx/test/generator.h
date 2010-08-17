/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define PRIVATE static
#define IN
#define INOUT
#define MACRO_BEGIN     do {
#define MACRO_END       } while (0)


typedef enum { FALSE, TRUE } boolean_t;

/* no. tests per file */
#define TESTS_PER_FILE		50

/* no: of elems in dir array */
#define DIR_SIZE 3

/* no: of elems in typ array */
#define TYP_SIZE 11

/* max strlength among the lems fo the dir array */
#define MAX_DIR_STR 7

/* max string length among th lems of the typ array */
#define MAX_TYP_STR 16

/* arrays size to be generated in the idl and itl files */
#define IDL_STR_MAX 1000
#define IDL_FIX_ARRAY_SIZE 15

/* length of vary/conf attrib , eg" length */
#define ATTRIB_LEN 7
#define ATTRIB_NO 3

typedef struct {
    char direction[MAX_DIR_STR];
    char type[MAX_TYP_STR];
    int attrib_ct;
    char attrib[ATTRIB_NO][ATTRIB_LEN];
    boolean_t first;
    boolean_t last;
    boolean_t length;
    boolean_t max;
    boolean_t size;
    unsigned int instringlen;
    char *inValue[IDL_FIX_ARRAY_SIZE];	/* value passed via RPC */
    char *inValue2[IDL_FIX_ARRAY_SIZE];
    char *outValue[IDL_FIX_ARRAY_SIZE];	/* value returned via RPC */
    char *outValue2[IDL_FIX_ARRAY_SIZE];
    int vc_low, vc_high, vc_max;	/* array bounds ([in] value) */
    int ovc_low, ovc_high;	/* array bounds ([out] value) */
} arg_tuple;

typedef struct {
    int argCount;
    arg_tuple *argDescr;
} rpcArgs;

#define MEM_CHK(x, y) if(!x) {fprintf(stderr, y); exit(1);}

#define FATAL( y ) {fprintf(stderr, y); exit(1);}

/* for vary/conf array for testing pusrposes we will assume a
high index of at least 5, so IDL_FIX_ARRAY_SIZE should never be
smaller than MIN_HIGH */
#define MIN_HIGH 2

/* max length of server name string -- file names <= 8 Chars, append Mgr,
and so limit server name to 5 chars */
#define MAX_SERV_NAME 5

#define PrintShortUsage \
    MACRO_BEGIN	\
    	fprintf( stderr, \
"Usage: generator [-h] [-l] [-f] <inputFileName> [-s] <serverName> \
-o <output dir> [-p] <platform> \n"); \
    MACRO_END

#define PrintLongUsage \
    MACRO_BEGIN	\
        PrintShortUsage; \
    	fprintf( stderr, \
"\nCommand line options(case insensitive):\
\n\t-h = help message \
\n\t-l = use lwps as the thread model instead of pthreads \
\n\t-f = set the input table file \
\n\t-s = set the server name (truncates to 5 chars) \
\n\t-o = set output directory \
\n\t-p = set target platform (NT or UNIX - defaults to NT) \
\n"); \
    MACRO_END

/* max no: of args 999 */
#define MAX_DIGITS_IN_ARGS 3

#define MAX_RAND_LENGTH 50

#define MAX_INDEX_DIGITS 10

/* size of rpc signature buffer */
#define SIGN_SIZE 10000

#define SkipWhiteSpaces(p)		\
MACRO_BEGIN				\
    while (isspace(*(p)))		\
	(p)++;				\
MACRO_END

/* limits for random generation */
#define MIN_CHAR 32
#define MAX_CHAR 126
#define MIN_FLT 0.00000000
#define MAX_FLT 1.00000000
#define MIN_DBL 0.0000000000000000
#define MAX_DBL 1.0000000000000000
