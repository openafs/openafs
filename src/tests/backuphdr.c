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

/* backuphdr.c - Parse and print backup system headers */

#include <stdlib.h>

#include "dumpscan.h"
#include "dumpscan_errs.h"
#include "stagehdr.h"

afs_uint32 try_backuphdr(XFILE *X, char *tag, tagged_field *field,
                      afs_uint32 value, tag_parse_info *pi,
                      void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  backup_system_header bh;
  u_int64 where;
  afs_uint32 r;

  /* Which header should we try (if any)? */
  switch (*tag) {
    case STAGE_VERSMIN: r = ParseStageHdr(X, tag, &bh); break;
    default: return DSERR_MAGIC;
  }
  if (r) return r;

  /* Do something with it... */
  if (p->print_flags & DSPRINT_BCKHDR) PrintBackupHdr(&bh);
  if (p->cb_bckhdr) {
    r = xftell(X, &where);
    if (!r && p->cb_bckhdr)
      r = (p->cb_bckhdr)(&bh, X, p->refcon);
    if (p->flags & DSFLAG_SEEK) {
      if (!r) r = xfseek(X, &where);
      else xfseek(X, &where);
    }
  }
  if (bh.server)  free(bh.server);
  if (bh.part)    free(bh.part);
  if (bh.volname) free(bh.volname);
  return r;
}


void PrintBackupHdr(backup_system_header *hdr)
{
  time_t from = hdr->from_date, to = hdr->to_date, dd = hdr->dump_date;

  printf("* BACKUP SYSTEM HEADER\n");
  printf(" Version:    %d\n", hdr->version);
  printf(" Volume:     %s (%d)\n", hdr->volname, hdr->volid);
  printf(" Location:   %s %s\n", hdr->server, hdr->part);
  printf(" Level:      %d\n", hdr->level);
  printf(" Range:      %d => %d\n", hdr->from_date, hdr->to_date);
  printf("          == %s", ctime(&from));
  printf("          => %s", ctime(&to));
  printf(" Dump Time:  %d == %s", hdr->dump_date, ctime(&dd));
  printf(" Dump Flags: 0x%08x\n", hdr->flags);
  printf(" Length:     %d\n", hdr->dumplen);
  printf(" File Num:   %d\n", hdr->filenum);
}
