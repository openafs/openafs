
#if !defined(lint) && !defined(LOCORE) && defined(RCS_HDRS)
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*

	System:		VICE-TWO
	Module:		vutils.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef AFS_VUTILS_H
#define AFS_VUTILS_H


/* Common definitions for volume utilities */

#define VUTIL_TIMEOUT	15	/* Timeout period for a remote host */


/* Exit codes -- see comments in tcp/exits.h */
#define VUTIL_RESTART	64	/* please restart this job later */
#define VUTIL_ABORT	1	/* do not restart this job */

#endif /* AFS_VUTILS_H */
