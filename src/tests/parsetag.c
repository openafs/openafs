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

/* parsetag.c - Parse a tagged data stream */

#include "dumpscan.h"
#include "dumpscan_errs.h"

/* If a parser function is defined, it will be called after the data value
 * (if any) is read.  The parser is called as follows:
 *
 *   parser(input_file, &tag, &field_rec, value, g_refcon, l_refcon);
 *
 * - input_file is the FILE * for the input stream
 * - field_rec is a pointer to the field record for the field just read
 * - g_refcon and l_refcon are as passed in to ParseTaggedData
 * - For integer types, value is the integer value
 * - For DKIND_STRING, tag is a pointer to the string just read
 * - For DKIND_SPEACH, tag is a pointer to the place to put the next tag.
 *
 * If the field type is DKIND_SPECIAL, the parser is expected to read its
 * own data from the input stream, and return when ParseTaggedData is supposed
 * to take over, with the next tag to process in *tag.  At no other time
 * should the parser read, write, or reposition the input stream.
 *
 * The parser routine should return 0 on success, non-0 on failure.  If the
 * data type is DKIND_STRING, the parser may return DSERR_KEEP to indicate
 * that the memory allocated for the value should not be freed.
 */

/* Parse a file containing tagged data and attributes **/
afs_uint32 ParseTaggedData(XFILE *X, tagged_field *fields, unsigned char *tag,
                    tag_parse_info *pi, void *g_refcon, void *l_refcon)
{
  int i = -1;
  afs_uint32 r, val;
  afs_uint16 val16;
  unsigned char val8;
  unsigned char *strval;

  for (;;) {
    if (i < 0 || (fields[i].kind & DKIND_MASK) != DKIND_SPECIAL) {
      /* Need to read in a tag */
      if (r = ReadByte(X, tag)) return r;
    }

    /* Simple error recovery - if we encounter a 0, it can never be
     * a valid tag.  If TPFLAG_SKIP is set, we can skip over any
     * such null bytes, and process whatever tag we find beyond.
     * In addition, if TPFLAG_RSKIP is set, then the next time
     * we encounter a 0, try skipping backwards.  That seems to
     * work much of the time.
     */
    if (!*tag && pi->shift_offset && (pi->flags & TPFLAG_RSKIP)) {
      u_int64 where, tmp64a, tmp64b;
      char buf1[21], buf2[21], buf3[21];
      char *p1, *p2, *p3;

      if (r = xftell(X, &tmp64a)) return r;
      sub64_32(where, tmp64a, pi->shift_offset + 1);
      if (r = xfseek(X, &where)) return r;
      if (pi->cb_error){
        (pi->cb_error)(DSERR_FMT, 0, pi->err_refcon,
                       "Inserted %d bytes before offset %d",
                       pi->shift_offset, decimate_int64(&where, 0));
        add64_32(tmp64a, pi->shift_start, pi->shift_offset);
        p1 = decimate_int64(&tmp64a, buf1);
        sub64_64(tmp64b, where, tmp64a);
        p2 = decimate_int64(&tmp64b, buf2);
        p3 = decimate_int64(&pi->shift_start, buf3);
        (pi->cb_error)(DSERR_FMT, 0, pi->err_refcon,
                       ">>> SHIFT start=%s length=%s target=%s",
                       p1, p2, p3);
      }
      pi->shift_offset = 0;
      if (r = ReadByte(X, tag)) return r;
    }
    if (!*tag && (pi->flags & TPFLAG_SKIP)) {
      int count = 0;
      u_int64 where, tmp64a;

      if (r = xftell(X, &where)) return r;
      
      while (!*tag) {
        if (r = ReadByte(X, tag)) return r;
        count++;
      }
      pi->shift_offset += count;
      cp64(pi->shift_start, where);
      if (pi->cb_error) {
        sub64_32(tmp64a, where, 1);
        (pi->cb_error)(DSERR_FMT, 0, pi->err_refcon,
                       "Skipped %d bytes at offset %s",
                       count, decimate_int64(&tmp64a, 0));
      }
    }

    for (i = 0; fields[i].tag && fields[i].tag != *tag; i++);
    if (!fields[i].tag) return 0;

    switch (fields[i].kind & DKIND_MASK) {
    case DKIND_NOOP:
      if (fields[i].func) {
        r = (fields[i].func)(X, 0, fields+i, 0, pi, g_refcon, l_refcon);
        if (r) return r;
      }
      break;

    case DKIND_BYTE:
      if (r = ReadByte(X, &val8)) return r;
      if (fields[i].func) {
        r = (fields[i].func)(X, 0, fields+i, val8, pi, g_refcon, l_refcon);
        if (r) return r;
      }
      break;

    case DKIND_INT16:
      if (r = ReadInt16(X, &val16)) return r;
      if (fields[i].func) {
        r = (fields[i].func)(X, 0, fields+i, val16, pi, g_refcon, l_refcon);
        if (r) return r;
      }
      break;

    case DKIND_INT32:
      if (r = ReadInt32(X, &val)) return r;
      if (fields[i].func) {
        r = (fields[i].func)(X, 0, fields+i, val, pi, g_refcon, l_refcon);
        if (r) return r;
      }
      break;

    case DKIND_STRING: 
      if (r = ReadString(X, &strval)) return r;
      if (fields[i].func) {
        r = (fields[i].func)(X, strval, fields+i, 0, pi, g_refcon, l_refcon);
        if (r != DSERR_KEEP) free(strval);
        if (r && r != DSERR_KEEP) return r;
      } else free(strval);
      break;

    case DKIND_SPECIAL:
      if (fields[i].func) {
        r = (fields[i].func)(X, tag, fields+i, 0, pi, g_refcon, l_refcon);
        if (r) return r;
      } else i = -1;
    }
  }
}
