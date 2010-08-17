/*
 * CMUCS AFStools
 * dumpscan - routines for scanning and manipulating AFS volume dumps
 *
 * Copyright (c) 1998, 2001, 2004 Carnegie Mellon University
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

/* directory.c - AFS directory parsing and generation */
/* See the end of this file for a description of the directory format */

#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "dumpscan.h"
#include "dumpscan_errs.h"
#include "xf_errs.h"
#include "dumpfmt.h"
#include "internal.h"

#include <afs/dir.h>

struct dir_state {
  unsigned char **dirpages;
  int npages;

  afs_dir_header *dh;        /* Directory header */
  afs_dir_page *page;        /* Current page */
  int pageno;                /* Current page # */
  int entno;                 /* Current (next avail) entry # */
  int used;                  /* # entries used in this page */
};

static afs_dir_page page;

#define bmbyte(bm,x) bm[(x)>>3]
#define bmbit(x) (1 << ((x) & 7))

#define allocbit(x) (bmbyte(page.header.freebitmap,x) & bmbit(x))
#define setallocbit(bm,x) (bmbyte(bm,x) |= bmbit(x))

#define DPHE (DHE + 1)

/* Hash function used in AFS directories.  */
static int namehash(char *name, int buckets, int seed)
{
  int hval = seed, tval;

  while (*name) hval = (hval * 173) + *name++;
  tval = hval & (buckets - 1);
  return tval ? hval < 0 ? buckets - tval : tval : 0;
}

#if 0
static void fixup(char *name, int l)
{
  name += 16;
  l -= 15;

  while (l-- > 0) {
    name[0] = name[4];
    name++;
  }
}
#endif

afs_uint32 parse_directory(XFILE *X, dump_parser *p, afs_vnode *v,
                        afs_uint32 size, int toeof)
{
  afs_dir_entry de;
  int pgno, i, l, n;
  afs_int32 r;
  u_int64 where;

  if (p->print_flags & DSPRINT_DIR) {
    printf("  VNode      Uniqifier   Name\n");
    printf("  ========== ==========  ==============================\n");
  }
  if ((p->flags & DSFLAG_SEEK) && (r = xftell(X, &where))) return r;
  for (pgno = 0; toeof || size; pgno++, size -= (toeof ? 0 : AFS_PAGESIZE)) {
    if ((p->flags & DSFLAG_SEEK) && (r = xfseek(X, &where))) return r;
    if ((r = xfread(X, &page, AFS_PAGESIZE))) {
      if (toeof && r == ERROR_XFILE_EOF) break;
      return r;
    }
    if ((p->flags & DSFLAG_SEEK) && (r = xftell(X, &where))) return r;
    if (page.header.tag != htons(1234)) {
      if (p->cb_error)
        (p->cb_error)(DSERR_MAGIC, 1, p->err_refcon,
                      "Invalid page tag (%d) in page %d",
                      ntohs(page.header.tag), pgno);
      return DSERR_MAGIC;
    }
    for (i = (pgno ? 1 : DPHE); i < EPP; i++) {
      if (!allocbit(i)) continue;
      if (page.entry[i].flag != FFIRST) {
        if (p->cb_error)
          (p->cb_error)(DSERR_MAGIC, 0, p->err_refcon,
                        "Invalid entry flag %d in entry %d/%d; skipping...",
                        page.entry[i].flag, pgno, i);
        continue;
      }
      n = (EPP - i - 1) * 32 + 16;
      for (l = 0; n && page.entry[i].name[l]; l++, n--);
      if (page.entry[i].name[l]) {
        if (p->cb_error)
          (p->cb_error)(DSERR_FMT, 0, p->err_refcon,
                        "Filename too long in entry %d/%d; skipping page",
                        pgno, i);
        break;
      }
/*    fixup(page.entry[i].name, l); */
      if (pgno) de.slot = i - 1 + (pgno - 1) * (EPP - 1) + (EPP - DPHE);
      else de.slot = i - DPHE;
      de.name  = page.entry[i].name;
      de.vnode = ntohl(page.entry[i].vnode);
      de.uniq  = ntohl(page.entry[i].vunique);
      if (p->print_flags & DSPRINT_DIR)
        printf("  %10d %10d  %s\n", de.vnode, de.uniq, de.name);
      if (p->cb_dirent) {
        r = (p->cb_dirent)(v, &de, X, p->refcon);
      }
      if (p->cb_dirent && (r = (p->cb_dirent)(v, &de, X, p->refcon)))
        return r;
      i += ((l + 16) >> 5);
    }
  }
  if ((p->flags & DSFLAG_SEEK) && (r = xfseek(X, &where))) return r;
  return 0;
}


afs_uint32 ParseDirectory(XFILE *X, dump_parser *p, afs_uint32 size, int toeof)
{
  afs_uint32 r;

  r = parse_directory(X, p, 0, size, toeof);
  return r;
}


typedef struct {
  char **name;
  afs_uint32 *vnode;
  afs_uint32 *vuniq;
} dirlookup_stat;


static afs_uint32 dirlookup_cb(afs_vnode *v, afs_dir_entry *de,
                            XFILE *X, void *refcon)
{
  dirlookup_stat *s = (dirlookup_stat *)refcon;

  if (s->name && s->name[0]) {                  /* Search by filename */
    if (strcmp(de->name, s->name[0])) return 0; /* Not it! */
    if (s->vnode) s->vnode[0] = de->vnode;
    if (s->vuniq) s->vuniq[0] = de->uniq;
  } else if (s->vnode) {                        /* Search by vnode */
    if (de->vnode != s->vnode[0]) return 0;     /* Not it! */
    if (s->name) {
      s->name[0] = (char *)malloc(strlen(de->name) + 1);
      if (!s->name[0]) return ENOMEM;
      strcpy(s->name[0], de->name);
    }
    if (s->vuniq) s->vuniq[0] = de->uniq;
  }
  return DSERR_DONE;
}


/* Look up an entry in a directory, by name or vnode.
 * If *name is NULL, we are looking up by vnode.
 * Otherwise, we are looking for a filename.
 * In any event, any of name, vnode, vuniq that are
 * neither NULL nor the search key are filled in on
 * success.
 *
 * Call this with X pointing to the start of the directory,
 * and size set to the length of the directory.
 * Returns 0 on success, whether or not the entry is found.
 */
afs_uint32 DirectoryLookup(XFILE *X, dump_parser *p, afs_uint32 size,
                    char **name, afs_uint32 *vnode, afs_uint32 *vuniq)
{
  dump_parser my_p;
  dirlookup_stat my_s;
  afs_uint32 r;

  memset(&my_s, 0, sizeof(my_s));
  my_s.name  = name;
  my_s.vnode = vnode;
  my_s.vuniq = vuniq;

  memset(&my_p, 0, sizeof(my_p));
  my_p.refcon = (void *)&my_s;
  my_p.err_refcon = p->err_refcon;
  my_p.cb_error = p->cb_error;
  my_p.cb_dirent  = dirlookup_cb;

  r = parse_directory(X, &my_p, 0, size, 0);
  if (!r) r = DSERR_DONE;
  return handle_return(r, X, 0, p);
}


static int allocpage(struct dir_state *ds, int reserve)
{
  unsigned char **dirpages;
  int i;

  dirpages = malloc((ds->npages + 1) * sizeof(unsigned char *));
  if (!dirpages) return ENOMEM;
  if (ds->dirpages) {
    memcpy(dirpages, ds->dirpages, ds->npages * sizeof(unsigned char *));
    free(ds->dirpages);
  }
  ds->dirpages = dirpages;

  ds->dirpages[ds->npages] = malloc(AFS_PAGESIZE);
  if (!ds->dirpages[ds->npages]) return ENOMEM;
  ds->pageno = ds->npages++;

  ds->page = (afs_dir_page *)(ds->dirpages[ds->pageno]);
  memset(ds->page, 0, AFS_PAGESIZE);
  ds->page->header.tag = htons(AFS_DIR_MAGIC);
  ds->entno = ds->used = reserve;
  for (i = 0; i < reserve; i++)
    setallocbit(ds->page->header.freebitmap, i);
  return 0;
}


afs_uint32 Dir_Init(struct dir_state **dsp)
{
  afs_uint32 r;

  *dsp = malloc(sizeof(struct dir_state));
  if (!*dsp) return ENOMEM;
  memset(*dsp, 0, sizeof(struct dir_state));
  if ((r = allocpage(*dsp, DPHE))) return r;
  (*dsp)->dh = (afs_dir_header *)((*dsp)->page);
  return 0;
}


afs_uint32 Dir_AddEntry(struct dir_state *ds, char *name,
                        afs_uint32 vnode, afs_uint32 unique)
{
  afs_uint32 r;
  int l = strlen(name) + 1;
  int ne = l > 16 ? 1 + ((l - 16) / 32) + !!((l - 16) % 32) : 1;
  int hash = namehash(name, NHASHENT, 0);

  if (ne > EPP - 1) return ENAMETOOLONG;
  if (ds->entno + ne > EPP) {
    if (ds->pageno < 128) ds->dh->allomap[ds->pageno] = ds->used;
    if ((r = allocpage(ds, 1))) return r;
  }
  ds->page->entry[ds->entno].flag    = FFIRST;
  ds->page->entry[ds->entno].next    = ds->dh->hash[hash];
  ds->page->entry[ds->entno].vnode   = htonl(vnode);
  ds->page->entry[ds->entno].vunique = htonl(unique);
  strcpy(ds->page->entry[ds->entno].name, name);
  ds->dh->hash[hash] = htons((ds->pageno * EPP) + ds->entno);
  while (ne--) {
    setallocbit(ds->page->header.freebitmap, ds->entno);
    ds->used++;
    ds->entno++;
  }
  return 0;
}


afs_uint32 Dir_Finalize(struct dir_state *ds)
{
  int pages = ds->pageno + 1;

  if (ds->pageno < 128) ds->dh->allomap[ds->pageno] = ds->used;
  ds->dh->pagehdr.pgcount = htons(pages);
  return 0;
}


afs_uint32 Dir_EmitData(struct dir_state *ds, XFILE *X, int dotag)
{
  afs_uint32 r, size;
  int i;

  size = ds->npages * AFS_PAGESIZE;
  if (dotag && (r = WriteTagInt32(X, VTAG_DATA, size))) return r;
  for (i = 0; i < ds->npages; i++) {
    if ((r = xfwrite(X, ds->dirpages[i], AFS_PAGESIZE))) return r;
  }
  return 0;
}


afs_uint32 Dir_Free(struct dir_state *ds)
{
  int i;

  for (i = 0; i < ds->npages; i++)
    free(ds->dirpages[i]);
  free(ds->dirpages);
  free(ds);
  return 0;
}


/* AFS directory format:
 * AFS directories are stored in volume dumps in exactly the same format
 * that is used on disk, which makes them relatively easy to dump and restore,
 * but means we have to do some work to interpret them.
 *
 * The ACL for a directory is stored on disk in the last part of a "large"
 * (directory) vnode.  This part of the vnode, which has fixed size
 * SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE, is copied directly into
 * the dump file with a tag of 'A' (VTAG_ACL).  The structure of this
 * section is described in <afs/acl.h>.
 *
 * The name-to-vnode mappings are also stored exactly as they appear on
 * disk, using the file data ('f') attribute.  As usual, this attribute
 * consists of a 32-bit number containing the size, immediately followed
 * by the data itself.  The interesting structures and constants are
 * defined in <afs/dir.h>
 *
 * A directory consists of one or more 'pages', each of which is 2K
 * (AFS_PAGESIZE).  Each page contains EPP (currently 64) 'entries', each
 * of which is 32 bytes.  The first page begins with a DirHeader, which
 * is DHE entries long, and includes a PageHeader.  All other pages begin
 * with just a PageHeader, which is 1 entry long.  Every other entry is
 * a DirEntry, a DirXEntry (name extension), or unused.
 *
 * A Page Header contains the following elements:
 * - pgcount    contains a count of the number of pages in the directory,
 *              if the directory is new-style (>128 pages), or 0 if it is
 *              old-style.  This field is meaningful only in the Dir Header.
 * - tag        a magic number, which must be 1234
 * - freecount  apparently unused
 * - freebitmap A bitmap of free entries.  Each byte corresponds to 8
 *              entries, with the least significant bit referring to the
 *              first of those.  Each bit is set iff the corresponding
 *              entry is allocated.  Entries used by the page and dir
 *              headers are considered allocated.
 *
 * A Dir Header consists of a Page Header, followed by an allocation map
 * and hash table.  The allocation map contains one byte for each of the
 * first 128 pages; that byte contains the number of entries in that page
 * that are allocated.  Every page that actually exists has at peast one
 * entry allocated (the Page Header); if a byte in this map is 0, it means
 * that the page does not yet exist.
 *
 * Each bucket in the hash table is a linked list, using 'blob numbers'
 * as pointers.  A blob number is defined as (page# * EPP) + entry#.
 * The head of each chain is kept in the hash table, and the next pointers
 * are kept in the 'next' entry of each directory.
 *
 * Directory entries themselves contain the following elements:
 * - flag    Set to FFIRST iff this is the first blob in an entry
 *           (otherwise it will be a name continuation).  This is
 *           probably not reliable.
 * - length  Unused
 * - next    Pointer to the next element in this hash chain
 * - fid     FileID (vnode and uniquifier)
 * - name    Filename (null-terminated)
 */
