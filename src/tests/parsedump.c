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

/* parsedump.c - Parse a volume dump file */

#include "dumpscan.h"
#include "dumpscan_errs.h"
#include "dumpfmt.h"
#include "internal.h"
#include "stagehdr.h"

static afs_uint32 parse_dumphdr  (XFILE *, unsigned char *, tagged_field *,
                               afs_uint32, tag_parse_info *, void *, void *);
static afs_uint32 parse_dumpend  (XFILE *, unsigned char *, tagged_field *,
                               afs_uint32, tag_parse_info *, void *, void *);
static afs_uint32 store_dumphdr  (XFILE *, unsigned char *, tagged_field *,
                               afs_uint32, tag_parse_info *, void *, void *);
static afs_uint32 parse_dumptimes(XFILE *, unsigned char *, tagged_field *,
                               afs_uint32, tag_parse_info *, void *, void *);

/** Field list for top-level objects **/
static tagged_field top_fields[] = {
  { TAG_DUMPHEADER,  DKIND_SPECIAL, "* DUMP HEADER",   parse_dumphdr, 0, 0 },
  { TAG_VOLHEADER,   DKIND_SPECIAL, "* VOLUME HEADER", parse_volhdr,  0, 0 },
  { TAG_VNODE,       DKIND_SPECIAL, "* VNODE ",        parse_vnode,   0, 0 },
  { TAG_DUMPEND,     DKIND_INT32,   "* DUMP END",      parse_dumpend, 0, 0 },
  { STAGE_VERSMIN,   DKIND_SPECIAL, "* STAGE HEADER",  try_backuphdr, 0, 0 },
  { 0,0,0,0,0,0 }};


/** Field list for dump headers **/
static tagged_field dumphdr_fields[] = {
  { DHTAG_VOLNAME,   DKIND_STRING,  " Volume name:  ", store_dumphdr,   0, 0 },
  { DHTAG_VOLID,     DKIND_INT32,   " Volume ID:    ", store_dumphdr,   0, 0 },
  { DHTAG_DUMPTIMES, DKIND_SPECIAL, " Dump Range:   ", parse_dumptimes, 0, 0 },
  { 0,0,0,0,0,0 }};


/* Parse a dump header, including its tagged attributes, and call the
 * dump-header callback, if one is defined.
 */
static afs_uint32 parse_dumphdr(XFILE *X, unsigned char *tag, tagged_field *field,
                             afs_uint32 value, tag_parse_info *pi,
                             void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_dump_header hdr;
  u_int64 where;
  afs_uint32 r;

  memset(&hdr, 0, sizeof(hdr));
  if (r = xftell(X, &where)) return r;
  sub64_32(hdr.offset, where, 1);

  if (r = ReadInt32(X, &hdr.magic)) return r;
  if (r = ReadInt32(X, &hdr.version)) return r;

  if (hdr.magic != DUMPBEGINMAGIC) {
    if (p->cb_error)
      (p->cb_error)(DSERR_MAGIC, 1, p->err_refcon,
                    "Invalid magic number (0x%08x) in dump header",
                    hdr.magic);
    return DSERR_MAGIC;
  }
  if (hdr.version != DUMPVERSION) {
    if (p->cb_error)
      (p->cb_error)(DSERR_MAGIC, 1, p->err_refcon,
                    "Unknown dump format version (%d) in dump header",
                    hdr.version);
    return DSERR_MAGIC;
  }

  if (p->print_flags & DSPRINT_DUMPHDR)
    printf("%s [%s = 0x%s]\n", field->label,
      decimate_int64(&hdr.offset, 0), hexify_int64(&hdr.offset, 0));
  if (p->print_flags & DSPRINT_DUMPHDR) {
    printf(" Magic number: 0x%08x\n", hdr.magic);
    printf(" Version:      %d\n", hdr.version);
  }
  r = ParseTaggedData(X, dumphdr_fields, tag, pi, g_refcon, (void *)&hdr);

  if (!r && p->cb_dumphdr) {
    r = xftell(X, &where);
    if (!r) r = (p->cb_dumphdr)(&hdr, X, p->refcon);
    if (p->flags & DSFLAG_SEEK) {
      if (!r) r = xfseek(X, &where);
      else xfseek(X, &where);
    }
  }
  if (hdr.field_mask & F_DUMPHDR_VOLNAME)
    free(hdr.volname);
  return r;
}


/* Store tagged attributes into a dump header */
static afs_uint32 store_dumphdr(XFILE *X, unsigned char *tag, tagged_field *field,
                             afs_uint32 value, tag_parse_info *pi,
                             void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_dump_header *hdr = (afs_dump_header *)l_refcon;

  switch (field->tag) {
  case DHTAG_VOLID:
    hdr->field_mask |= F_DUMPHDR_VOLID;
    hdr->volid = value;
    if (p->print_flags & DSPRINT_DUMPHDR)
      printf("%s%d\n", field->label, hdr->volid);
    return 0;

  case DHTAG_VOLNAME:
    if (tag && tag[0]) {
      hdr->field_mask |= F_DUMPHDR_VOLNAME;
      hdr->volname = tag;
      if (p->print_flags & DSPRINT_DUMPHDR)
        printf("%s%s\n", field->label, hdr->volname);
      return DSERR_KEEP;
    } else return 0;

  default:
    if (p->print_flags & DSPRINT_DUMPHDR)
      printf("%s<<< UNKNOWN FIELD >>>\n", field->label);
    return 0;
  }
}


/* Parse and store the dump time range from a dump header */
static afs_uint32 parse_dumptimes(XFILE *X, unsigned char *tag,
                               tagged_field *field, afs_uint32 value,
                               tag_parse_info *pi,
                               void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_dump_header *hdr = (afs_dump_header *)l_refcon;
  afs_uint16 count;
  afs_uint32 r;

  if (r = ReadInt16(X, &count)) return r;
  if (count != 2) {
    if (p->cb_error)
      (p->cb_error)(DSERR_FMT, 1, p->err_refcon,
                    "Incorrect array count (%d) in dump times", count);
    return DSERR_FMT;
  }
  if (r = ReadInt32(X, &hdr->from_date)) return r;
  if (r = ReadInt32(X, &hdr->to_date)) return r;
  hdr->field_mask |= (F_DUMPHDR_FROM | F_DUMPHDR_TO);
  if (p->print_flags & DSPRINT_DUMPHDR)
    printf("%s%d => %d\n", field->label, hdr->from_date, hdr->to_date);

  return ReadByte(X, tag);
}


/* Parse a dump_end record */
static afs_uint32 parse_dumpend(XFILE *X, unsigned char *tag, tagged_field *field,
                             afs_uint32 value, tag_parse_info *pi,
                             void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_uint32 r;

  if (value != DUMPENDMAGIC) {
    if (p->cb_error)
      (p->cb_error)(DSERR_MAGIC, 1, p->err_refcon,
                    "Invalid magic number (0x%08x) in dump trailer",
                    value);
    return DSERR_MAGIC;
  }
  if (p->print_flags & (DSPRINT_DUMPHDR | DSPRINT_ITEM))
    printf("%s\n", field->label);
  return DSERR_DONE;
}



afs_uint32 ParseDumpFile(XFILE *X, dump_parser *p)
{
  tag_parse_info pi;
  unsigned char tag;
  afs_uint32 r;

  prep_pi(p, &pi);
  r = ParseTaggedData(X, top_fields, &tag, &pi, (void *)p, 0);
  return handle_return(r, X, tag, p);
}


afs_uint32 ParseDumpHeader(XFILE *X, dump_parser *p)
{
  tag_parse_info pi;
  unsigned char tag;
  afs_uint32 r;

  prep_pi(p, &pi);
  if (r = ReadByte(X, &tag)) return handle_return(r, X, tag, p);
  if (tag != TAG_DUMPHEADER) return handle_return(0, X, tag, p);
  r = parse_dumphdr(X, &tag, &top_fields[0], 0, &pi, (void *)p, 0);
  if (!r && tag >= 1 && tag <= 4) r = DSERR_DONE;
  return handle_return(r, X, tag, p);
}


afs_uint32 ParseVolumeHeader(XFILE *X, dump_parser *p)
{
  tag_parse_info pi;
  unsigned char tag;
  afs_uint32 r;

  prep_pi(p, &pi);
  if (r = ReadByte(X, &tag)) return handle_return(r, X, tag, p);
  if (tag != TAG_VOLHEADER) return handle_return(0, X, tag, p);
  r = parse_volhdr(X, &tag, &top_fields[1], 0, &pi, (void *)p, 0);
  if (!r && tag >= 1 && tag <= 4) r = DSERR_DONE;
  return handle_return(r, X, tag, p);
}


afs_uint32 ParseVNode(XFILE *X, dump_parser *p)
{
  tag_parse_info pi;
  unsigned char tag;
  afs_uint32 r;

  prep_pi(p, &pi);
  if (r = ReadByte(X, &tag)) return handle_return(r, X, tag, p);
  if (tag != TAG_VNODE) return handle_return(0, X, tag, p);
  r = parse_vnode(X, &tag, &top_fields[2], 0, &pi, (void *)p, 0);
  if (!r && tag >= 1 && tag <= 4) r = DSERR_DONE;
  return handle_return(r, X, tag, p);
}
