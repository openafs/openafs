/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*------------------------------------------------------------------------
 * globfuncs.c
 *
 * Description:
 *	Generically useful functions for package, the AFS workstation
 *	configuration tool.
 *
 *------------------------------------------------------------------------*/

#include <stdio.h>
#include "globals.h"

/*------------------------------------------------------------------------
 * emalloc
 *
 * Description:
 *	Malloc with error checking.
 *
 * Arguments:
 *	unsigned size : Number of bytes to allocate.
 *
 * Returns:
 *	Ptr to new space if successful, 
 *	Exit from package on failure.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As described; may exit from package.
 *------------------------------------------------------------------------*/

char *
emalloc(unsigned int size)
{				/*emalloc */

    char *malloc();
    char *ptr;

    if ((ptr = malloc(size)) == NULL) {
	fprintf(stderr,
		"Error: Out of memory; malloc() failed allocating %d bytes\n",
		size);
	exit(ERR_OUTOFMEMORY);
    } else
	return (ptr);
}				/*emalloc */

/*------------------------------------------------------------------------
 * ecalloc
 *
 * Description:
 *	Calloc() with error checking.
 *
 * Arguments:
 *	unsigned nelem : Number of elements to allocate.
 *	unsigned size  : Number of bytes for each.
 *
 * Returns:
 *	Ptr to new space on success,
 *	Exit from package on failure.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As described; may exit from package.
 *------------------------------------------------------------------------*/

char *
ecalloc(unsigned int nelem, unsigned int size)
{				/*ecalloc */

    char *calloc();
    char *ptr;

    if ((ptr = calloc(nelem, size)) == NULL) {
	fprintf(stderr, "Error: Out of memory; calloc(%d, %d) failed\n",
		nelem, size);
	exit(ERR_OUTOFMEMORY);
    } else
	return (ptr);

}				/*ecalloc */

/*------------------------------------------------------------------------
 * efopen
 *
 * Description:
 *	Fopen with error checking.
 *
 * Arguments:
 *	char *filename	: Name of file to open.
 *	char *type	: Open mode.
 *
 * Returns:
 *	Ptr to file descriptor on success,
 *	Exit from package on failure.
 *
 * Environment:
 *	Nothign interesting.
 *
 * Side Effects:
 *	As described; may exit from package.
 *------------------------------------------------------------------------*/

FILE *
efopen(char *filename, char *type)
{				/*efopen */

    FILE *f;

    if ((f = fopen(filename, type)) == NULL) {
	fprintf(stderr, "Error: Couldn't open file; fopen(%s, %s) failed\n",
		filename, type);
	exit(ERR_FOPENFAILED);
    } else
	return (f);

}				/*efopen */
