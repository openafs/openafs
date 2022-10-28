/*
 * Copyright 2010, Sine Nomine Associates.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * libuafs SWIG interface file
 *
 * This file specifies the libuafs interfaces for SWIG, which can then be
 * used to easily create libuafs bindings to other languages such as Perl.
 *
 * For each language you want a binding for, there are two typemaps you
 * must define for that language, since SWIG does not handle them natively.
 * These are the 'in' typemap for READBUF and LENGTH, and the 'argout'
 * typemap for READBUF. Search this file for 'SWIGPERL' to see existing ones
 * for the Perl bindings.
 */

%module "AFS::ukernel"

%{
#include <afsconfig.h>
#include <afs/param.h>

#include <afs/afsutil.h>
#include <afs/sysincludes.h>
#include <afs_usrops.h>
#include <afs/cmd.h>
#include <afs/afs_args.h>
#include <afs/com_err.h>
%}

%include "typemaps.i";

%apply long { off_t };

%rename (uafs_ParseArgs) swig_uafs_ParseArgs;
%inline %{
/* SWIG doesn't handle argv-like string arrays too well, so instead have
 * a wrapper to convert a single string of all arguments into an argv-like
 * array. Conveniently, libcmd can do this.
 *
 * We could instead do this with SWIG typemaps, but writing this little
 * function instead is much more language-neutral. With typemaps, we'd have
 * to write the converting code for each target language.
 */
extern int
swig_uafs_ParseArgs(char *line)
{
    char *argv[1024];
    int argc;
    int code;

    code = cmd_ParseLine(line, argv, &argc, sizeof(argv)/sizeof(argv[0]));
    if (code) {
        afs_com_err("AFS::ukernel", code, "parsing line: '%s'", line);
        return code;
    }

    code = uafs_ParseArgs(argc, argv);

    cmd_FreeArgv(argv);

    return code;
}
%}
extern int uafs_Setup(const char *mount);
extern int uafs_Run(void);

/*
 * Define typemaps for binary read buffers. SWIG natively handles
 * NUL-terminated strings, but uafs_read could have NULs in the middle of the
 * string, so we need these typemaps to pay attention to the string length.
 *
 * (Reading in a binary buffer from e.g. uafs_write is already handled natively
 * by SWIG. Fancy that.)
 */
#if defined(SWIGPERL)
%typemap(in, numinputs=1) (char *READBUF, int LENGTH) {
    if (!SvIOK($input)) {
        SWIG_croak("expected an integer");
    }
    $2 = SvIV($input);
    Newx($1, $2, char);
}
%typemap(argout, numinputs=1) char *READBUF {
    /* some logic here copied from typemaps.i and/or SWIG itself, since I'm not
     * a perl dev */

    if (argvi >= items) {
        EXTEND(sp, 1);
    }

    /* 'result' is the return value from the actual C function call; we assume
     * it is an int that represents how many bytes of data were actually read into
     * the buffer if nonnegative */
    if (result < 0) {
        $result = &PL_sv_undef;
    } else {
        $result = sv_2mortal(newSVpvn($1, result));
    }

    Safefree($1);
    argvi++;
}
#else
# error No READBUF typemap defined for the given language
#endif

extern int uafs_mkdir(char *path, int mode);
extern int uafs_chdir(char *path);
extern int uafs_open(char *path, int flags, int mode=0);
extern int uafs_creat(char *path, int mode);
extern int uafs_write(int fd, char *STRING, int LENGTH);
extern int uafs_pwrite(int fd, char *STRING, int LENGTH, off_t offset);
extern int uafs_read(int fd, char *READBUF, int LENGTH);
extern int uafs_pread(int fd, char *READBUF, int LENGTH, off_t offset);
extern int uafs_fsync(int fd);
extern int uafs_close(int fd);

%{
#define STAT_TYPE long
#define STAT_ARGS        \
    STAT_TYPE *adev,     \
    STAT_TYPE *aino,     \
    STAT_TYPE *amode,    \
    STAT_TYPE *anlink,   \
    STAT_TYPE *auid,     \
    STAT_TYPE *agid,     \
    STAT_TYPE *ardev,    \
    STAT_TYPE *asize,    \
    STAT_TYPE *aatime,   \
    STAT_TYPE *amtime,   \
    STAT_TYPE *actime,   \
    STAT_TYPE *ablksize, \
    STAT_TYPE *ablocks

#define STAT_COPYFROM(st) do {   \
    *adev     = (st).st_dev;     \
    *aino     = (st).st_ino;     \
    *amode    = (st).st_mode;    \
    *anlink   = (st).st_nlink;   \
    *auid     = (st).st_uid;     \
    *agid     = (st).st_gid;     \
    *ardev    = (st).st_rdev;    \
    *asize    = (st).st_size;    \
    *aatime   = (st).st_atime;   \
    *amtime   = (st).st_mtime;   \
    *actime   = (st).st_ctime;   \
    *ablksize = (st).st_blksize; \
    *ablocks  = (st).st_blocks;  \
} while (0)

int
swig_uafs_stat(char *path, STAT_ARGS)
{
    int code;
    struct stat st;
    code = uafs_stat(path, &st);
    if (code == 0) {
        STAT_COPYFROM(st);
    }
    return code;
}
int
swig_uafs_lstat(char *path, STAT_ARGS)
{
    int code;
    struct stat st;
    code = uafs_lstat(path, &st);
    if (code == 0) {
        STAT_COPYFROM(st);
    }
    return code;
}
int
swig_uafs_fstat(int fd, STAT_ARGS)
{
    int code;
    struct stat st;
    code = uafs_fstat(fd, &st);
    if (code == 0) {
        STAT_COPYFROM(st);
    }
    return code;
}

#undef STAT_TYPE
#undef STAT_ARGS
#undef STAT_COPYFROM
%}

%define STAT_TYPE
long
%enddef
%define STAT_OUT_ARGS
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT,
STAT_TYPE *OUTPUT
%enddef

%rename (uafs_stat) swig_uafs_stat;
%rename (uafs_lstat) swig_uafs_lstat;
%rename (uafs_fstat) swig_uafs_fstat;

extern int swig_uafs_stat(char *path, STAT_OUT_ARGS);
extern int swig_uafs_lstat(char *path, STAT_OUT_ARGS);
extern int swig_uafs_fstat(int fd, STAT_OUT_ARGS);

extern int uafs_truncate(char *path, int len);
extern int uafs_ftruncate(int fd, int len);
extern int uafs_lseek(int fd, int offset, int whence);
extern int uafs_chmod(char *path, int mode);
extern int uafs_fchmod(int fd, int mode);
extern int uafs_symlink(char *target, char *source);
extern int uafs_unlink(char *path);
extern int uafs_rmdir(char *path);
extern int uafs_readlink(char *path, char *READBUF, int LENGTH);
extern int uafs_link(char *existing, char *new);
extern int uafs_rename(char *old, char *new);

extern usr_DIR *uafs_opendir(char *path);

%rename (uafs_readdir) swig_uafs_readdir;
%{
/*
 * Language-neutral wrapper for uafs_readdir. Since the language won't know
 * what to do with a struct usr_dirent, we could either make a SWIG typemap, or
 * define a wrapper with multiple return arguments. Making a typemap is
 * language-specific, so we define a wrapper, and let the typemaps.i library
 * worry about the language-specific parts of getting multiple return values.
 */
char *
swig_uafs_readdir(usr_DIR *dirp, unsigned long *d_ino, unsigned long *d_off, unsigned short *d_reclen)
{
    struct usr_dirent *dentry;

    dentry = uafs_readdir(dirp);
    if (!dentry) {
        *d_ino = *d_off = *d_reclen = 0;
        return NULL;
    }

    *d_ino = dentry->d_ino;
    *d_off = dentry->d_off;
    *d_reclen = dentry->d_reclen;

    return strdup(dentry->d_name);
}
%}

extern char * swig_uafs_readdir(usr_DIR *dirp, unsigned long *OUTPUT, unsigned long *OUTPUT, unsigned short *OUTPUT);
extern int uafs_closedir(usr_DIR * dirp);
extern void uafs_SetRxPort(int);
extern void uafs_Shutdown(void);
