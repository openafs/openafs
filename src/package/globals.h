/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

/*------------------------------------------------------------------------
 * globals.h
 *
 * Description:
 *	Various global definitions for the package AFS workstation
 *	configuration tool.
 *
 * Author:
 *	Transarc Corporation & Carnegie Mellon University
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
