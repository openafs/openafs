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

/* xf_files.c - XFILE routines for accessing UNIX files */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "xfiles.h"
#include "xf_errs.h"

#define O_MODE_MASK (O_RDONLY | O_WRONLY | O_RDWR)


/* do_read for stdio xfiles */
static afs_uint32 xf_FILE_do_read(XFILE *X, void *buf, afs_uint32 count)
{
  FILE *F = X->refcon;

  /* XXX: handle short and interrupted reads */
  if (fread(buf, count, 1, F) != 1)
    return ferror(F) ? errno : ERROR_XFILE_EOF;
  return 0;
}


/* do_write for stdio xfiles */
static afs_uint32 xf_FILE_do_write(XFILE *X, void *buf, afs_uint32 count)
{
  FILE *F = X->refcon;

  /* XXX: handle interrupted writes */
  if (fwrite(buf, count, 1, F) != 1)
    return errno;
  return 0;
}


/* do_tell for stdio xfiles */
static afs_uint32 xf_FILE_do_tell(XFILE *X, u_int64 *offset)
{
  FILE *F = X->refcon;
  off_t where;

  where = ftell(F);
  if (where == -1) return errno;
  set64(*offset, where);
  return 0;
}


/* do_seek for stdio xfiles */
static afs_uint32 xf_FILE_do_seek(XFILE *X, u_int64 *offset)
{
  FILE *F = X->refcon;
  off_t where = get64(*offset);

  if (fseek(F, where, SEEK_SET) == -1) return errno;
  return 0;
}


/* do_skip for stdio xfiles */
static afs_uint32 xf_FILE_do_skip(XFILE *X, afs_uint32 count)
{
  FILE *F = X->refcon;

  if (fseek(F, count, SEEK_CUR) == -1) return errno;
  return 0;
}


/* do_close for stdio xfiles */
static afs_uint32 xf_FILE_do_close(XFILE *X)
{
  FILE *F = X->refcon;

  X->refcon = 0;
  if (fclose(F)) return errno;
  return 0;
}


/* Prepare a stdio XFILE */
static void prepare(XFILE *X, FILE *F, int xflag)
{
  struct stat st;

  memset(X, 0, sizeof(*X));
  X->do_read  = xf_FILE_do_read;
  X->do_write = xf_FILE_do_write;
  X->do_tell  = xf_FILE_do_tell;
  X->do_close = xf_FILE_do_close;
  X->refcon = F;
  if (xflag == O_RDWR) X->is_writable = 1;

  if (!fstat(fileno(F), &st)
  && ((st.st_mode & S_IFMT) == S_IFREG
  ||  (st.st_mode & S_IFMT) == S_IFBLK)) {
    X->is_seekable = 1;
    X->do_seek = xf_FILE_do_seek;
    X->do_skip = xf_FILE_do_skip;
  }
}


/* Open an XFILE by path */
afs_uint32 xfopen_path(XFILE *X, int flag, char *path, int mode)
{
  FILE *F = 0;
  int fd = -1, xflag;
  afs_uint32 code;

  xflag = flag & O_MODE_MASK;
  if (xflag == O_WRONLY) return ERROR_XFILE_WRONLY;

  if ((fd = open(path, flag, mode)) < 0) return errno;
  if (!(F = fdopen(fd, (xflag == O_RDONLY) ? "r" : "r+"))) {
    code = errno;
    close(fd);
    return code;
  }

  prepare(X, F, xflag);
  return 0;
}


/* Open an XFILE by FILE * */
afs_uint32 xfopen_FILE(XFILE *X, int flag, FILE *F)
{
  flag &= O_MODE_MASK;
  if (flag == O_WRONLY) return ERROR_XFILE_WRONLY;
  prepare(X, F, flag);
  return 0;
}


/* Open an XFILE by file descriptor */
afs_uint32 xfopen_fd(XFILE *X, int flag, int fd)
{
  FILE *F;

  flag &= O_MODE_MASK;
  if (flag == O_WRONLY) return ERROR_XFILE_WRONLY;
  if (!(F = fdopen(fd, (flag == O_RDONLY) ? "r" : "r+"))) return errno;
  prepare(X, F, flag);
  return 0;
}


/* open-by-name support for filenames */
afs_uint32 xfon_path(XFILE *X, int flag, char *name)
{
  return xfopen_path(X, flag, name, 0644);
}


/* open-by-name support for file descriptors */
afs_uint32 xfon_fd(XFILE *X, int flag, char *name)
{
  int fd = atoi(name);
  return xfopen_fd(X, flag, fd);
}


/* open-by-name support for standard I/O */
afs_uint32 xfon_stdio(XFILE *X, int flag)
{
  flag &= O_MODE_MASK;
  if (flag == O_WRONLY) flag = O_RDWR;
  return xfopen_FILE(X, flag, (flag == O_RDONLY) ? stdin : stdout);
}
