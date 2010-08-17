/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	Information Technology Center
	Carnegie-Mellon University
*/


#ifndef _PRSFS_
#define _PRSFS_

/*
An access list is associated with each directory.  Possession of each of the following rights allows
the possessor the corresponding privileges on ALL files in that directory
*/
#define PRSFS_READ            1	/*Read files */
#define PRSFS_WRITE           2	/*Write and write-lock existing files */
#define PRSFS_INSERT          4	/*Insert and write-lock new files */
#define PRSFS_LOOKUP          8	/*Enumerate files and examine access list */
#define PRSFS_DELETE          16	/*Remove files */
#define PRSFS_LOCK            32	/*Read-lock files */
#define PRSFS_ADMINISTER      64	/*Set access list of directory */

/* user space rights */
#define PRSFS_USR0	      0x01000000
#define PRSFS_USR1	      0x02000000
#define PRSFS_USR2	      0x04000000
#define PRSFS_USR3	      0x08000000
#define PRSFS_USR4	      0x10000000
#define PRSFS_USR5	      0x20000000
#define PRSFS_USR6	      0x40000000
#define PRSFS_USR7	      0x80000000
#endif
