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

/* primitive.c - Routines for reading and writing low-level things */

#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "dumpscan.h"

#define BUFSIZE 256


afs_uint32 ReadByte(XFILE *X, unsigned char *val)
{
  return xfread(X, val, 1);
}

afs_uint32 ReadInt16(XFILE *X, afs_uint16 *val)
{
  afs_uint32 r;

  if (r = xfread(X, val, 2)) return r;
  *val = ntohs(*val);
  return 0;
}

afs_uint32 ReadInt32(XFILE *X, afs_uint32 *val)
{
  afs_uint32 r;

  if (r = xfread(X, val, 4)) return r;
  *val = ntohl(*val);
  return 0;
}

/* Read in a NUL-terminated string.  This method is kind of messy, but
 * has the advantage that it reads the data stream only once, doesn't
 * read anything extra, and never has to seek on the data stream.
 */
afs_uint32 ReadString(XFILE *X, unsigned char **val)
{
  static unsigned char buf[BUFSIZE];
  unsigned char *result = 0;
  afs_uint32 r;
  int i, l = 0;

  *val = 0;
  for (;;) {
    for (i = 0; i < BUFSIZE; i++) {
      r = ReadByte(X, buf + i);
      if (r) {
        if (result) free(result);
        return r;
      }
      if (!buf[i]) break;
    }
    /* iff we found a null, i < BUFSIZE and buf[i] holds the NUL */
    if (result) result = (unsigned char *)realloc(result, l + i + 1);
    else result = (unsigned char *)malloc(i + 1);
    if (!result) return ENOMEM;
    memcpy(result + l, buf, i);
    result[l+i] = 0;
    l += i;
    if (i < BUFSIZE) break;
  }
  *val = result;
  return 0;
}


afs_uint32 WriteByte(XFILE *X, unsigned char val)
{
  return xfwrite(X, &val, 1);
}

afs_uint32 WriteInt16(XFILE *X, afs_uint16 val)
{
  val = htons(val);
  return xfwrite(X, &val, 2);
}

afs_uint32 WriteInt32(XFILE *X, afs_uint32 val)
{
  val = htonl(val);
  return xfwrite(X, &val, 4);
}

afs_uint32 WriteString(XFILE *X, unsigned char *str)
{
  int len = strlen((char *)str) + 1;
  return xfwrite(X, str, len);
}

afs_uint32 WriteTagByte(XFILE *X, unsigned char tag, unsigned char val)
{
  char buffer[2];
  buffer[0] = tag;
  buffer[1] = val;
  return xfwrite(X, buffer, 2);
}

afs_uint32 WriteTagInt16(XFILE *X, unsigned char tag, afs_uint16 val)
{
  char buffer[3];
  buffer[0] = tag;
  buffer[1] = (val & 0xff00) >> 8;
  buffer[2] = val & 0xff;
  return xfwrite(X, buffer, 3);
}

afs_uint32 WriteTagInt32(XFILE *X, unsigned char tag, afs_uint32 val)
{
  char buffer[5];
  buffer[0] = tag;
  buffer[1] = (val & 0xff000000) >> 24;
  buffer[2] = (val & 0xff0000) >> 16;
  buffer[3] = (val & 0xff00) >> 8;
  buffer[4] = val & 0xff;
  return xfwrite(X, buffer, 5);
}

afs_uint32 WriteTagInt32Pair(XFILE *X, unsigned char tag,
                             afs_uint32 val1, afs_uint32 val2)
{
  char buffer[9];
  buffer[0] = tag;
  buffer[1] = (val1 & 0xff000000) >> 24;
  buffer[2] = (val1 & 0xff0000) >> 16;
  buffer[3] = (val1 & 0xff00) >> 8;
  buffer[4] = val1 & 0xff;
  buffer[5] = (val2 & 0xff000000) >> 24;
  buffer[6] = (val2 & 0xff0000) >> 16;
  buffer[7] = (val2 & 0xff00) >> 8;
  buffer[8] = val2 & 0xff;
  return xfwrite(X, buffer, 9);
}
