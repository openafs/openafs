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

/* dump.c - Write out parts of a volume dump */

#include "dumpscan.h"
#include "dumpfmt.h"

#define COPYBUFSIZE 65536

afs_uint32 DumpDumpHeader(XFILE *OX, afs_dump_header *hdr)
{
  afs_uint32 r;

  if (r = WriteTagInt32Pair(OX, TAG_DUMPHEADER, hdr->magic, hdr->version))
    return r;

  if (hdr->field_mask & F_DUMPHDR_VOLID) {
    if (r = WriteTagInt32(OX, DHTAG_VOLID, hdr->volid)) return r;
  }
  if (hdr->field_mask & F_DUMPHDR_VOLNAME) {
    if (r = WriteByte(OX, DHTAG_VOLNAME)) return r;
    if (r = WriteString(OX, hdr->volname)) return r;
  }
  if (hdr->field_mask & (F_DUMPHDR_FROM | F_DUMPHDR_TO)) {
    if (r = WriteTagInt16(OX, DHTAG_DUMPTIMES, 2))
      return r;
    if (r = WriteInt32(OX, (hdr->field_mask & F_DUMPHDR_FROM)
                       ? hdr->from_date : 0))
      return r;
    if (r = WriteInt32(OX, (hdr->field_mask & F_DUMPHDR_TO)
                       ? hdr->to_date : time(0)))
      return r;
  }
  return 0;
}


afs_uint32 DumpVolumeHeader(XFILE *OX, afs_vol_header *hdr)
{
  afs_uint32 r;
  int i;

  if (r = WriteByte(OX, TAG_VOLHEADER)) return r;

  if (hdr->field_mask & F_VOLHDR_VOLID) {
    if (r = WriteTagInt32(OX, VHTAG_VOLID, hdr->volid)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_VOLVERS) {
    if (r = WriteTagInt32(OX, VHTAG_VERS, hdr->volvers)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_VOLNAME) {
    if (r = WriteByte(OX, VHTAG_VOLNAME)) return r;
    if (r = WriteString(OX, hdr->volname)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_INSERV) {
    if (r = WriteTagByte(OX, VHTAG_INSERV, hdr->flag_inservice)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_BLESSED) {
    if (r = WriteTagByte(OX, VHTAG_BLESSED, hdr->flag_blessed)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_VOLUNIQ) {
    if (r = WriteTagInt32(OX, VHTAG_VUNIQ, hdr->voluniq)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_VOLTYPE) {
    if (r = WriteTagByte(OX, VHTAG_TYPE, hdr->voltype)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_PARENT) {
    if (r = WriteTagInt32(OX, VHTAG_PARENT, hdr->parent_volid)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_CLONE) {
    if (r = WriteTagInt32(OX, VHTAG_CLONE, hdr->clone_volid)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_MAXQ) {
    if (r = WriteTagInt32(OX, VHTAG_MAXQUOTA, hdr->maxquota)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_MINQ) {
    if (r = WriteTagInt32(OX, VHTAG_MINQUOTA, hdr->minquota)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_DISKUSED) {
    if (r = WriteTagInt32(OX, VHTAG_DISKUSED, hdr->diskused)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_NFILES) {
    if (r = WriteTagInt32(OX, VHTAG_FILECNT, hdr->nfiles)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_ACCOUNT) {
    if (r = WriteTagInt32(OX, VHTAG_ACCOUNT, hdr->account_no)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_OWNER) {
    if (r = WriteTagInt32(OX, VHTAG_OWNER, hdr->owner)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_CREATE_DATE) {
    if (r = WriteTagInt32(OX, VHTAG_CREAT, hdr->create_date)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_ACCESS_DATE) {
    if (r = WriteTagInt32(OX, VHTAG_ACCESS, hdr->access_date)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_UPDATE_DATE) {
    if (r = WriteTagInt32(OX, VHTAG_UPDATE, hdr->update_date)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_EXPIRE_DATE) {
    if (r = WriteTagInt32(OX, VHTAG_EXPIRE, hdr->expire_date)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_BACKUP_DATE) {
    if (r = WriteTagInt32(OX, VHTAG_BACKUP, hdr->backup_date)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_OFFLINE_MSG) {
    if (r = WriteTagInt32(OX, VHTAG_OFFLINE, hdr->offline_msg)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_MOTD) {
    if (r = WriteTagInt32(OX, VHTAG_MOTD, hdr->motd_msg)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_WEEKUSE) {
    if (r = WriteTagInt16(OX, VHTAG_WEEKUSE, 7)) return r;
    for (i = 0; i < 7; i++)
      if (r = WriteInt32(OX, hdr->weekuse[i])) return r;
  }
  if (hdr->field_mask & F_VOLHDR_DAYUSE_DATE) {
    if (r = WriteTagInt32(OX, VHTAG_DUDATE, hdr->dayuse_date)) return r;
  }
  if (hdr->field_mask & F_VOLHDR_DAYUSE) {
    if (r = WriteTagInt32(OX, VHTAG_DAYUSE, hdr->dayuse)) return r;
  }
  return 0;
}


afs_uint32 DumpVNode(XFILE *OX, afs_vnode *v)
{
  afs_uint32 r;

  if (r = WriteTagInt32Pair(OX, TAG_VNODE, v->vnode, v->vuniq)) return r;

  if (v->field_mask & F_VNODE_TYPE) {
    if (r = WriteTagByte(OX, VTAG_TYPE, v->type)) return r;
  }
  if (v->field_mask & F_VNODE_NLINKS) {
    if (r = WriteTagInt16(OX, VTAG_NLINKS, v->nlinks)) return r;
  }
  if (v->field_mask & F_VNODE_DVERS) {
    if (r = WriteTagInt32(OX, VTAG_DVERS, v->datavers)) return r;
  }
  if (v->field_mask & F_VNODE_SDATE) {
    if (r = WriteTagInt32(OX, VTAG_SERVER_DATE, v->server_date)) return r;
  }
  if (v->field_mask & F_VNODE_AUTHOR) {
    if (r = WriteTagInt32(OX, VTAG_AUTHOR, v->author)) return r;
  }
  if (v->field_mask & F_VNODE_OWNER) {
    if (r = WriteTagInt32(OX, VTAG_OWNER, v->owner)) return r;
  }
  if (v->field_mask & F_VNODE_GROUP) {
    if (r = WriteTagInt32(OX, VTAG_GROUP, v->group)) return r;
  }
  if (v->field_mask & F_VNODE_MODE) {
    if (r = WriteTagInt16(OX, VTAG_MODE, v->mode)) return r;
  }
  if (v->field_mask & F_VNODE_PARENT) {
    if (r = WriteTagInt32(OX, VTAG_PARENT, v->parent)) return r;
  }
  if (v->field_mask & F_VNODE_CDATE) {
    if (r = WriteTagInt32(OX, VTAG_CLIENT_DATE, v->client_date)) return r;
  }
  if (v->field_mask & F_VNODE_ACL) {
    if (r = WriteByte(OX, VTAG_ACL)) return r;
    if (r = xfwrite(OX, v->acl, SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE))
      return r;
  }
  return 0;
}


afs_uint32 DumpVNodeData(XFILE *OX, char *buf, afs_uint32 size)
{
  afs_uint32 r;

  if (r = WriteTagInt32(OX, VTAG_DATA, size)) return r;
  if (r = xfwrite(OX, buf, size)) return r;
  return 0;
}


afs_uint32 CopyVNodeData(XFILE *OX, XFILE *X, afs_uint32 size)
{
  afs_uint32 r, n;
  static char buf[COPYBUFSIZE];

  if (r = WriteTagInt32(OX, VTAG_DATA, size)) return r;
  while (size) {
    n = (size > COPYBUFSIZE) ? COPYBUFSIZE : size;
    if (r = xfread(X, buf, n)) return r;
    if (r = xfwrite(OX, buf, n)) return r;
    size -= n;
  }
  return 0;
}


afs_uint32 DumpDumpEnd(XFILE *OX) {
  afs_uint32 r;

  if (r = WriteTagInt32(OX, TAG_DUMPEND, DUMPENDMAGIC)) return r;
  return 0;
}
