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

/*
 * - <length>: variable size length, 0x80=unknown (means that
 *   this section must be parsed to proceed)
 * - If bit 7 is not set, bits 0-6 are the length
 * - If bit 7 is set, bits 0-6 tell you how many bytes long the
 *   length is.  The length follows immediately in MSB-first order.
 * - The special value 0x80 means indefinite length.
 * - 0xfe and 0xff would indicate a single-bit value, with the
 *   value stored in the low-order bit of the length.
 *
 *  List of known tags in the dump header section (section 0)
 *
 *     val     hex     what                            standard
 *     1       0x01    D_DUMPHEADER
 *     2       0x02    D_VOLUMEHEADER
 *     4       0x04    D_DUMPEND
 *     'n'     0x6e    V_name
 *     't'     0x74    fromtime, V_backupDate
 *     'v'     0x76    V_id / V_parentId               *
 *     126     0x7e    next tag critical               *
 */
/*
 *  List of known tags in the volume header section (section 1)
 *
 *     val     hex     what                            standard
 *     3       0x03    D_VNODE
 *     4       0x04    D_DUMPEND
 *     'A'     0x41    V_RaccessDate
 *     'C'     0x43    V_creationDate
 *     'D'     0x44    V_dayUseDate
 *     'E'     0x45    V_expirationDate
 *     'M'     0x4d    nullstring (motd)
 *     'O'     0x4f    V_offlineMessage
 *     'U'     0x55    V_updateDate
 *     'W'     0x57    V_weekUse
 *     'Z'     0x5a    V_dayUse
 *     'a'     0x61    V_acountNumber                  *
 *     'b'     0x62    V_blessed
 *     'c'     0x63    V_cloneId
 *     'd'     0x64    V_diskused                      *
 *     'f'     0x66    V_filecount                     *
 *     'i'     0x69    V_id                            *
 *     'm'     0x6d    V_minquota                      *
 *     'n'     0x6e    V_name
 *     'o'     0x6f    V_owner                         *
 *     'p'     0x70    V_parentId                      *
 *     'q'     0x71    V_maxquota                      *
 *     's'     0x73    V_inService
 *     't'     0x74    V_type
 *     'u'     0x75    V_uniquifier                    *
 *     'v'     0x76    V_stamp.version                 *
 *     126     0x7e    next tag critical               *
 */
/*
 *  List of known tags in the vnode section (section 2)
 *
 *     val     hex     what                            standard
 *     3       0x03    D_VNODE
 *     4       0x04    D_DUMPEND
 *     'A'     0x41    VVnodeDiskACL
 *     'a'     0x61    author                          *
 *     'b'     0x62    modeBits
 *     'f'     0x66    small file
 *     'g'     0x67    group                           *
 *     'h'     0x68    large file
 *     'l'     0x6c    linkcount
 *     'm'     0x6d    unixModifyTime                  *
 *     'o'     0x6f    owner                           *
 *     'p'     0x70    parent                          *
 *     's'     0x73    serverModifyTime                *
 *     't'     0x74    type
 *     'u'     0x74    lastUsageTime                   *
 *     'v'     0x76    dataVersion                     *
 *     126     0x7e    next tag critical               *
 */
