/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_FILEUTIL_H
#define OPENAFS_FILEUTIL_H

/* File-oriented utility functions */

extern int
  renamefile(const char *oldname, const char *newname);

/* Path normalization routines */
#define FPN_FORWARD_SLASHES 1
#define FPN_BACK_SLASHES    2

extern void
  FilepathNormalizeEx(char *path, int slashType);

/* Just a wrapper for FilepathNormalizeEx(path, FPN_FORWARD_SLASHES); */
extern void
  FilepathNormalize(char *path);

/*
 * Data structure used to implement buffered I/O. We cannot
 * use fopen in the fileserver because the file descriptor
 * in the FILE structure only has 8 bits.
 */
typedef int BUFIO_FD;
#define BUFIO_INVALID_FD (-1)

#define BUFIO_BUFSIZE 4096

typedef struct {
    BUFIO_FD fd;
    int pos;
    int len;
    int eof;
    char buf[BUFIO_BUFSIZE];
} bufio_t, *bufio_p;

/* Open a file for buffered I/O */
extern bufio_p BufioOpen(char *path, int oflag, int mode);

/* Read the next line of a file */
extern int
  BufioGets(bufio_p bp, char *buf, int len);

/* Close a buffered I/O handle */
extern int
  BufioClose(bufio_p bp);

#endif /* OPENAFS_FILEUTIL_H */
