/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*	Standard Preamble for .h files

Abstract:	Contains miscellaneous general-purpose macros.

*/

#define MAXSELECT	20	/* Max no. of file desc. to be checked in select() calls */

/*------------------------------------------------------------*/
#define	IN			/* Input parameter */
#define OUT			/* Output parameter */
#define INOUT			/* Obvious */
/*------------------------------------------------------------*/


/* Ha, ha!! I did not realize C has a builtin XOR operator! */
#define XOR(a,b)  (unsigned char) (((a&~b)|(~a&b)) & 0377)	/* NOTE: a and b should be unsigned char */


/* Conditional debugging output macros */

#ifndef NOSAY
#define say(when, what, how)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf how;fflush(stdout);}\
	else
#else
#define say(when, what, how)	/* null macro; BEWARE: avoid side effects in say() */
#endif

/* the ones below are obsolete and are here for upward compatibility only */
#define say0(when, what, how)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how);fflush(stdout);}
#define say1(when, what, how, x1)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1); fflush(stdout);}
#define say2(when, what, how, x1, x2)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2); fflush(stdout);}
#define say3(when, what, how, x1, x2, x3)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3); fflush(stdout);}
#define say4(when, what, how, x1, x2, x3, x4)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4); fflush(stdout);}
#define say5(when, what, how, x1, x2, x3, x4, x5)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4, x5); fflush(stdout);}
#define say6(when, what, how, x1, x2, x3, x4, x5, x6)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4, x5, x6); fflush(stdout);}
#define say7(when, what, how, x1, x2, x3, x4, x5, x6, x7)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4, x5, x6, x7); fflush(stdout);}
#define say8(when, what, how, x1, x2, x3, x4, x5, x6, x7, x8)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4, x5, x6, x7, x8); fflush(stdout);}
#define say9(when, what, how, x1, x2, x3, x4, x5, x6, x7, x8, x9)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4, x5, x6, x7, x8, x9); fflush(stdout);}
#define say10(when, what, how, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10); fflush(stdout);}
#define say11(when, what, how, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
			printf(how, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11); fflush(stdout);}
#define say12(when, what, how, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12)\
	if (when < what){printf("\"%s\", line %d:    ", __FILE__, __LINE__);\
	    		printf(how, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12); fflush(stdout);}


/* length-checked string routines: return 0 on success, -1 on length violation */
#define SafeStrCat(dest,src,totalspace)\
    ((strlen(dest)+strlen(src) < totalspace) ? strcat(dest,src),0 : -1)

#define SafeStrCpy(dest,src,totalspace)\
    ((strlen(src) < totalspace) ? strcpy(dest,src),0 : -1)


/* The following definition of assert is slightly modified from the standard 4.2BSD one.
	This prints out the failing assertion, in addition to the file and line number.
	BEWARE:  avoid quotes in the assertion!!
	Also beware: you cannot make the NOASSERT case a null macro, because of side effects */

#undef assert
#ifndef NOASSERT
#define assert(ex) {if (!(ex)){fprintf(stderr,"Assertion failed: file %s, line %d\n", __FILE__, __LINE__);fprintf(stderr, "\t%s\n", # ex); abort();}}
#else
#define assert(ex) {if (!(ex)) abort();}
#endif


#define TRUE 1
#define FALSE 0

#ifdef LWP
#define SystemError(y) (fprintf(stderr, "%d(%s): ", getpid(), LWP_ActiveProcess->name), perror(y))
#else
#define SystemError(y) (fprintf(stderr, "%d: ", getpid()), perror(y))
#endif
