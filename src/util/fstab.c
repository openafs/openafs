/*-
 * Copyright (c) 1980, 1989, 1993
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

#include <afsconfig.h>
#include <afs/param.h>

#if defined(AFS_DARWIN_ENV)
/*
 * Reworked from FreeBSD umount
 */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>

size_t
mntinfo(struct statfs **mntbuffer)
{
    static struct statfs *origbuf;
    size_t bufsize;
    int mntsize;

    mntsize = getfsstat(NULL, 0, MNT_NOWAIT);
    if (mntsize <= 0)
	return (0);
    bufsize = (mntsize + 1) * sizeof(struct statfs);
    if ((origbuf = malloc(bufsize)) == NULL)
	err(1, "malloc");
    mntsize = getfsstat(origbuf, (long)bufsize, MNT_NOWAIT);
    *mntbuffer = origbuf;
    return (mntsize);
}

static struct statfs *mntbuf = (struct statfs *)NULL;
static struct statfs *mntent;
static int mntcnt;
static struct fstab fstabent;

struct fstab *
getfsent(void)
{
    if (((!mntbuf) && !setfsent()) || mntcnt == 0)
	return ((struct fstab *)NULL);
    fstabent.fs_file = mntent->f_mntonname;
    fstabent.fs_freq = fstabent.fs_passno = 0;
    fstabent.fs_mntops = mntent->f_fstypename;
    fstabent.fs_spec = mntent->f_mntfromname;
    if (mntent->f_flags & MNT_RDONLY)
	fstabent.fs_type = FSTAB_RO;
    else
	fstabent.fs_type = FSTAB_RW;
    fstabent.fs_vfstype = mntent->f_fstypename;

    mntent++;
    mntcnt--;
    return &fstabent;
}

int
setfsent(void)
{
    if (mntbuf)
	free(mntbuf);
    mntbuf = NULL;
    mntent = 0;
    if ((mntcnt = mntinfo(&mntbuf)) <= 0) {
	err(1, "setfsent");
	return 0;
    }
    mntent = mntbuf;
    return 1;
}

void
endfsent(void)
{
    if (mntbuf)
	free(mntbuf);
    mntbuf = NULL;
    mntent = 0;
    mntcnt = 0;
}
#endif
