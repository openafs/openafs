/* Copyright (C)  1998  Transarc Corporation.  All rights reserved.
 *
 */

#ifndef TRANSARC_FILEUTIL_H
#define TRANSARC_FILEUTIL_H

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
extern bufio_p
BufioOpen(char *path, int oflag, int mode);

/* Read the next line of a file */
extern int
BufioGets(bufio_p bp, char *buf, int len);

/* Close a buffered I/O handle */
extern int
BufioClose(bufio_p bp);

#endif /* TRANSARC_FILEUTIL_H */
