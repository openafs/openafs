/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1980, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <afs/param.h>

#if defined(AFS_DARWIN_ENV)
/*-----------------------------------------------------------------------
 * This version of fstab.c is intended to be used on Darwin systems to
 * replace getfsent() and family.  It has been modified so that rather
 * than read /etc/fstab, it calls getfsstat() to get the real list of
 * mounted volumes.
 *-----------------------------------------------------------------------*/

#include <errno.h>
#include <fstab.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>

#define	STDERR_FILENO	2

static struct fstab _fs_fstab;
static struct statfs *_fs_buf;
static struct statfs *_fs_ptr;
static int _fs_count;

static error __P((int));
static fstabscan __P((void));

static
fstabscan()
{
    if (_fs_count <= 0)
	return (0);
    _fs_fstab.fs_spec = _fs_ptr->f_mntfromname;
    _fs_fstab.fs_file = _fs_ptr->f_mntonname;
    _fs_fstab.fs_vfstype = _fs_ptr->f_fstypename;
    _fs_fstab.fs_mntops = _fs_ptr->f_fstypename;	// no mount options given
    _fs_fstab.fs_type = (_fs_ptr->f_flags & MNT_RDONLY) ? FSTAB_RO : FSTAB_RW;
    _fs_fstab.fs_freq = 0;
    _fs_fstab.fs_passno = 0;

    _fs_ptr++;
    _fs_count--;
    return (1);
}

struct fstab *
getfsent()
{
    if (!_fs_buf && !setfsent() || !fstabscan())
	return ((struct fstab *)NULL);
    return (&_fs_fstab);
}

struct fstab *
getfsspec(name)
     register const char *name;
{
    if (setfsent())
	while (fstabscan())
	    if (!strcmp(_fs_fstab.fs_spec, name))
		return (&_fs_fstab);
    return ((struct fstab *)NULL);
}

struct fstab *
getfsfile(name)
     register const char *name;
{
    if (setfsent())
	while (fstabscan())
	    if (!strcmp(_fs_fstab.fs_file, name))
		return (&_fs_fstab);
    return ((struct fstab *)NULL);
}

setfsent()
{
    long bufsize;

    if (_fs_buf) {
	free(_fs_buf);
	_fs_buf = NULL;
    }
    if ((_fs_count = getfsstat(NULL, 0, MNT_WAIT)) < 0) {
	error(errno);
	return (0);
    }
    bufsize = (long)_fs_count *sizeof(struct statfs);
    if ((_fs_buf = malloc(bufsize)) == NULL) {
	error(errno);
	return (0);
    }
    if (getfsstat(_fs_buf, bufsize, MNT_WAIT) < 0) {
	error(errno);
	return (0);
    }
    _fs_ptr = _fs_buf;
    return (1);
}

void
endfsent()
{
    if (_fs_buf) {
	free(_fs_buf);
	_fs_buf = NULL;
    }
    _fs_count = 0;
}

static
error(err)
     int err;
{
    char *p;

    (void)write(STDERR_FILENO, "fstab: ", 7);
    (void)write(STDERR_FILENO, _PATH_FSTAB, sizeof(_PATH_FSTAB) - 1);
    (void)write(STDERR_FILENO, ": ", 1);
    p = strerror(err);
    (void)write(STDERR_FILENO, p, strlen(p));
    (void)write(STDERR_FILENO, "\n", 1);
}
#endif /* defined(AFS_DARWIN_ENV) */
