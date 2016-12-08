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

#define RWVOL			0
#define ROVOL			1
#define BACKVOL			2

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
#define VFORMAT	"V%010" AFS_VOLID_FMT ".vl"
#else
#define VHDREXT	".vol"
#define VFORMAT	"V%010" AFS_VOLID_FMT ".vol"
#endif

#define VMAXPATHLEN 64		/* Maximum length (including null) of a volume
				 * external path name
				 */
