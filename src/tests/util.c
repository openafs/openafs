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

/* util.c - Useful utilities */

#include <errno.h>

#include "xf_errs.h"
#include "dumpscan.h"
#include "dumpscan_errs.h"
#include "dumpfmt.h"


/* Take care of errno, ERROR_XFILE_EOF, and ENOMEM return codes.
 * Call whatever callbacks are necessary, and return the code to
 * actually use.  If you don't want '0' to result in a DSERR_TAG,
 * then you must translate it to DSERR_DONE before calling this.
 */
/*** THIS FUNCTION INTENDED FOR INTERNAL USE ONLY ***/
int handle_return(int r, XFILE *X, unsigned char tag, dump_parser *p)
{
  u_int64 where, xwhere;

  switch (r) {
  case 0:
    if (p->cb_error) {
      xftell(X, &where);
      sub64_32(xwhere, where, 1);
      (p->cb_error)(DSERR_TAG, 1, p->err_refcon,
                    (tag > 0x20 && tag < 0x7f)
                    ? "Unexpected tag '%c' at %s = 0x%s"
                    : "Unexpected tag 0x%02x at %s = 0x%s",
                    tag, decimate_int64(&xwhere, 0), hexify_int64(&xwhere, 0));
    }
    return DSERR_TAG;
    
  case ERROR_XFILE_EOF:
    if (p->cb_error) {
      xftell(X, &where);
      (p->cb_error)(ERROR_XFILE_EOF, 1, p->err_refcon,
                    "Unexpected EOF at %s = 0x%s",
                    decimate_int64(&where, 0), hexify_int64(&where, 0));
    }
    return ERROR_XFILE_EOF;
    
  case ENOMEM:
    if (p->cb_error) {
      xftell(X, &where);
      (p->cb_error)(ENOMEM, 1, p->err_refcon,
                    "Out of memory at %s = 0x%s",
                    decimate_int64(&where, 0), hexify_int64(&where, 0));
    }
    return ENOMEM;
    
  case DSERR_DONE:
    return 0;

  default:
    /* For other negative valuees, the callback was already done */
    if (r > 0 && p->cb_error)
      (p->cb_error)(r, 1, p->err_refcon,
                    "System error %d reading dump file", r);
    return r;
  }
}


/* Prepare a tag_parse_info for use by the dump parser. *
/*** THIS FUNCTION INTENDED FOR INTERNAL USE ONLY ***/
void prep_pi(dump_parser *p, tag_parse_info *pi)
{
  memset(pi, 0, sizeof(tag_parse_info));
  pi->err_refcon = p->err_refcon;
  pi->cb_error = p->cb_error;

  if (p->repair_flags & DSFIX_SKIP)
    pi->flags |= TPFLAG_SKIP;
  if ((p->flags & DSFLAG_SEEK) && (p->repair_flags & DSFIX_RSKIP))
    pi->flags |= TPFLAG_RSKIP;
}


/* Does the designated location match a vnode?
 * Returns 0 if yes, DSERR_FMT if no, something else on error
 */
/*** THIS FUNCTION INTENDED FOR INTERNAL USE ONLY ***/
int match_next_vnode(XFILE *X, dump_parser *p, u_int64 *where, afs_uint32 vnode)
{
  afs_uint32 r, x, y, z;
  unsigned char tag;

  if (r = xfseek(X, where)) return r;
  if (r = ReadByte(X, &tag)) return r;
  switch (tag) {
  case 3:  /* A vnode? */
    if (r = ReadInt32(X, &x)) return r;
    if (r = ReadInt32(X, &y)) return r;
    if (r = ReadByte(X, &tag)) return r;
    if ( !((vnode & 1) && !(x & 1) && x < vnode)
    &&   !((vnode & 1) == (x & 1) && x > vnode))
      return DSERR_FMT;
    if (x > vnode && x - vnode > 10000) return DSERR_FMT;
    if (y < 0 || y > p->vol_uniquifier)  return DSERR_FMT;

    /* Now, what follows the vnode/uniquifier? */
    switch (tag) {
    case 3:   /* Another vnode? - Only if this is a non-directory */
      if (x & 1) return DSERR_FMT;
      if (r = ReadInt32(X, &z)) return r;
      if ( !((x & 1) && !(z & 1) && z < x)
      &&   !((x & 1) == (z & 1) && z > x))
        return DSERR_FMT;
      return 0;

    case 4:   /* Dump end - Only if this is a non-directory */
      if (x & 1) return DSERR_FMT;
      if (r = ReadInt32(X, &z)) return r;
      if (z != DUMPENDMAGIC) return DSERR_FMT;
      return 0;

    case 't': /* Vnode type byte */
      if (r = ReadByte(X, &tag)) return r;
      if ((tag == vFile || tag == vSymlink) && !(x & 1)) return 0;
      if (tag == vDirectory && (x & 1)) return 0;
      return DSERR_FMT;

    default:
      return DSERR_FMT;
    }

  case 4:  /* A dump end? */
    if (r = ReadInt32(X, &x)) return r;
    if (x != DUMPENDMAGIC) return DSERR_FMT;
    return 0;

  default:
    return DSERR_FMT;
  }
}
