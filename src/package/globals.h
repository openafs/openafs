/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*------------------------------------------------------------------------
 * globals.h
 *
 * Description:
 *	Various global definitions for the package AFS workstation
 *	configuration tool.
 *
 *------------------------------------------------------------------------*/

#define	TRUE	1
#define	FALSE	0

#define	ERR_OUTOFMEMORY	-1
#define	ERR_FOPENFAILED	-2

#ifdef DEBUG
#define	dbgprint(x)   {fprintf x ; fflush(stderr);}
#else
#define	dbgprint(x)
#endif /* DEBUG */

char *emalloc();
char *ecalloc();

FILE *efopen();
