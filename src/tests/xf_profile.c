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

/* xf_profile.c - XFILE routines for read/write profiling */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "xfiles.h"
#include "xf_errs.h"

#define O_MODE_MASK (O_RDONLY | O_WRONLY | O_RDWR)

typedef struct {
  XFILE content;
  XFILE profile;
} PFILE;


/* do_read for profiled xfiles */
static afs_uint32 xf_PROFILE_do_read(XFILE *X, void *buf, afs_uint32 count)
{
  PFILE *PF = X->refcon;
  afs_uint32 err;

  err = xfread(&PF->content, buf, count);
  xfprintf(&PF->profile, "R %ld =%ld\n", (long)count, (long)err);
  return err;
}


/* do_write for profiled xfiles */
static afs_uint32 xf_PROFILE_do_write(XFILE *X, void *buf, afs_uint32 count)
{
  PFILE *PF = X->refcon;
  afs_uint32 err;

  err = xfwrite(&PF->content, buf, count);
  xfprintf(&PF->profile, "W %ld =%ld\n", (long)count, (long)err);
  return err;
}


/* do_tell for profiled xfiles */
static afs_uint32 xf_PROFILE_do_tell(XFILE *X, u_int64 *offset)
{
  PFILE *PF = X->refcon;
  afs_uint32 err;

  err = xftell(&PF->content, offset);
  if (err) xfprintf(&PF->profile, "TELL ERR =%ld\n", (long)err);
  else     xfprintf(&PF->profile, "TELL %s =0\n", hexify_int64(offset, 0));
  return err;
}


/* do_seek for profiled xfiles */
static afs_uint32 xf_PROFILE_do_seek(XFILE *X, u_int64 *offset)
{
  PFILE *PF = X->refcon;
  afs_uint32 err;

  err = xfseek(&PF->content, offset);
  xfprintf(&PF->profile, "SEEK %s =%ld\n", hexify_int64(offset, 0), (long)err);
  return err;
}


/* do_skip for profiled xfiles */
static afs_uint32 xf_PROFILE_do_skip(XFILE *X, afs_uint32 count)
{
  PFILE *PF = X->refcon;
  afs_uint32 err;

  err = xfskip(&PF->content, count);
  xfprintf(&PF->profile, "SKIP %ld =%ld\n", (long)count, (long)err);
  return err;
}


/* do_close for profiled xfiles */
static afs_uint32 xf_PROFILE_do_close(XFILE *X)
{
  PFILE *PF = X->refcon;
  afs_uint32 err, err2;

  err = xfclose(&PF->content);
  err2 = xfclose(&PF->profile);
  free(PF);
  return err ? err : err2;
}


/* Open a profiled XFILE */
afs_uint32 xfopen_profile(XFILE *X, int flag, char *xname, char *profile)
{
  PFILE *PF;
  afs_uint32 err;

  PF = malloc(sizeof(*PF));
  if (!PF) return ENOMEM;
  memset(PF, 0, sizeof(*PF));

  err = xfopen(&PF->profile, O_RDWR|O_CREAT|O_TRUNC, profile);
  if (err) {
    free(PF);
    return err;
  }

  err = xfopen(&PF->content, flag, xname);
  if (err) {
    xfclose(&PF->profile);
    free(PF);
    return err;
  }

  memset(X, 0, sizeof(*X));
  X->refcon = PF;
  X->do_read  = xf_PROFILE_do_read;
  X->do_write = xf_PROFILE_do_write;
  X->do_tell  = xf_PROFILE_do_tell;
  X->do_close = xf_PROFILE_do_close;
  X->is_writable = PF->content.is_writable;
  if (PF->content.is_seekable) {
    X->is_seekable;
    X->do_seek  = xf_PROFILE_do_seek;
    X->do_skip  = xf_PROFILE_do_skip;
  }
  xfprintf(&PF->profile, "OPEN %s\n", xname);
  return 0;
}


afs_uint32 xfon_profile(XFILE *X, int flag, char *name)
{
  char *x, *profile, *xname;
  afs_uint32 err;

  if (!(name = strdup(name))) return ENOMEM;

  profile = "-";
  xname = name;
  for (x = name; *x; x++) {
    if (x[0] == ':' && x[1] == ':') {
      *x = 0;
      profile = name;
      xname = x + 2;
      break;
    }
  }
  if (!*name) profile = "-";
  err = xfopen_profile(X, flag, xname, profile);
  free(name);
  return err;
}
