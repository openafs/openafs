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

/* dumpfmt.h - Description of AFS dump format */

#ifndef _DUMPFMT_H_
#define _DUMPFMT_H_

#include "intNN.h"

/* AFS dump file format:
 * All data in AFS dumps is tagged; that is, each data item is preceeded
 * by a 1-byte tag which identifies what the data item is.  There is no
 * explicit mention of what the data type is, but the type of each possible
 * data item (and thus, each possible tag) is fixed.  Usually this is
 * a relatively simple, fixed amount of data (byte, short, word), but
 * sometimes it is more complex.
 *
 * There is some amount of structure to an AFS volume dump.  Basically,
 * you get a dump header, followed by a volume header, followed by some
 * vnodes, followed by a dump end.  Each of these items (header, vnode,
 * dump end) consists of a tag, a fixed amount of required information,
 * and 0 or more tagged attributes (except dump-end, which has no attributes).
 *
 * Vnodes, in turn, are usually listed in a particular order.  First, we
 * list all the directory vnodes in the volume, in increasing order by
 * vnode.  Then, we list all the file vnodes, again in increasing order.
 * Directory vnodes must have a complete set of attributes and data, but
 * in an incremental dump, file vnodes may have no attributes if the vnode
 * has not changed since the reference date.
 *
 * The primary purpose of this file is to define the tags and some magic
 * numbers.  There is also some information that is defined in the Transarc
 * provided header files.
 */


/** MAGIC NUMBERS **/
#define DUMPVERSION     1
#define DUMPBEGINMAGIC  0xb3a11322
#define DUMPENDMAGIC    0x3a214b6e


/** TOP-LEVEL TAGS **/
#define TAG_DUMPHEADER  1
#define TAG_VOLHEADER   2
#define TAG_VNODE       3
#define TAG_DUMPEND     4


/** DUMP HEADER TAGS **/
#define DHTAG_VOLNAME    'n'
#define DHTAG_VOLID      'v'
#define DHTAG_DUMPTIMES  't'


/** VOLUME HEADER TAGS **/
#define VHTAG_VOLID      'i'
#define VHTAG_VERS       'v'
#define VHTAG_VOLNAME    'n'
#define VHTAG_INSERV     's'
#define VHTAG_BLESSED    'b'
#define VHTAG_VUNIQ      'u'
#define VHTAG_TYPE       't'
#define VHTAG_PARENT     'p'
#define VHTAG_CLONE      'c'
#define VHTAG_MAXQUOTA   'q'
#define VHTAG_MINQUOTA   'm'
#define VHTAG_DISKUSED   'd'
#define VHTAG_FILECNT    'f'
#define VHTAG_ACCOUNT    'a'
#define VHTAG_OWNER      'o'
#define VHTAG_CREAT      'C'
#define VHTAG_ACCESS     'A'
#define VHTAG_UPDATE     'U'
#define VHTAG_EXPIRE     'E'
#define VHTAG_BACKUP     'B'
#define VHTAG_OFFLINE    'O'
#define VHTAG_MOTD       'M'
#define VHTAG_WEEKUSE    'W'
#define VHTAG_DUDATE     'D'
#define VHTAG_DAYUSE     'Z'


/** VNODE TAGS **/
#define VTAG_TYPE        't'
#define VTAG_NLINKS      'l'
#define VTAG_DVERS       'v'
#define VTAG_CLIENT_DATE 'm'
#define VTAG_AUTHOR      'a'
#define VTAG_OWNER       'o'
#define VTAG_GROUP       'g'
#define VTAG_MODE        'b'
#define VTAG_PARENT      'p'
#define VTAG_SERVER_DATE 's'
#define VTAG_ACL         'A'
#define VTAG_DATA        'f'


#define AFS_DIR_EPP 64

typedef struct {
  afs_uint16 pgcount;
  afs_uint16 tag;
  char freecount;
  char freebitmap[AFS_DIR_EPP/8];
  char padding[32 - (5 + AFS_DIR_EPP/8)];
} afs_dir_pagehdr;

typedef struct {
  char flag;
  char length;
  afs_uint16 next;
  afs_uint32 vnode;
  afs_uint32 vunique;
  char name[16];
  char padding[4];
} afs_dir_direntry;

typedef union {
  afs_dir_pagehdr header;
  afs_dir_direntry entry[AFS_DIR_EPP];
} afs_dir_page;

#endif /* _DUMPFMT_H_ */
