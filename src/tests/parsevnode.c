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

/* parsevnode.c - Parse a VNode */

#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>

#include "dumpscan.h"
#include "dumpscan_errs.h"
#include "dumpfmt.h"
#include "internal.h"

#include <afs/acl.h>
#include <afs/prs_fs.h>

static afs_uint32 LastGoodVNode = 0;
static afs_uint32 store_vnode(XFILE *, unsigned char *, tagged_field *, afs_uint32,
                           tag_parse_info *, void *, void *);
static afs_uint32 parse_acl  (XFILE *, unsigned char *, tagged_field *, afs_uint32,
                           tag_parse_info *, void *, void *);
static afs_uint32 parse_vdata(XFILE *, unsigned char *, tagged_field *, afs_uint32,
                           tag_parse_info *, void *, void *);

/** Field list for vnodes **/
static tagged_field vnode_fields[] = {
  { VTAG_TYPE,        DKIND_BYTE,    " VNode type:   ", store_vnode, 0, 0 },
  { VTAG_NLINKS,      DKIND_INT16,   " Link count:   ", store_vnode, 0, 0 },
  { VTAG_DVERS,       DKIND_INT32,   " Version:      ", store_vnode, 0, 0 },
  { VTAG_CLIENT_DATE, DKIND_TIME,    " Server Date:  ", store_vnode, 0, 0 },
  { VTAG_AUTHOR,      DKIND_INT32,   " Author:       ", store_vnode, 0, 0 },
  { VTAG_OWNER,       DKIND_INT32,   " Owner:        ", store_vnode, 0, 0 },
  { VTAG_GROUP,       DKIND_INT32,   " Group:        ", store_vnode, 0, 0 },
  { VTAG_MODE,        DKIND_INT16,   " UNIX mode:    ", store_vnode, 0, 0 },
  { VTAG_PARENT,      DKIND_INT32,   " Parent:       ", store_vnode, 0, 0 },
  { VTAG_SERVER_DATE, DKIND_TIME,    " Client Date:  ", store_vnode, 0, 0 },
  { VTAG_ACL,         DKIND_SPECIAL, " xxxxxxxx ACL: ", parse_acl,   0, 0 },
  { VTAG_DATA,        DKIND_SPECIAL, " Contents:     ", parse_vdata, 0, 0 },
  { 0,0,0,0,0,0 }};


static afs_uint32 resync_vnode(XFILE *X, dump_parser *p, afs_vnode *v,
                            int start, int limit)
{
  u_int64 where, expected_where;
  afs_uint32 r;
  int i;

  if (r = xftell(X, &expected_where)) return r;
  cp64(where, expected_where);

  r = match_next_vnode(X, p, &where, v->vnode);
  if (r && r != DSERR_FMT) return r;
  if (r) for (i = -start; i < limit; i++) {
    add64_32(where, expected_where, i);
    r = match_next_vnode(X, p, &where, v->vnode);
    if (!r) break;
    if (r != DSERR_FMT) return r;
  }
  if (r) {
    if (p->cb_error)
      (p->cb_error)(r, 1, p->err_refcon,
                    "Unable to resync after vnode %d [%s = 0x%s]",
                    v->vnode, decimate_int64(&expected_where, 0),
                    hexify_int64(&expected_where, 0));
    return r;
  }
  if (ne64(where, expected_where) && p->cb_error) {
    (p->cb_error)(DSERR_FMT, 0, p->err_refcon,
                  "Vnode after %d not in expected location",
                  v->vnode);
    (p->cb_error)(DSERR_FMT, 0, p->err_refcon, "Expected location: %s = 0x%s",
                  decimate_int64(&expected_where, 0),
                  hexify_int64(&expected_where, 0));
    (p->cb_error)(DSERR_FMT, 0, p->err_refcon, "Actual location: %s = 0x%s",
                  decimate_int64(&where, 0), hexify_int64(&where, 0));
  }
  return xfseek(X, &where);
}


/* Parse a VNode, including any tagged attributes and data, and call the
 * appropriate callback, if one is defined.
 */
afs_uint32 parse_vnode(XFILE *X, unsigned char *tag, tagged_field *field,
                    afs_uint32 value, tag_parse_info *pi,
                    void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_uint32 (*cb)(afs_vnode *, XFILE *, void *);
  u_int64 where, offset2k;
  afs_vnode v;
  afs_uint32 r;


  if (r = xftell(X, &where)) return r;
  memset(&v, 0, sizeof(v));
  sub64_32(v.offset, where, 1);
  if (r = ReadInt32(X, &v.vnode)) return r;
  if (r = ReadInt32(X, &v.vuniq)) return r;

  mk64(offset2k, 0, 2048);
  if (!LastGoodVNode
  || ((p->flags & DSFLAG_SEEK) && v.vnode == 1
       && lt64(v.offset, offset2k)))
    LastGoodVNode = -1;

  if (p->print_flags & DSPRINT_ITEM) {
    printf("%s %d/%d [%s = 0x%s]\n", field->label, v.vnode, v.vuniq,
           decimate_int64(&where, 0), hexify_int64(&where, 0));
  }

  r = ParseTaggedData(X, vnode_fields, tag, pi, g_refcon, (void *)&v);

  /* Try to resync, if requested */
  if (!r && (p->repair_flags & DSFIX_VFSYNC)) {
    afs_uint32 drop;
    u_int64 xwhere;

    if (r = xftell(X, &where)) return r;
    sub64_32(xwhere, where, 1);

    /* Are we at the start of a valid vnode (or dump end)? */
    r = match_next_vnode(X, p, &xwhere, v.vnode);
    if (r && r != DSERR_FMT) return r;
    if (r) { /* Nope. */
      /* Was _this_ a valid vnode?  If so, we can keep it and search for
       * the next one.  Otherwise, we throw it out, and start the search
       * at the starting point of this vnode.
       */
      drop = r = match_next_vnode(X, p, &v.offset, LastGoodVNode);
      if (r && r != DSERR_FMT) return r;
      if (!r) {
        add64_32(where, v.offset, 1);
        if (r = xfseek(X, &v.offset)) return r;
      } else {
        if (r = xfseek(X, &xwhere)) return r;
      }
      if (r = resync_vnode(X, p, &v, 0, 1024)) return r;
      if (r = ReadByte(X, tag)) return r;
      if (drop) {
        if (p->cb_error)
          (p->cb_error)(DSERR_FMT, 0, p->err_refcon,
                        "Dropping vnode %d", v.vnode);
        return 0;
      }
    } else {
      if (r = xfseek(X, &where)) return r;
    }
  }
  LastGoodVNode = v.vnode;

  if (!r) {
    if (v.field_mask & F_VNODE_TYPE)
      switch (v.type) {
      case vFile:      cb = p->cb_vnode_file;  break;
      case vDirectory: cb = p->cb_vnode_dir;   break;
      case vSymlink:   cb = p->cb_vnode_link;  break;
      default:         cb = p->cb_vnode_wierd; break;
      }
    else               cb = p->cb_vnode_empty;
    if (cb) {
      u_int64 where;

      if (r = xftell(X, &where)) return r;
      r = (cb)(&v, X, p->refcon);
      if (p->flags & DSFLAG_SEEK) {
        if (!r) r = xfseek(X, &where);
        else xfseek(X, &where);
      }
    }
  }
  return r;
}


/* Store data in a vnode */
static afs_uint32 store_vnode(XFILE *X, unsigned char *tag, tagged_field *field,
                           afs_uint32 value, tag_parse_info *pi,
                           void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_vnode *v = (afs_vnode *)l_refcon;
  time_t when;
  afs_uint32 r = 0;

  switch (field->tag) {
  case VTAG_TYPE:
    v->field_mask |= F_VNODE_TYPE;
    v->type = value;
    if (p->print_flags & DSPRINT_VNODE) {
      switch (value) {
      case vFile:
        printf("%sFile (%d)\n", field->label, value);
        break;
      case vDirectory:
        printf("%sDirectory (%d)\n", field->label, value);
        break;
      case vSymlink:
        printf("%sSymbolic Link (%d)\n", field->label, value);
        break;
      default:
        printf("%s??? (%d)\n", field->label, value);
      }
      return r;
    }
    break;

  case VTAG_NLINKS:
    v->field_mask |= F_VNODE_NLINKS;
    v->nlinks = value;
    break;

  case VTAG_DVERS:
    v->field_mask |= F_VNODE_DVERS;
    v->datavers = value;
    break;

  case VTAG_CLIENT_DATE:
    v->field_mask |= F_VNODE_CDATE;
    v->client_date = value;
    break;

  case VTAG_SERVER_DATE:
    v->field_mask |= F_VNODE_SDATE;
    v->server_date = value;
    break;

  case VTAG_AUTHOR:
    v->field_mask |= F_VNODE_AUTHOR;
    v->author = value;
    break;

  case VTAG_OWNER:
    v->field_mask |= F_VNODE_OWNER;
    v->owner = value;
    break;

  case VTAG_GROUP:
    v->field_mask |= F_VNODE_GROUP;
    v->group = value;
    break;

  case VTAG_MODE:
    v->field_mask |= F_VNODE_MODE;
    v->mode = value;
    break;

  case VTAG_PARENT:
    v->field_mask |= F_VNODE_PARENT;
    v->parent = value;
    break;
  }

  if (p->print_flags & DSPRINT_VNODE)
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


static char *rights2str(afs_uint32 rights)
{
  static char str[16];
  char *p = str;

  if (rights & PRSFS_READ)       *p++ = 'r';
  if (rights & PRSFS_LOOKUP)     *p++ = 'l';
  if (rights & PRSFS_INSERT)     *p++ = 'i';
  if (rights & PRSFS_DELETE)     *p++ = 'd';
  if (rights & PRSFS_WRITE)      *p++ = 'w';
  if (rights & PRSFS_LOCK)       *p++ = 'k';
  if (rights & PRSFS_ADMINISTER) *p++ = 'a';
  if (rights & PRSFS_USR0)       *p++ = 'A';
  if (rights & PRSFS_USR1)       *p++ = 'B';
  if (rights & PRSFS_USR2)       *p++ = 'C';
  if (rights & PRSFS_USR3)       *p++ = 'D';
  if (rights & PRSFS_USR4)       *p++ = 'E';
  if (rights & PRSFS_USR5)       *p++ = 'F';
  if (rights & PRSFS_USR6)       *p++ = 'G';
  if (rights & PRSFS_USR7)       *p++ = 'H';

  *p = 0;
  if (!str[0]) strcpy(str, "none");
  return str;
}


/* Parse and store the ACL data from a directory vnode */
static afs_uint32 parse_acl(XFILE *X, unsigned char *tag, tagged_field *field,
                         afs_uint32 value, tag_parse_info *pi,
                         void *g_refcon, void *l_refcon)
{
  struct acl_accessList *acl;
  dump_parser *p = (dump_parser *)g_refcon;
  afs_vnode *v = (afs_vnode *)l_refcon;
  afs_uint32 r, i, n;

  if (r = xfread(X, v->acl, SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE))
    return r;

  v->field_mask |= F_VNODE_ACL;
  if (p->print_flags & DSPRINT_ACL) {
    acl = (struct acl_accessList *)(v->acl);
    n = ntohl(acl->positive);
    if (n) {
      printf("Positive ACL: %d entries\n", n);
      for (i = 0; i < n; i++)
        printf("              %9d  %s\n",
               ntohl(acl->entries[i].id),
               rights2str(acl->entries[i].rights));
    }
    n = ntohl(acl->negative);
    if (n) {
      printf("Positive ACL: %d entries\n", n);
      for (i = ntohl(acl->positive); i < ntohl(acl->total); i++)
        printf("              %9d  %s\n",
               ntohl(acl->entries[i].id),
               rights2str(acl->entries[i].rights));
    }
  }
  return ReadByte(X, tag);
}


/* Parse or skip over the vnode data */
static afs_uint32 parse_vdata(XFILE *X, unsigned char *tag, tagged_field *field,
                           afs_uint32 value, tag_parse_info *pi,
                           void *g_refcon, void *l_refcon)
{
  dump_parser *p = (dump_parser *)g_refcon;
  afs_vnode *v = (afs_vnode *)l_refcon;
  static char *symlink_buf = 0;
  static int symlink_size = 0;
  afs_uint32 r;

  if (r = ReadInt32(X, &v->size)) return r;
  v->field_mask |= F_VNODE_SIZE;

  if (v->size) {
    v->field_mask |= F_VNODE_DATA;
    if (r = xftell(X, &v->d_offset)) return r;
    if (p->print_flags & DSPRINT_VNODE)
      printf("%s%d (0x%08x) bytes at %s (0x%s)\n", field->label,
             v->size, v->size, decimate_int64(&v->d_offset, 0),
             hexify_int64(&v->d_offset, 0));
    
    switch (v->type) {
    case vSymlink:
      if (v->size > symlink_size) {
        if (symlink_buf) symlink_buf = (char *)realloc(symlink_buf, v->size + 1);
        else symlink_buf = (char *)malloc(v->size + 1);
        symlink_size = symlink_buf ? v->size : 0;
      }
      if (symlink_buf) {
        if (r = xfread(X, symlink_buf, v->size)) return r;
        symlink_buf[v->size] = 0;
        if (p->print_flags & DSPRINT_VNODE)
          printf("Target:       %s\n", symlink_buf);
      } else {
        /* Call the callback here, because it's non-fatal */
        if (p->cb_error)
          (p->cb_error)(ENOMEM, 0, p->err_refcon,
                        "Out of memory reading symlink");
        if (r = xfskip(X, v->size)) return r;
      }
      break;

    case vDirectory:
      if (p->cb_dirent || (p->print_flags & DSPRINT_DIR)) {
        if (r = parse_directory(X, p, v, v->size, 0)) return r;
        break;
      }

    default:
      if (r = xfskip(X, v->size)) return r;
    }
  } else if (p->print_flags & DSPRINT_VNODE) {
    printf("%sEmpty\n", field->label);
  }
  if (p->repair_flags & DSFIX_VDSYNC) {
    r = resync_vnode(X, p, v, 10, 15);
    if (r) return r;
  }
  return ReadByte(X, tag);
}
