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

/* repair.c - Routines to generate a repaired dump */

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "dumpscan.h"
#include "dumpscan_errs.h"
#include "dumpfmt.h"

#include <afs/acl.h>
#include <afs/dir.h>
#include <afs/prs_fs.h>

XFILE repair_output;
int repair_verbose;
#define RV repair_verbose


/* Try to dump a dump header.  Generate missing fields, if neccessary */
afs_uint32 repair_dumphdr_cb(afs_dump_header *hdr, XFILE *X, void *refcon)
{
  afs_uint32 r, field_mask = hdr->field_mask;
  char volname[22];

  if (!(field_mask & F_DUMPHDR_VOLID)) {
    if (RV) fprintf(stderr, ">>> DUMP HEADER missing volume ID\n");
    return DSERR_FMT;
  }
  if (!(field_mask & F_DUMPHDR_VOLNAME)) {
    if (RV) {
      fprintf(stderr, ">>> DUMP HEADER missing volume name\n");
      fprintf(stderr, ">>> Will use RESTORED.%d\n", hdr->volid);
    }
    sprintf(volname, "RESTORED.%d", hdr->volid);
    hdr->volname = (unsigned char *)malloc(strlen(volname) + 1);
    if (!hdr->volname) return ENOMEM;
    strcpy(hdr->volname, volname);
    hdr->field_mask |= F_DUMPHDR_VOLNAME;
  }
  if (!(field_mask & F_DUMPHDR_FROM)) {
    if (RV) fprintf(stderr, ">>> DUMP HEADER missing from time (using 0)\n");
    hdr->from_date = 0;
    hdr->field_mask |= F_DUMPHDR_FROM;
  }
  if (!(field_mask & F_DUMPHDR_TO)) {
    hdr->to_date = time(0);
    if (RV) fprintf(stderr, ">>> DUMP HEADER missing from time (using %d)\n",
      hdr->to_date);
    hdr->field_mask |= F_DUMPHDR_TO;
  }

  return DumpDumpHeader(&repair_output, hdr);
}


/* Try to dump a volume header.  Generate missing fields, if necessary */
afs_uint32 repair_volhdr_cb(afs_vol_header *hdr, XFILE *X, void *refcon)
{
  afs_uint32 r, field_mask = hdr->field_mask;
  char volname[22];

  if (!(field_mask & F_VOLHDR_VOLID)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER missing volume ID\n");
    return DSERR_FMT;
  }
  if (!(field_mask & F_VOLHDR_VOLVERS)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER missing version (using 1)\n");
    hdr->volvers = 1;
    hdr->field_mask |= F_VOLHDR_VOLVERS;
  } else if (hdr->volvers != 1) {
    if (RV) fprintf(stderr, ">>> VOL HEADER bogus version %d (using 1)\n",
            hdr->volvers);
    hdr->volvers = 1;
  }
  if (!(field_mask & F_VOLHDR_VOLNAME)) {
    if (RV) {
      fprintf(stderr, ">>> VOL HEADER missing volume name\n");
      fprintf(stderr, ">>> Will use RESTORED.%d\n", hdr->volid);
    }
    sprintf(volname, "RESTORED.%d", hdr->volid);
    hdr->volname = (unsigned char *)malloc(strlen(volname) + 1);
    if (!hdr->volname) return ENOMEM;
    strcpy(hdr->volname, volname);
    hdr->field_mask |= F_VOLHDR_VOLNAME;
  }
  if (!(field_mask & F_VOLHDR_INSERV)) {
    if (RV)
      fprintf(stderr, ">>> VOL HEADER missing in-service flag (using 1)\n");
    hdr->flag_inservice = 1;
    hdr->field_mask |= F_VOLHDR_INSERV;
  }
  if (!(field_mask & F_VOLHDR_BLESSED)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER missing blessed flag (using 1)\n");
    hdr->flag_blessed = 1;
    hdr->field_mask |= F_VOLHDR_BLESSED;
  }
  if (!(field_mask & F_VOLHDR_VOLUNIQ)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER missing uniquifier (using 1)\n");
    hdr->voluniq = 1;
    hdr->field_mask |= F_VOLHDR_VOLUNIQ;
  }
  if (!(field_mask & F_VOLHDR_VOLTYPE)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER missing type (using 0: RW)\n");
    hdr->voltype = 0;
    hdr->field_mask |= F_VOLHDR_VOLTYPE;
  } else if (hdr->voltype < 0 || hdr->voltype > 2) {
    if (RV) fprintf(stderr, ">>> VOL HEADER bogus type %d (using 0: RW)\n",
            hdr->voltype);
    hdr->voltype = 0;
  }
  if (!(field_mask & F_VOLHDR_PARENT)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER parent (using %d)\n", hdr->volid);
    hdr->parent_volid = hdr->volid;
    hdr->field_mask |= F_VOLHDR_PARENT;
  }
  if (!(field_mask & F_VOLHDR_MAXQ)) {
    if (field_mask & F_VOLHDR_DISKUSED) hdr->maxquota = hdr->diskused;
    else hdr->maxquota = 1;
    if (RV) fprintf(stderr, ">>> VOL HEADER missing max quota (using %d)\n",
            hdr->maxquota);
    hdr->field_mask |= F_VOLHDR_MAXQ;
  }
  if (!(field_mask & F_VOLHDR_DISKUSED)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER missing disk used (using 2048)\n");
    hdr->diskused = 2048;
    hdr->field_mask |= F_VOLHDR_DISKUSED;
  }
  if (!(field_mask & F_VOLHDR_NFILES)) {
    if (RV) fprintf(stderr, ">>> VOL HEADER missing file count (using 1)\n");
    hdr->nfiles = 1;
    hdr->field_mask |= F_VOLHDR_NFILES;
  }
  if (!(field_mask & F_VOLHDR_CREATE_DATE)) {
    hdr->create_date = 0;
    if ((field_mask & F_VOLHDR_ACCESS_DATE)
    &&  (!hdr->create_date || hdr->access_date < hdr->create_date))
      hdr->create_date = hdr->access_date;
    if ((field_mask & F_VOLHDR_UPDATE_DATE)
    &&  (!hdr->create_date || hdr->update_date < hdr->create_date))
      hdr->create_date = hdr->update_date;
    if ((field_mask & F_VOLHDR_BACKUP_DATE)
    &&  (!hdr->create_date || hdr->backup_date < hdr->create_date))
      hdr->create_date = hdr->backup_date;

    if (RV) fprintf(stderr, ">>> VOL HEADER missing create date (using %d)\n",
            hdr->create_date);
    hdr->field_mask |= F_VOLHDR_CREATE_DATE;
  }
  if (!(field_mask & F_VOLHDR_ACCESS_DATE)) {
    hdr->access_date = 0;
    if ((field_mask & F_VOLHDR_CREATE_DATE)
    &&  (!hdr->access_date || hdr->create_date > hdr->access_date))
      hdr->access_date = hdr->create_date;
    if ((field_mask & F_VOLHDR_UPDATE_DATE)
    &&  (!hdr->access_date || hdr->update_date > hdr->access_date))
      hdr->access_date = hdr->update_date;
    if ((field_mask & F_VOLHDR_BACKUP_DATE)
    &&  (!hdr->access_date || hdr->backup_date > hdr->access_date))
      hdr->access_date = hdr->backup_date;

    if (RV) fprintf(stderr, ">>> VOL HEADER missing access date (using %d)\n",
            hdr->access_date);
    hdr->field_mask |= F_VOLHDR_ACCESS_DATE;
  }
  if (!(field_mask & F_VOLHDR_UPDATE_DATE)) {
    hdr->update_date = 0;
    if ((field_mask & F_VOLHDR_CREATE_DATE)
    &&  (!hdr->update_date || hdr->create_date > hdr->update_date))
      hdr->update_date = hdr->create_date;
    if ((field_mask & F_VOLHDR_ACCESS_DATE) && !hdr->update_date)
      hdr->update_date = hdr->access_date;
    if ((field_mask & F_VOLHDR_BACKUP_DATE) && !hdr->update_date)
      hdr->update_date = hdr->backup_date;

    if (RV) fprintf(stderr, ">>> VOL HEADER missing update date (using %d)\n",
            hdr->update_date);
    hdr->field_mask |= F_VOLHDR_UPDATE_DATE;
  }

  return DumpVolumeHeader(&repair_output, hdr);
}


/* Try to dump a vnode.  Generate missing fields, if necessary */
afs_uint32 repair_vnode_cb(afs_vnode *v, XFILE *X, void *refcon)
{
  afs_uint32 r, field_mask = v->field_mask;

  if ((v->vnode & 1) && !field_mask) {
    if (RV) fprintf(stderr, ">>> VNODE %d is directory but has no fields?\n");
    v->type = vDirectory;
    v->field_mask |= F_VNODE_TYPE;
    field_mask = F_VNODE_TYPE; /* Messy! */
  }
  if (field_mask && !(field_mask & F_VNODE_TYPE)) {
    v->type = (v->vnode & 1) ? vDirectory : vFile;
    if (RV) fprintf(stderr, ">>> VNODE %d missing type (using %d)\n",
            v->vnode, v->type);
    v->field_mask |= F_VNODE_TYPE;
  }
  if (field_mask && !(field_mask & F_VNODE_NLINKS)) {
    if (RV)
      fprintf(stderr, ">>> VNODE %d missing link count (using 1)\n", v->vnode);
    v->nlinks = 1;
    v->field_mask |= F_VNODE_NLINKS;
  }
  if (field_mask && !(field_mask & F_VNODE_PARENT)) {
    if (RV)
      fprintf(stderr, ">>> VNODE %d missing parent (using 1)\n", v->vnode);
    v->parent = 1;
    v->field_mask |= F_VNODE_PARENT;
  }
  if (field_mask && !(field_mask & F_VNODE_DVERS)) {
    if (RV) fprintf(stderr, ">>> VNODE %d missing data version (using 1)\n",
              v->vnode);
    v->datavers = 1;
    v->field_mask |= F_VNODE_DVERS;
  }
  if (field_mask && !(field_mask & F_VNODE_AUTHOR)) {
    if (field_mask & F_VNODE_OWNER) v->author = v->owner;
    else v->author = 0;
    if (RV) fprintf(stderr, ">>> VNODE %d missing author (using %d)\n",
            v->vnode, v->author);
    v->field_mask |= F_VNODE_AUTHOR;
  }
  if (field_mask && !(field_mask & F_VNODE_OWNER)) {
    if (field_mask & F_VNODE_AUTHOR) v->owner = v->author;
    else v->owner = 0;
    if (RV) fprintf(stderr, ">>> VNODE %d missing owner (using %d)\n",
            v->vnode, v->owner);
    v->field_mask |= F_VNODE_OWNER;
  }
  if (field_mask && !(field_mask & F_VNODE_MODE)) {
    v->mode = (v->vnode & 1) ? 0755 : 0644;
    if (RV) fprintf(stderr, ">>> VNODE missing mode (using %d)\n", v->mode);
    v->field_mask |= F_VNODE_MODE;
  }
  if (field_mask && !(field_mask & F_VNODE_CDATE)) {
    if (field_mask & F_VNODE_SDATE) v->client_date = v->server_date;
    else v->client_date = 0;

    if (RV) fprintf(stderr, ">>> VNODE %d missing client date (using %d)\n",
            v->vnode, v->client_date);
    v->field_mask |= F_VNODE_CDATE;
  }
  if (field_mask && !(field_mask & F_VNODE_SDATE)) {
    if (field_mask & F_VNODE_CDATE) v->server_date = v->client_date;
    else v->server_date = 0;

    if (RV) fprintf(stderr, ">>> VNODE %d missing server date (using %d)\n",
            v->vnode, v->server_date);
    v->field_mask |= F_VNODE_SDATE;
  }
  if (field_mask && !(field_mask & F_VNODE_SIZE)) {
    if (RV) fprintf(stderr, ">>> VNODE %d has no data size (using 0)\n");
    v->size = 0;
    v->field_mask |= F_VNODE_SIZE;
  }
  if ((field_mask & F_VNODE_DATA) && !v->size) {
    if (RV)
      fprintf(stderr, ">>> VNODE %d has data, but size == 0 (ignoring)\n",
            v->vnode);
    v->field_mask &=~ F_VNODE_DATA;
  }
  if (field_mask && v->type == vDirectory && !(field_mask & F_VNODE_ACL)) {
    struct acl_accessList *acl = (struct acl_accessList *)v->acl;
    if (RV) {
      fprintf(stderr, ">>> VNODE %d is directory but has no ACL\n");
      fprintf(stderr, ">>> Will generate default ACL\n");
    }
    memset(v->acl, 0, SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE);
    acl->size = htonl(SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE);
    acl->version = htonl(ACL_ACLVERSION);
    acl->total = htonl(v->owner ? 0 : 1);
    acl->positive = acl->total;
    acl->negative = 0;
    if (v->owner) {
      acl->entries[0].id = htonl(v->owner);
      acl->entries[0].rights = htonl((PRSFS_READ   | PRSFS_WRITE
                                    | PRSFS_INSERT | PRSFS_LOOKUP
                                    | PRSFS_DELETE | PRSFS_LOCK
                                    | PRSFS_ADMINISTER));
    }
    v->field_mask |= F_VNODE_ACL;
  }

  r = DumpVNode(&repair_output, v);
  if (r) return r;

  if (v->size) {
    if (r = xfseek(X, &v->d_offset)) return r;
    r = CopyVNodeData(&repair_output, X, v->size);
  } else if (v->type == vDirectory) {
    afs_dir_page page;
    struct DirHeader *dhp = (struct DirHeader *)&page;
    int i;

    if (RV) {
      fprintf(stderr, ">>> VNODE %d is directory but has no contents\n");
      fprintf(stderr, ">>> Will generate deafult directory entries\n");
    }
    memset(&page, 0, sizeof(page));

    /* Page and Directory Headers */
    page.header.tag = htons(1234);
    page.header.freecount = (EPP - DHE - 3);
    page.header.freebitmap[0] = 0xff;
    page.header.freebitmap[1] = 0x7f;
    dhp->alloMap[0] = EPP - DHE - 3;
    for (i = 1; i < MAXPAGES; i++) dhp->alloMap[i] = EPP;

    /* Entry for . */
    page.entry[DHE + 1].flag    = FFIRST;
    page.entry[DHE + 1].length  = 1;
    page.entry[DHE + 1].vnode   = v->vnode;
    page.entry[DHE + 1].vunique = v->vuniq;
    strcpy(page.entry[DHE + 1].name, ".");
    dhp->hashTable[0x2e] = DHE + 1;

    /* Entry for .. */
    page.entry[DHE + 2].flag    = FFIRST;
    page.entry[DHE + 2].length  = 1;
    page.entry[DHE + 2].vnode   = v->parent;
    page.entry[DHE + 2].vunique = 1;         /* Can't have everything! */
    strcpy(page.entry[DHE + 2].name, "..");
    dhp->hashTable[0x44] = DHE + 2;

    r = DumpVNodeData(&repair_output, (char *)&page, 2048);
  } else if (field_mask) {
    /* We wrote out attributes, so we should also write the 0-length data */
    r = DumpVNodeData(&repair_output, "", 0);
  }

  return r;
}
