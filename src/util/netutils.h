/* Copyright (C)  1999  Transarc Corporation.  All rights reserved.
 *
 * RCSID("$Header")
 */

#ifndef TRANSARC_NETUTILS_H
#define TRANSARC_NETUTILS_H

/* Network and IP address utility and file parsing functions */

extern int parseNetRestrictFile(
				afs_int32 outAddrs[], 
				afs_int32 mask[], 
				afs_int32 mtu[],
				afs_uint32 maxAddrs,
				afs_uint32 *nAddrs, 
				char reason[], 
				const char *fileName
				);

extern int filterAddrs(
		       u_long addr1[],u_long addr2[],
		       afs_int32  mask1[], afs_int32 mask2[],
		       afs_int32  mtu1[], afs_int32 mtu2[]
		       );

extern int parseNetFiles(
			 afs_int32  addrbuf[],
			 afs_int32  maskbuf[],
			 afs_int32  mtubuf[],
			 u_long max,
			 char reason[],
			 const char *niFilename,
			 const char *nrFilename
			 );

#endif /* TRANSARC_NETUTILS_H */ 
