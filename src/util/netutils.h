/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * RCSID("$Header")
 */

#ifndef TRANSARC_NETUTILS_H
#define TRANSARC_NETUTILS_H

/* Network and IP address utility and file parsing functions */

extern int parseNetRestrictFile(
				afs_uint32 outAddrs[], 
				afs_uint32 mask[], 
				afs_uint32 mtu[],
				afs_uint32 maxAddrs,
				afs_uint32 *nAddrs, 
				char reason[], 
				const char *fileName
				);

extern int filterAddrs(
		       afs_uint32 addr1[],afs_uint32 addr2[],
		       afs_uint32  mask1[], afs_uint32 mask2[],
		       afs_uint32  mtu1[], afs_uint32 mtu2[]
		       );

extern int parseNetFiles(
			 afs_uint32  addrbuf[],
			 afs_uint32  maskbuf[],
			 afs_uint32  mtubuf[],
			 afs_uint32 max,
			 char reason[],
			 const char *niFilename,
			 const char *nrFilename
			 );

#endif /* TRANSARC_NETUTILS_H */ 
