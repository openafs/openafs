/*
 * Copyright (c) 1998 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <afs/stds.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/afsint.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>

enum { PIOCTL_MAXSIZE = 2000 };

struct VenusFid {
    afs_int32 Cell;
    struct AFSFid Fid;
};

/*
 * fs_getfid, the the `fid' that `path' points on. 
 */

int
fs_getfid(char *path, struct VenusFid *fid)
{
    struct ViceIoctl a_params;

    if (path == NULL || fid == NULL)
	return EINVAL;

    a_params.in_size = 0;
    a_params.out_size = sizeof(struct VenusFid);
    a_params.in = NULL;
    a_params.out = (void *)fid;

    if (pioctl(path, VIOCGETFID, &a_params, 1) == -1)
	return errno;

    return 0;
}

/*
 * Do nothing
 */

int
fs_nop(void)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = 0;
    a_params.in = NULL;
    a_params.out = NULL;

    if (pioctl(NULL, VIOCNOP, &a_params, 1) == -1)
	return errno;

    return 0;
}

/*
 * Get the `cell' that the `path' ends up in
 */

int
fs_getfilecellname(char *path, char *cell, size_t len)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = len;
    a_params.in = NULL;
    a_params.out = cell;

    if (pioctl(path, VIOC_FILE_CELL_NAME, &a_params, 1) == -1)
	return errno;

    return 0;
}

/*
 * set the level of crypt
 */

#ifdef VIOC_SETRXKCRYPT
int
fs_setcrypt(afs_uint32 n)
{
    struct ViceIoctl a_params;

    a_params.in_size = sizeof(n);
    a_params.out_size = 0;
    a_params.in = (char *)&n;
    a_params.out = NULL;

    if (pioctl(NULL, VIOC_SETRXKCRYPT, &a_params, 0) == -1)
	return errno;

    return 0;
}
#endif

/*
 * get currernt level of crypt
 */

#ifdef VIOC_GETRXKCRYPT
int
fs_getcrypt(afs_uint32 * level)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = sizeof(*level);
    a_params.in = NULL;
    a_params.out = (char *)level;

    if (pioctl(NULL, VIOC_GETRXKCRYPT, &a_params, 0) == -1)
	return errno;

    return 0;
}
#endif

/*
 * get and set the connect-mode
 */

#ifdef VIOCCONNECTMODE
int
fs_connect(afs_int32 type, afs_int32 * flags)
{
    struct ViceIoctl a_params;

    a_params.in_size = sizeof(type);
    a_params.out_size = sizeof(afs_int32);
    a_params.in = (char *)&type;
    a_params.out = (char *)flags;

    if (pioctl(NULL, VIOCCONNECTMODE, &a_params, 0) == -1)
	return errno;

    return 0;
}
#endif

/*
 *
 */

#ifdef VIOC_FPRIOSTATUS
int
fs_setfprio(struct VenusFid fid, int16_t prio)
{
    struct ViceIoctl a_params;
    struct vioc_fprio fprio;

    fprio.cmd = FPRIO_SET;
    fprio.Cell = fid.Cell;
    fprio.Volume = fid.fid.Volume;
    fprio.Vnode = fid.fid.Vnode;
    fprio.Unique = fid.fid.Unique;
    fprio.prio = prio;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = 0;
    a_params.in = (char *)&fprio;
    a_params.out = NULL;

    if (pioctl(NULL, VIOC_FPRIOSTATUS, &a_params, 0) == -1)
	return errno;

    return 0;
}
#endif

#ifdef VIOC_FPRIOSTATUS
int
fs_getfprio(struct VenusFid fid, int16_t * prio)
{
    struct ViceIoctl a_params;
    struct vioc_fprio fprio;

    fprio.cmd = FPRIO_GET;
    fprio.Cell = fid.Cell;
    fprio.Volume = fid.fid.Volume;
    fprio.Vnode = fid.fid.Vnode;
    fprio.Unique = fid.fid.Unique;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = sizeof(*prio);
    a_params.in = (char *)&fprio;
    a_params.out = (char *)prio;

    if (pioctl(NULL, VIOC_FPRIOSTATUS, &a_params, 0) == -1)
	return errno;

    return 0;
}
#endif

#ifdef VIOC_FPRIOSTATUS
int
fs_setmaxfprio(int16_t maxprio)
{
    struct ViceIoctl a_params;
    struct vioc_fprio fprio;

    fprio.cmd = FPRIO_SETMAX;
    fprio.prio = maxprio;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = 0;
    a_params.in = (char *)&fprio;
    a_params.out = NULL;

    if (pioctl(NULL, VIOC_FPRIOSTATUS, &a_params, 0) == -1)
	return errno;

    return 0;
}
#endif

#ifdef VIOC_FPRIOSTATUS
int
fs_getmaxfprio(int16_t * maxprio)
{
    struct ViceIoctl a_params;
    struct vioc_fprio fprio;

    fprio.cmd = FPRIO_GETMAX;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = sizeof(*maxprio);
    a_params.in = (char *)&fprio;
    a_params.out = (char *)maxprio;

    if (pioctl(NULL, VIOC_FPRIOSTATUS, &a_params, 0) == -1)
	return errno;

    return 0;
}
#endif

/*
 *
 */

#ifdef VIOCGETCACHEPARAMS
int
fs_getfilecachestats(afs_uint32 * max_bytes, afs_uint32 * used_bytes,
		     afs_uint32 * max_vnodes, afs_uint32 * used_vnodes)
{
    afs_uint32 parms[16];
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = sizeof(parms);
    a_params.in = NULL;
    a_params.out = (char *)parms;

    memset(parms, 0, sizeof(parms));

    if (pioctl(NULL, VIOCGETCACHEPARAMS, &a_params, 0) == -1)
	return errno;

    /* param[0] and param[1] send maxbytes and usedbytes in kbytes */

    if (max_vnodes)
	*max_vnodes = parms[2];
    if (used_vnodes)
	*used_vnodes = parms[3];
    if (max_bytes)
	*max_bytes = parms[4];
    if (used_bytes)
	*used_bytes = parms[5];

    return 0;
}
#endif

/*
 *
 */

#ifdef VIOC_AVIATOR
int
fs_getaviatorstats(afs_uint32 * max_workers, afs_uint32 * used_workers)
{
    afs_uint32 parms[16];
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = sizeof(parms);
    a_params.in = NULL;
    a_params.out = (char *)parms;

    if (pioctl(NULL, VIOC_AVIATOR, &a_params, 0) == -1)
	return errno;

    if (max_workers)
	*max_workers = parms[0];
    if (used_workers)
	*used_workers = parms[1];

    return 0;
}
#endif

/*
 *
 */

#ifdef VIOC_GCPAGS
int
fs_gcpags(void)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = 0;
    a_params.in = NULL;
    a_params.out = NULL;


    if (pioctl(NULL, VIOC_GCPAGS, &a_params, 0) != 0)
	return errno;

    return 0;
}
#endif

/*
 *
 */

#ifdef VIOC_CALCULATE_CACHE
int
fs_calculate_cache(afs_uint32 * calculated, afs_uint32 * usedbytes)
{
    afs_uint32 parms[16];
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = sizeof(parms);
    a_params.in = NULL;
    a_params.out = (char *)parms;

    if (pioctl(NULL, VIOC_CALCULATE_CACHE, &a_params, 0) == -1)
	return errno;

    if (calculated)
	*calculated = parms[0];
    if (usedbytes)
	*usedbytes = parms[1];

    return 0;
}
#endif

/*
 *
 */

#ifdef VIOC_BREAKCALLBACK
int
fs_invalidate(const char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = 0;
    a_params.in = NULL;
    a_params.out = NULL;

    if (pioctl((char *)path, VIOC_BREAKCALLBACK, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}
#endif

/*
 * Get/set debug levels with pioctl_cmd.
 *
 * inflags == -1 -> don't change
 * outflags == NULL -> don't return
 */

static int
debug(int pioctl_cmd, int inflags, int *outflags, char *pathname)
{
    struct ViceIoctl a_params;

    afs_int32 rinflags = inflags;
    afs_int32 routflags;

    if (inflags != -1) {
	a_params.in_size = sizeof(rinflags);
	a_params.in = (char *)&rinflags;
    } else {
	a_params.in_size = 0;
	a_params.in = NULL;
    }

    if (outflags) {
	a_params.out_size = sizeof(routflags);
	a_params.out = (char *)&routflags;
    } else {
	a_params.out_size = 0;
	a_params.out = NULL;
    }

    if (pioctl(pathname, pioctl_cmd, &a_params, 0) == -1)
	return errno;

    if (outflags)
	*outflags = routflags;

    return 0;
}

/*
 * xfs_debug
 */

#ifdef VIOC_XFSDEBUG
int
xfs_debug(int inflags, int *outflags)
{
    return debug(VIOC_XFSDEBUG, inflags, outflags, NULL);
}
#endif

/*
 * xfs_debug_print
 */

#ifdef VIOC_XFSDEBUG_PRINT
int
xfs_debug_print(int inflags, char *pathname)
{
    return debug(VIOC_XFSDEBUG_PRINT, inflags, NULL, pathname);
}
#endif

/*
 * arla_debug
 */

#ifdef VIOC_ARLADEBUG
int
arla_debug(int inflags, int *outflags)
{
    return debug(VIOC_ARLADEBUG, inflags, outflags, NULL);
}
#endif

/*
 * checkservers
 *
 *   flags is the same flags as in CKSERV flags
 *
 */

int
fs_checkservers(char *cell, afs_int32 flags, afs_uint32 * hosts, int numhosts)
{
    struct ViceIoctl a_params;
    char *in = NULL;
    int ret;
    size_t insize;

    if (cell != NULL) {
	insize = strlen(cell) + sizeof(afs_int32) + 1;
	in = malloc(insize);
	if (in == NULL)
	    errx(1, "malloc");

	memcpy(in, &flags, sizeof(flags));

	memcpy(in + sizeof(afs_int32), cell, strlen(cell));
	in[sizeof(afs_int32) + strlen(cell)] = '\0';

	a_params.in_size = insize;
	a_params.in = in;
    } else {
	a_params.in_size = sizeof(flags);
	a_params.in = (caddr_t) & flags;
    }

    a_params.out_size = numhosts * sizeof(afs_uint32);
    a_params.out = (caddr_t) hosts;

    ret = 0;

    if (pioctl(NULL, VIOCCKSERV, &a_params, 0) == -1)
	ret = errno;

    if (in)
	free(in);

    return ret;
}

/*
 * check validity of cached volume information
 */

int
fs_checkvolumes(void)
{
    struct ViceIoctl a_params;

    a_params.in = NULL;
    a_params.in_size = 0;
    a_params.out = NULL;
    a_params.out_size = 0;

    if (pioctl(NULL, VIOCCKBACK, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * set current sysname to `sys'
 */

int
fs_set_sysname(const char *sys)
{
    struct ViceIoctl a_params;
    afs_int32 set = 1;

    a_params.in_size = sizeof(set) + strlen(sys) + 1;
    a_params.in = malloc(a_params.in_size);
    if (a_params.in == NULL)
	return ENOMEM;
    a_params.out = NULL;
    a_params.out_size = 0;
    memcpy(a_params.in, &set, sizeof(set));
    strcpy(a_params.in + sizeof(set), sys);

    if (pioctl(NULL, VIOC_AFS_SYSNAME, &a_params, 1) < 0)
	return errno;
    else
	return 0;
}

/*
 *
 */

int
fs_setcache(int lv, int hv, int lb, int hb)
{
    struct ViceIoctl a_params;
    afs_uint32 s[4];

    s[0] = lv;
    s[1] = hv;
    s[2] = lb;
    s[3] = hb;

    a_params.in_size = ((hv == 0) ? 1 : 4) * sizeof(afs_uint32);
    a_params.out_size = 0;
    a_params.in = (void *)s;
    a_params.out = NULL;

    if (pioctl(NULL, VIOCSETCACHESIZE, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * return the local cell in `cell' (of size `cell_sz').
 */

int
fs_wscell(char *cell, size_t cell_sz)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.in = NULL;
    a_params.out_size = cell_sz;
    a_params.out = cell;

    if (pioctl(NULL, VIOC_GET_WS_CELL, &a_params, 0) < 0)
	return errno;
    return 0;
}

/*
 * Flush the contents of the volume pointed to by `path'.
 */

int
fs_flushvolume(const char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = 0;
    a_params.in = NULL;
    a_params.out = NULL;

    if (pioctl((char *)path, VIOC_FLUSHVOLUME, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * Flush the file `path' from the cache.
 */

int
fs_flush(const char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = 0;
    a_params.in = NULL;
    a_params.out = NULL;

    if (pioctl((char *)path, VIOCFLUSH, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 *
 */

int
fs_venuslog(void)
{
    struct ViceIoctl a_params;
    afs_int32 status = 0;	/* XXX not really right, but anyway */

    a_params.in_size = sizeof(afs_int32);
    a_params.out_size = 0;
    a_params.in = (caddr_t) & status;
    a_params.out = NULL;

    if (pioctl(NULL, VIOC_VENUSLOG, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * Get status for `cell' and put the flags in `flags'.
 */

int
fs_getcellstatus(char *cellname, afs_uint32 * flags)
{
    struct ViceIoctl a_params;

    a_params.in_size = strlen(cellname) + 1;
    a_params.out_size = sizeof(afs_uint32);
    a_params.in = cellname;
    a_params.out = (caddr_t) flags;

    if (pioctl(NULL, VIOC_GETCELLSTATUS, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * Separate `path' into directory and last component and call
 * pioctl with `pioctl_cmd'.
 */

static int
internal_mp(const char *path, int pioctl_cmd, char **res)
{
    struct ViceIoctl a_params;
    char *last;
    char *path_bkp;
    int error;

    path_bkp = strdup(path);
    if (path_bkp == NULL) {
	printf("fs: Out of memory\n");
	return ENOMEM;
    }

    a_params.out = malloc(PIOCTL_MAXSIZE);
    if (a_params.out == NULL) {
	printf("fs: Out of memory\n");
	free(path_bkp);
	return ENOMEM;
    }

    /* If path contains more than the filename alone - split it */

    last = strrchr(path_bkp, '/');
    if (last != NULL) {
	*last = '\0';
	a_params.in = last + 1;
    } else
	a_params.in = (char *)path;

    a_params.in_size = strlen(a_params.in) + 1;
    a_params.out_size = PIOCTL_MAXSIZE;

    error = pioctl(last ? path_bkp : ".", pioctl_cmd, &a_params, 1);
    if (error < 0) {
	error = errno;
	free(path_bkp);
	free(a_params.out);
	return error;
    }

    if (res != NULL)
	*res = a_params.out;
    else
	free(a_params.out);
    free(path_bkp);
    return 0;
}

int
fs_lsmount(const char *path)
{
    char *res;
    int error = internal_mp(path, VIOC_AFS_STAT_MT_PT, &res);

    if (error == 0) {
	printf("'%s' is a mount point for volume '%s'\n", path, res);
	free(res);
    }
    return error;
}

int
fs_rmmount(const char *path)
{
    return internal_mp(path, VIOC_AFS_DELETE_MT_PT, NULL);
}

int
fs_incompat_renumber(int *ret)
{
    struct ViceIoctl a_params;
    unsigned char buf[1024];

    a_params.in_size = 0;
    a_params.out_size = sizeof(buf);
    a_params.in = 0;
    a_params.out = (caddr_t) buf;

    /* getcrypt or getinitparams */
    if (pioctl(NULL, _VICEIOCTL(49), &a_params, 0) < 0) {
	if (errno == EINVAL) {

	    /* not openafs or old openafs */

	    a_params.in_size = 0;
	    a_params.out_size = 4;
	    a_params.in = 0;
	    a_params.out = (caddr_t) buf;

	    if (pioctl(NULL, _VICEIOCTL(49), &a_params, 0) < 0) {
		if (errno == EINVAL) {

		    a_params.in_size = 0;
		    a_params.out_size = 4;
		    a_params.in = 0;
		    a_params.out = (caddr_t) buf;

		    /* might be new interface */

		    if (pioctl(NULL, _VICEIOCTL(55), &a_params, 0) < 0)
			return errno;	/* dunno */

		    *ret = 1;
		    return 0;
		} else {
		    return errno;
		}
	    }
	    *ret = 0;
	    return 0;
	} else
	    return errno;
    }
    *ret = 1;
    return 0;
}
