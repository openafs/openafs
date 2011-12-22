/*
 * Copyright (c) 2008-2010 Sine Nomine Associates
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
 * 3. Neither the name of Sine Nomine Associates nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * afsd_fuse.c - Driver for afsd.fuse, and glue between FUSE and libuafs
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <sysincludes.h>
#include <afs/afsutil.h>
#include <afs_usrops.h>
#include <afs/cmd.h>
#include <afs/afs_args.h>

#include "afsd.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#define FUSE_USE_VERSION 27
#include <fuse.h>

/* command-line arguments to pass to afsd and the cmd_ library */
static struct fuse_args afsd_args = FUSE_ARGS_INIT(0, NULL);

/* command-line arguments to pass to FUSE */
static struct fuse_args fuse_args = FUSE_ARGS_INIT(0, NULL);

/* used for command-line parsing in fuafsd_fuse_opt below */
static int fuafsd_cmd_accept = 0;
static int nullfd;
static int stderr_save;

/**
 * Turn FUSE-y paths into libuafs-y paths.
 *
 * @param[in] apath  a path in the form of /localcell/foo/bar
 *
 * @return a path (non-const) in the form /afs/localcell/foo/bar to be freed
 *         by the caller
 */
static char *
afs_path(const char *apath)
{
    size_t len;
    static const char prefix[] = "/afs/";
    char *path;

    len = strlen(apath) + sizeof(prefix);

    path = malloc(len);

    sprintf(path, "%s%s", prefix, apath);

    return path;
}

static void *
fuafsd_init(struct fuse_conn_info *conn)
{
    uafs_Run();

    uafs_setMountDir("/afs");

    return NULL;
}

/* Wrappers around libuafs calls for FUSE */

static int
fuafsd_getattr(const char *apath, struct stat *stbuf)
{
	int code;
	char *path = afs_path(apath);

	code = uafs_lstat(path, stbuf);

	free(path);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_opendir(const char *apath, struct fuse_file_info * fi)
{
	usr_DIR * dirp;
	char *path = afs_path(apath);

	dirp = uafs_opendir(path);

	free(path);

	if (!dirp) {
		return -errno;
	}

	fi->fh = (uintptr_t)dirp;

	return 0;
}

static int
fuafsd_readdir(const char *path, void * buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info * fi)
{
	usr_DIR * dirp;
	struct usr_dirent * direntP;

	dirp = (usr_DIR *)(uintptr_t)fi->fh;

	errno = 0;
	while ((direntP = uafs_readdir(dirp))) {
		if (filler(buf, direntP->d_name, NULL, 0)) {
			/* buffer is full */
			return 0;
		}
	}

	return -errno;
}

static int
fuafsd_releasedir(const char *path, struct fuse_file_info * fi)
{
	return uafs_closedir((usr_DIR *)(uintptr_t)fi->fh);
}

static int
fuafsd_create(const char *apath, mode_t mode, struct fuse_file_info * fi)
{
	int fd;
	char *path = afs_path(apath);

	fd = uafs_open(path, fi->flags, mode);

	free(path);

	if (fd < 0) {
		return -errno;
	}

	fi->fh = fd;

	return 0;
}

static int
fuafsd_open(const char *path, struct fuse_file_info * fi)
{
	return fuafsd_create(path, 0, fi);
}

static int
fuafsd_read(const char *path, char * buf, size_t len, off_t offset,
	struct fuse_file_info * fi)
{
	int fd, code;

	fd = fi->fh;

	code = uafs_pread(fd, buf, len, offset);
	if (code < 0) {
		return -errno;
	}

	return code;
}

static int
fuafsd_readlink(const char *apath, char * buf, size_t len)
{
	int code;
	char *path = afs_path(apath);

	code = uafs_readlink(path, buf, len);

	free(path);

	if (code < 0) {
		return -errno;
	}

	buf[code] = '\0';

	return 0;
}

static int
fuafsd_mkdir(const char *apath, mode_t mode)
{
	int code;
	char *path = afs_path(apath);

	code = uafs_mkdir(path, mode);

	free(path);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_unlink(const char *apath)
{
	int code;
	char *path = afs_path(apath);

	code = uafs_unlink(path);

	free(path);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_rmdir(const char *apath)
{
	int code;
	char *path = afs_path(apath);

	code = uafs_rmdir(path);

	free(path);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_symlink(const char *atarget, const char *asource)
{
	int code;
	char *target = afs_path(atarget);
	char *source = afs_path(asource);

	code = uafs_symlink(target, source);

	free(target);
	free(source);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_rename(const char *aold, const char *anew)
{
	int code;
	char *old = afs_path(aold);
	char *new = afs_path(anew);

	code = uafs_rename(old, new);

	free(old);
	free(new);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_link(const char *aexisting, const char *anew)
{
	int code;
	char *existing = afs_path(aexisting);
	char *new = afs_path(anew);

	code = uafs_link(existing, new);

	free(existing);
	free(new);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_chmod(const char *apath, mode_t mode)
{
	int code;
	char *path = afs_path(apath);

	code = uafs_chmod(path, mode);

	free(path);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_truncate(const char *apath, off_t length)
{
	int code;
	char *path = afs_path(apath);

	code = uafs_truncate(path, length);

	free(path);

	if (code < 0) {
		return -errno;
	}
	return 0;
}

static int
fuafsd_write(const char *path, const char *abuf, size_t len, off_t offset,
             struct fuse_file_info * fi)
{
	int fd, code;
	char *buf = malloc(len);

	fd = fi->fh;
	memcpy(buf, abuf, len);

	code = uafs_pwrite(fd, buf, len, offset);

	free(buf);

	if (code < 0) {
		return -errno;
	}

	return code;
}

static int
fuafsd_statvfs(const char * path, struct statvfs * buf)
{
	if (uafs_statvfs(buf) < 0) {
		return -errno;
	}

	/*
	 * FUSE ignores frsize, and uses bsize for displaying e.g. available
	 * space. Just set bsize to frsize so we get a consistent `df` output.
	 */
	buf->f_bsize = buf->f_frsize;
	return 0;
}

static int
fuafsd_release(const char *path, struct fuse_file_info * fi)
{
	int fd;

	fd = fi->fh;

	if (uafs_close(fd) < 0) {
		return -errno;
	}

	return 0;
}

static void
fuafsd_destroy(void * private_data)
{
	uafs_Shutdown();
}

static struct fuse_operations fuafsd_oper = {
	.init       = fuafsd_init,
	.getattr    = fuafsd_getattr,
	.opendir    = fuafsd_opendir,
	.readdir    = fuafsd_readdir,
	.releasedir = fuafsd_releasedir,
	.open       = fuafsd_open,
	.create     = fuafsd_create,
	.read       = fuafsd_read,
	.readlink   = fuafsd_readlink,
	.mkdir      = fuafsd_mkdir,
	.rmdir      = fuafsd_rmdir,
	.link       = fuafsd_link,
	.unlink     = fuafsd_unlink,
	.symlink    = fuafsd_symlink,
	.rename     = fuafsd_rename,
	.chmod      = fuafsd_chmod,
	.truncate   = fuafsd_truncate,
	.write      = fuafsd_write,
	.statfs     = fuafsd_statvfs,
	.release    = fuafsd_release,
	.destroy    = fuafsd_destroy
};

/* Command line argument processing */

/*
 * See fuafsd_fuse_opt below.
 */
static int
fuafsd_cmd_check(struct cmd_syndesc * ts, void *beforeRock)
{
	fuafsd_cmd_accept = 1;
	return 1;
}

/*
 * Split arguments into FUSE and afsd/libcmd arguments. To determine whether an
 * argument is meant for FUSE or for the cmd interface, we pass the given
 * argument to cmd_Dispatch, and see if our beforeProc is run (which we set to
 * fuafsd_cmd_check). If it was run, the argument is acceptable to cmd; if it
 * was not run, cmd doesn't know what to do with the argument, so we assume the
 * argument is meant for FUSE. We can tell if fuafsd_cmd_check was run by the
 * global fuafsd_cmd_accept bool, which is set to true when fuafsd_cmd_check is
 * run.
 */
static void
split_args(const struct fuse_args *args)
{
	int i;

	cmd_SetBeforeProc(fuafsd_cmd_check, NULL);

	nullfd = open("/dev/null", O_WRONLY);
	stderr_save = dup(2);
	if (nullfd < 0 || stderr_save < 0) {
		perror("open/dup");
		exit(1);
	}

	for (i = 0; args->argv[i]; ++i) {
		int code;
		char *arg = args->argv[i];
		char *argv[3] = {args->argv[0], arg, NULL};

		if (fuafsd_cmd_accept) {
			fuse_opt_add_arg(&afsd_args, arg);
			fuafsd_cmd_accept = 0;
			continue;
		}

		/* redirect stderr to null, so libcmd doesn't print out
		 * an error message for unknown options */
		if (dup2(nullfd, 2) < 0) {
			perror("dup2");
			exit(1);
		}

		code = cmd_Dispatch(2, argv);

		if (dup2(stderr_save, 2) < 0) {
			perror("dup2");
			exit(1);
		}

		/* fuafsd_cmd_check should prevent the dispatch from succeeding;
		 * the only way we should be able to succeed is if -help was
		 * specified, so just exit */
		if (code == 0) {
			exit(1);
		}

		if (fuafsd_cmd_accept || code == CMD_TOOFEW) {
			/* libcmd accepted the argument; must go to afsd */
			fuse_opt_add_arg(&afsd_args, arg);

			if (code == CMD_TOOFEW) {
				/* flag takes another argument; get the next one, too */
				fuafsd_cmd_accept = 1;
			} else {
				fuafsd_cmd_accept = 0;
			}

		} else {
			/* libcmd doesn't recognize the argument; give it to FUSE */
			fuse_opt_add_arg(&fuse_args, arg);
		}
	}

	if (close(nullfd) < 0) {
		perror("close");
	}
	if (close(stderr_save) < 0) {
		perror("close");
	}

	cmd_SetBeforeProc(NULL, NULL);
}

/*
 * First we divide the given arguments into FUSE and cmd arguments, pass the
 * FUSE arguments to FUSE, and call cmd_Dispatch in the FUSE init function.
 */
int
main(int argc, char **argv)
{
	int code;
	struct fuse_args args = FUSE_ARGS_INIT(argc-1, &argv[1]);
	fuse_opt_add_arg(&afsd_args, argv[0]);

#ifdef AFS_SUN511_ENV
	/* for some reason, Solaris 11 FUSE takes the filesystem name from
	 * argv[0], and ignores the -ofsname option */
	fuse_opt_add_arg(&fuse_args, "AFS");
#else
	fuse_opt_add_arg(&fuse_args, argv[0]);
#endif

	/* let us determine file inode numbers, not FUSE. also make "AFS" appear
	 * in df/mount/mnttab/etc output. */
	fuse_opt_add_arg(&fuse_args, "-ouse_ino,fsname=AFS");

	if (getuid() == 0) {
	    /* allow other users to access the mountpoint. only do this for
	     * root, since non-root may or may not be able to do this */
	    fuse_opt_add_arg(&fuse_args, "-oallow_other");
	}

	code = uafs_Setup("/afs");
	if (code) {
		errno = code;
		perror("libuafs");
		return 1;
	}

	split_args(&args);

	uafs_ParseArgs(afsd_args.argc, afsd_args.argv);

	/* pass "-- /mount/dir" to fuse to specify dir to mount; "--" is
	 * just to make sure fuse doesn't interpret the mount dir as a flag
	 */
#ifndef AFS_SUN511_ENV
	/* This seems to screw up option parsing on Solaris 11 FUSE, so just
	 * don't do it. This makes it slightly more annoying to mount on a dir
	 * called -foo or something, but oh well. */
	fuse_opt_add_arg(&fuse_args, "--");
#endif
	fuse_opt_add_arg(&fuse_args, uafs_MountDir());

	return fuse_main(fuse_args.argc, fuse_args.argv, &fuafsd_oper, NULL);
}
