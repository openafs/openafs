/*
 * CMUCS AFStools
 * dumpscan - routines for scanning and manipulating AFS volume dumps
 *
 * Copyright (c) 1998 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/* stagehdr.h - (old) Stage backup header format */

#ifndef _STAGEHDR_H_
#define _STAGEHDR_H_

#include "intNN.h"

/* Stage-related constants */
#define STAGE_MAGIC    0x00adf8bc       /* magic number for stage header */
#define STAGE_CHECKSUM 84446            /* checksum (same as 4.2bsd dump) */
#define STAGE_VERSMIN  20               /* minimum version */
#define STAGE_NAMLEN   64               /* length of host/part/vol names */
#define STAGE_HDRLEN   1024             /* size of the header */

struct stage_header {
  unsigned char c_vers;               /* header version (starts at 20) */
  unsigned char c_notused[3];
  afs_uint32       c_fdate;              /* dump "from" date */
  afs_uint32       c_tdate;              /* dump "to" date */
  afs_uint32       c_filenum;            /* tape file number */
  afs_uint32       c_time;               /* time dump was done */
  char          c_host[STAGE_NAMLEN]; /* hostname volume came from */
  char          c_disk[STAGE_NAMLEN]; /* partition volume came from */
  char          c_name[STAGE_NAMLEN]; /* volume name */
  afs_uint32       c_id;                 /* volume ID */
  afs_uint32       c_length;             /* length of the dump */
  afs_uint32       c_level;              /* dump level */
  afs_uint32       c_magic;              /* magic number */
  afs_uint32       c_checksum;           /* checksum of backup header */
  afs_uint32       c_flags;              /* feature flags */
};

#endif /* _STAGEHDR_H_ */
