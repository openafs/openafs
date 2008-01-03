/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		voldefs.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

/* If you add volume types here, be sure to check the definition of
   volumeWriteable in volume.h */

#define readwriteVolume		RWVOL
#define readonlyVolume		ROVOL
#define backupVolume		BACKVOL

#define RWVOL			0
#define ROVOL			1
#define BACKVOL			2

#define VOLMAXTYPES             3   /* _current_ max number of types */

/* maximum numbe of Vice partitions */
#define	VOLMAXPARTS	255

/* All volumes will have a volume header name in this format */
#if	defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV)
/* Note that <afs/param.h> must have been included before we get here... */
#define	VFORMAT	"V%010lu.vl"	/* Sys5's filename length limitation hits us again */
#define	VHDREXT	".vl"
#else
#define VFORMAT "V%010lu.vol"
#define	VHDREXT	".vol"
#endif
#define VMAXPATHLEN 64		/* Maximum length (including null) of a volume
				 * external path name */

#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
/* INODEDIR holds all the inodes. Since it's name does not begin with "V"
 * and it's created when the first volume is created, linear directory
 * searches will find the directory early. If only I had needed this before
 * the NT server went beta, it could be used there as well.
 */
#define INODEDIR "AFSIDat"
#define INODEDIRLEN (sizeof(INODEDIR)-1)
#endif

/* Pathname for the maximum volume id ever created by this server */
#define MAXVOLIDPATH	"/vice/vol/maxvolid"

/* Pathname for server id definitions--the server id is used to allocate volume numbers */
#define SERVERLISTPATH	"/vice/db/servers"

/* Values for connect parameter to VInitVolumePackage */
#define CONNECT_FS	1
#define DONT_CONNECT_FS	0
