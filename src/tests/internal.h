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

/* internal.h - Routines for internal use only */

#include "xfiles.h"
#include "dumpscan.h"


/* parsevol.c - Routines to parse volume headers */
extern afs_uint32 parse_volhdr(XFILE *, unsigned char *, tagged_field *, afs_uint32,
                            tag_parse_info *, void *, void *);

/* parsevnode.c - Routines to parse vnodes and their fields */
extern afs_uint32 parse_vnode(XFILE *, unsigned char *, tagged_field *, afs_uint32,
                           tag_parse_info *, void *, void *);

/* directory.c - Routines for parsing AFS directories */
extern afs_uint32 parse_directory(XFILE *, dump_parser *, afs_vnode *,
                               afs_uint32, int);

/* backuphdr.c - Generic support for backup system headers */
extern afs_uint32 try_backuphdr(XFILE *X, unsigned char *tag, tagged_field *field,
                             afs_uint32 value, tag_parse_info *pi,
                             void *g_refcon, void *l_refcon);

/* util.c - Random utilities */
extern afs_uint32 handle_return(int, XFILE *, unsigned char, dump_parser *);
extern void prep_pi(dump_parser *, tag_parse_info *);
extern afs_uint32 match_next_vnode(XFILE *, dump_parser *, u_int64 *, afs_uint32);
