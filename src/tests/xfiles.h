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

/* xfiles.h - Type, constant, and function declarations for
 * extensible file-like things */

#ifndef _XFILES_H_
#define _XFILES_H_

#include <stdio.h>
#include <stdarg.h>
#include "intNN.h"

struct rx_call;
struct rx_connection;

/* The XFILE structure */
typedef struct XFILE XFILE;
struct XFILE {
  afs_uint32 (*do_read)(XFILE *, void *, afs_uint32);   /* read data */
  afs_uint32 (*do_write)(XFILE *, void *, afs_uint32);  /* write data */
  afs_uint32 (*do_tell)(XFILE *, u_int64 *);         /* find position */
  afs_uint32 (*do_seek)(XFILE *, u_int64 *);         /* set position */
  afs_uint32 (*do_skip)(XFILE *, afs_uint32);           /* skip forward */
  afs_uint32 (*do_close)(XFILE *);                   /* close */
  u_int64 filepos;                                /* position (counted) */
  int is_seekable;                                /* 1 if seek works */
  int is_writable;                                /* 1 if write works */
  XFILE *passthru;                                /* XFILE to pass thru to */
  void *refcon;                                   /* type-specific data */
};


/* Functions for opening XFILEs.  For these, the first two arguments are
 * always a pointer to an XFILE to fill in, and the mode in which to
 * open the file.  O_RDONLY and O_RDWR are permitted; O_WRONLY is not.
 * Other open modes may or may not be used, depending on the object type.
 * Remaining arguments are a function of the object type
 */
extern afs_uint32 xfopen     (XFILE *, int, char *);      /* open by TYPE:name */
extern afs_uint32 xfopen_path(XFILE *, int, char *, int); /* open by path   */
extern afs_uint32 xfopen_FILE(XFILE *, int, FILE *);      /* open by FILE * */
extern afs_uint32 xfopen_fd  (XFILE *, int, int);         /* open by fd     */
extern afs_uint32 xfopen_rxcall (XFILE *, int, struct rx_call *);
extern afs_uint32 xfopen_voldump(XFILE *, struct rx_connection *,
                              afs_int32, afs_int32, afs_int32);
extern afs_uint32 xfopen_profile(XFILE *, int, char *, char *);

extern afs_uint32 xfregister(char *, afs_uint32 (*)(XFILE *, int, char *));

/* Standard operations on XFILEs */
extern afs_uint32 xfread(XFILE *, void *, afs_uint32);     /* read data */
extern afs_uint32 xfwrite(XFILE *, void *, afs_uint32);    /* write data */
extern afs_uint32 xfprintf(XFILE *, char *, ...);          /* formatted */
extern afs_uint32 vxfprintf(XFILE *, char *, va_list);     /* formatted VA */
extern afs_uint32 xftell(XFILE *, u_int64 *);              /* get position */
extern afs_uint32 xfseek(XFILE *, u_int64 *);              /* set position */
extern afs_uint32 xfskip(XFILE *, afs_uint32);             /* skip forward */
extern afs_uint32 xfpass(XFILE *, XFILE *);                /* set passthru */
extern afs_uint32 xfunpass(XFILE *);                       /* unset passthru */
extern afs_uint32 xfclose(XFILE *);                        /* close */

#endif /* _XFILES_H_ */
