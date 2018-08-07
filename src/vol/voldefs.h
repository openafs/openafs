/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * System:		VICE-TWO
 * Module:		voldefs.h
 * Institution:	The Information Technology Center, Carnegie-Mellon University
 */

/* If you add volume types here, be sure to check the definition of
 * volumeWriteable in volume.h
 */

#define readwriteVolume		RWVOL
#define readonlyVolume		ROVOL
#define backupVolume		BACKVOL
#define rwreplicaVolume		RWREPL

#define RWVOL			0
#define ROVOL			1
#define BACKVOL			2
#define RWREPL			3

#define VOLMAXTYPES             4   /* current max number of types */

/* the maximum number of volumes in a volume group that we can handle */

#define VOL_VG_MAX_VOLS 20

/* How many times to retry if we raced the fileserver restarting, when trying
 * to check out or lock a volume. */

#define VOL_MAX_CHECKOUT_RETRIES 10

/* maximum number of vice partitions */
#define VOLMAXPARTS	255

#define VFORMATDIGITS 10

/*
 * All volumes will have a volume header file with the filename
 * in one of these two formats. The difference is largely historical.
 * Early versions of System V Unix had severe limitations on maximum
 * filename length, and the '.vol' extension had to be shortened to
 * allow for the maximum volume name to fit. Those restrictions no
 * longer apply, but the descendants of those legacy AIX and HPUX
 * servers still use the shorter name.
 *
 * If you change VHDREXT, you must change VFORMAT as well. Don't try
 * this with token pasting in cpp; it's just too inconsistent in
 * across C compilers.
 *
 * Note that <afs/param.h> must have been included before we get here.
 */

#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV)
#define VHDREXT	".vl"
#define	VFORMAT	"V%010" AFS_VOLID_FMT ".vl"
#else
#define	VHDREXT	".vol"
#define VFORMAT "V%010" AFS_VOLID_FMT ".vol"
#endif
#define	VHDRNAMELEN (VFORMATDIGITS + 1 + sizeof(VHDREXT) - 1) /* must match VFORMAT */

/* Maximum length (including trailing NUL) of a volume external path name. */
#define VMAXPATHLEN 512

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

/* Pathname for server id definitions. The server id is used to
 * allocate volume numbers. */

#define SERVERLISTPATH	"/vice/db/servers"
