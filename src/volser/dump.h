/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		Volser
	Module:		dump.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#define DUMPVERSION	1

#define DUMPENDMAGIC	0x3A214B6E
#define DUMPBEGINMAGIC	0xB3A11322

#define D_DUMPHEADER	1
#define D_VOLUMEHEADER  2
#define D_VNODE		3
#define D_DUMPEND	4

#define D_MAX		20

#define MAXDUMPTIMES	50

/* DumpHeader:
   Each {from,to} pair of time values gives a span of time covered by this dump.
   Merged dumps may have multiple pairs if there are dumps missing from the merge */

struct DumpHeader {
    afs_int32 version;
    VolumeId volumeId;
    char volumeName[VNAMESIZE];
    int nDumpTimes;		/* Number of pairs */
    struct {
	afs_int32 from, to;
    } dumpTimes[MAXDUMPTIMES];
};


/* Some handshaking constants for volume move */
#define SHAKE1	"x"
#define SHAKE2	"y"
#define SHAKE3	"z"
#define SHAKE4	"P"
#define SHAKE5	"Q"
#define SHAKE_ABORT	"!"
