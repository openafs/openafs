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

/* parsevol.c - Parse a volume header */

#include "dumpscan.h"
#include "dumpscan_errs.h"
#include "dumpfmt.h"

static afs_uint32 store_volhdr   (XFILE *, unsigned char *, tagged_field *,
                               afs_uint32, tag_parse_info *, void *, void *);
static afs_uint32 parse_weekuse  (XFILE *, unsigned char *, tagged_field *,
                               afs_uint32, tag_parse_info *, void *, void *);

/** Field list for volume headers **/
static tagged_field volhdr_fields[] = {
  { VHTAG_VOLID,     DKIND_INT32,   " Volume ID:   ", store_volhdr,  0, 0 },
  { VHTAG_VERS,      DKIND_INT32,   " Version:     ", store_volhdr,  0, 0 },
  { VHTAG_VOLNAME,   DKIND_STRING,  " Volume name: ", store_volhdr,  0, 0 },
  { VHTAG_INSERV,    DKIND_FLAG,    " In service?  ", store_volhdr,  0, 0 },
  { VHTAG_BLESSED,   DKIND_FLAG,    " Blessed?     ", store_volhdr,  0, 0 },
  { VHTAG_VUNIQ,     DKIND_INT32,   " Uniquifier:  ", store_volhdr,  0, 0 },
  { VHTAG_TYPE,      DKIND_BYTE,    " Type:        ", store_volhdr,  0, 0 },
  { VHTAG_PARENT,    DKIND_INT32,   " Parent ID:   ", store_volhdr,  0, 0 },
  { VHTAG_CLONE,     DKIND_INT32,   " Clone ID:    ", store_volhdr,  0, 0 },
  { VHTAG_MAXQUOTA,  DKIND_INT32,   " Max quota:   ", store_volhdr,  0, 0 },
  { VHTAG_MINQUOTA,  DKIND_INT32,   " Min quota:   ", store_volhdr,  0, 0 },
  { VHTAG_DISKUSED,  DKIND_INT32,   " Disk used:   ", store_volhdr,  0, 0 },
  { VHTAG_FILECNT,   DKIND_INT32,   " File count:  ", store_volhdr,  0, 0 },
  { VHTAG_ACCOUNT,   DKIND_INT32,   " Account:     ", store_volhdr,  0, 0 },
  { VHTAG_OWNER,     DKIND_INT32,   " Owner:       ", store_volhdr,  0, 0 },
  { VHTAG_CREAT,     DKIND_TIME,    " Created:     ", store_volhdr,  0, 0 },
  { VHTAG_ACCESS,    DKIND_TIME,    " Accessed:    ", store_volhdr,  0, 0 },
  { VHTAG_UPDATE,    DKIND_TIME,    " Updated:     ", store_volhdr,  0, 0 },
  { VHTAG_EXPIRE,    DKIND_TIME,    " Expires:     ", store_volhdr,  0, 0 },
  { VHTAG_BACKUP,    DKIND_TIME,    " Backed up:   ", store_volhdr,  0, 0 },
  { VHTAG_OFFLINE,   DKIND_STRING,  " Offine Msg:  ", store_volhdr,  0, 0 },
  { VHTAG_MOTD,      DKIND_STRING,  " MOTD:        ", store_volhdr,  0, 0 },
  { VHTAG_WEEKUSE,   DKIND_SPECIAL, " Weekuse:     ", parse_weekuse, 0, 0 },
  { VHTAG_DUDATE,    DKIND_TIME,    " Dayuse Date: ", store_volhdr,  0, 0 },
  { VHTAG_DAYUSE,    DKIND_INT32,   " Daily usage: ", store_volhdr,  0, 0 },
  { 0,0,0,0,0,0 }};


/* Parse a volume header, including any tagged attributes, and call the
 * volume-header callback, if one is defined.
 */
afs_uint32 parse_volhdr(XFILE *X, unsigned char *tag, tagged_field *field,
                     afs_uint32 value, tag_parse_info *pi,
                     void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_vol_header hdr;
  u_int64 where;
  afs_uint32 r;

  memset(&hdr, 0, sizeof(hdr));
  if (r = xftell(X, &where)) return r;
  sub64_32(hdr.offset, where, 1);
  if (p->print_flags & DSPRINT_VOLHDR)
    printf("%s [%s = 0x%s]\n", field->label,
           decimate_int64(&hdr.offset, 0), hexify_int64(&hdr.offset, 0));

  r = ParseTaggedData(X, volhdr_fields, tag, pi, g_refcon, (void *)&hdr);

  if (!r && p->cb_volhdr) {
    if (r = xftell(X, &where)) return r;
    r = (p->cb_volhdr)(&hdr, X, p->refcon);
    if (p->flags & DSFLAG_SEEK) {
      if (!r) r = xfseek(X, &where);
      else xfseek(X, &where);
    }
  }
  if (hdr.field_mask & F_VOLHDR_VOLUNIQ)
    p->vol_uniquifier = hdr.voluniq;
  if (hdr.field_mask & F_VOLHDR_VOLNAME)
    free(hdr.volname);
  if (hdr.field_mask & F_VOLHDR_OFFLINE_MSG)
    free(hdr.offline_msg);
  if (hdr.field_mask & F_VOLHDR_MOTD)
    free(hdr.motd_msg);
  return r;
}


/* Store data in a volume header */
static afs_uint32 store_volhdr(XFILE *X, unsigned char *tag, tagged_field *field,
                            afs_uint32 value, tag_parse_info *pi,
                            void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_vol_header *hdr = (afs_vol_header *)l_refcon;
  time_t when;
  afs_uint32 r = 0;

  switch (field->tag) {
  case VHTAG_VOLID:
    hdr->field_mask |= F_VOLHDR_VOLID;
    hdr->volid = value;
    break;

  case VHTAG_VERS:
    hdr->field_mask |= F_VOLHDR_VOLVERS;
    hdr->volvers = value;
    break;

  case VHTAG_VOLNAME:
    if (tag && tag[0]) {
      hdr->field_mask |= F_VOLHDR_VOLNAME;
      hdr->volname = tag;
      r = DSERR_KEEP;
    }
    break;

  case VHTAG_INSERV:
    hdr->field_mask |= F_VOLHDR_INSERV;
    hdr->flag_inservice = value;
    break;

  case VHTAG_BLESSED:
    hdr->field_mask |= F_VOLHDR_BLESSED;
    hdr->flag_blessed = value;
    break;

  case VHTAG_VUNIQ:
    hdr->field_mask |= F_VOLHDR_VOLUNIQ;
    hdr->voluniq = value;
    break;

  case VHTAG_TYPE:
    hdr->field_mask |= F_VOLHDR_VOLTYPE;
    hdr->voltype = value;
    break;

  case VHTAG_PARENT:
    hdr->field_mask |= F_VOLHDR_PARENT;
    hdr->parent_volid = value;
    break;

  case VHTAG_CLONE:
    hdr->field_mask |= F_VOLHDR_CLONE;
    hdr->clone_volid = value;
    break;

  case VHTAG_MAXQUOTA:
    hdr->field_mask |= F_VOLHDR_MAXQ;
    hdr->maxquota = value;
    break;

  case VHTAG_MINQUOTA:
    hdr->field_mask |= F_VOLHDR_MINQ;
    hdr->minquota = value;
    break;

  case VHTAG_DISKUSED:
    hdr->field_mask |= F_VOLHDR_DISKUSED;
    hdr->diskused = value;
    break;

  case VHTAG_FILECNT:
    hdr->field_mask |= F_VOLHDR_NFILES;
    hdr->nfiles = value;
    break;

  case VHTAG_ACCOUNT:
    hdr->field_mask |= F_VOLHDR_ACCOUNT;
    hdr->account_no = value;
    break;

  case VHTAG_OWNER:
    hdr->field_mask |= F_VOLHDR_OWNER;
    hdr->owner = value;
    break;

  case VHTAG_CREAT:
    hdr->field_mask |= F_VOLHDR_CREATE_DATE;
    hdr->create_date = value;
    break;

  case VHTAG_ACCESS:
    hdr->field_mask |= F_VOLHDR_ACCESS_DATE;
    hdr->access_date = value;
    break;

  case VHTAG_UPDATE:
    hdr->field_mask |= F_VOLHDR_UPDATE_DATE;
    hdr->update_date = value;
    break;

  case VHTAG_EXPIRE:
    hdr->field_mask |= F_VOLHDR_EXPIRE_DATE;
    hdr->expire_date = value;
    break;

  case VHTAG_BACKUP:
    hdr->field_mask |= F_VOLHDR_BACKUP_DATE;
    hdr->backup_date = value;
    break;

  case VHTAG_OFFLINE:
    if (tag && tag[0]) {
      hdr->field_mask |= F_VOLHDR_OFFLINE_MSG;
      hdr->offline_msg = tag;
      r = DSERR_KEEP;
    }
    break;

  case VHTAG_MOTD:
    if (tag && tag[0]) {
      hdr->field_mask |= F_VOLHDR_MOTD;
      hdr->motd_msg = tag;
      r = DSERR_KEEP;
    }
    break;

  case VHTAG_DUDATE:
    hdr->field_mask |= F_VOLHDR_DAYUSE_DATE;
    hdr->dayuse_date = value;
    break;

  case VHTAG_DAYUSE:
    hdr->field_mask |= F_VOLHDR_DAYUSE;
    hdr->dayuse = value;
    break;
  }

  if (p->print_flags & DSPRINT_VOLHDR)
    switch (field->kind) {
    case DKIND_BYTE:
    case DKIND_INT16:
    case DKIND_INT32:  printf("%s%d\n",     field->label, value); break;
    case DKIND_HEX8:   printf("%s0x%02x\n", field->label, value); break;
    case DKIND_HEX16:  printf("%s0x%04x\n", field->label, value); break;
    case DKIND_HEX32:  printf("%s0x%08x\n", field->label, value); break;
    case DKIND_CHAR:   printf("%s%c\n",     field->label, value); break;
    case DKIND_STRING: printf("%s%s\n",     field->label, tag);   break;
    case DKIND_FLAG:
      printf("%s%s\n", field->label, value ? "true" : "false");
      break;
    case DKIND_TIME:
      when = value;
      printf("%s%s", field->label, ctime(&when));
      break;
  }
  return r;
}


/* Parse and store the week use data from a volume header */
static afs_uint32 parse_weekuse(XFILE *X, unsigned char *tag, tagged_field *field,
                             afs_uint32 value, tag_parse_info *pi,
                             void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_vol_header *hdr = (afs_vol_header *)l_refcon;
  afs_uint16 count;
  afs_uint32 r;
  unsigned int i;

  if (r = ReadInt16(X, &count)) return r;
  if (count != 7) {
    if (p->cb_error)
      (p->cb_error)(DSERR_FMT, 1, p->err_refcon,
                    "Incorrect array count (%d) in weekuse data", count);
    return DSERR_FMT;
  }
  for (i = 0; i < count; i++)
    if (r = ReadInt32(X, hdr->weekuse + i)) return r;
  hdr->field_mask |= F_VOLHDR_WEEKUSE;
  if (p->print_flags & DSPRINT_VOLHDR) {
    printf("%s%10d %10d %10d %10d\n", field->label,
           hdr->weekuse[0], hdr->weekuse[1], hdr->weekuse[2], hdr->weekuse[3]);
    printf("%s%10d %10d %10d\n", field->label,
           hdr->weekuse[4], hdr->weekuse[5], hdr->weekuse[6]);
  }
  return ReadByte(X, tag);
}
