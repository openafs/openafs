/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#define VFS 1

#include <afs/cmd.h>

#include "afsd.h"

#include <assert.h>
#include <afs/afsutil.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/wait.h>


#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if defined(AFS_LINUX20_ENV)
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_FS_TYPES_H
#include <sys/fs_types.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

#ifdef HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif

#ifdef HAVE_SYS_MNTENT_H
#include <sys/mntent.h>
#endif

#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_FSTYP_H
#include <sys/fstyp.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <netinet/in.h>
#include <afs/afs_args.h>
#include <afs/cellconfig.h>
#include <ctype.h>
#include <afs/afssyscalls.h>
#include <afs/afsutil.h>

#ifdef AFS_DARWIN_ENV
#ifdef AFS_DARWIN80_ENV
#include <sys/ioctl.h>
#include <sys/xattr.h>
#endif
#include <mach/mach.h>
#ifndef AFS_DARWIN100_ENV
/* Symbols from the DiskArbitration framework */
kern_return_t DiskArbStart(mach_port_t *);
kern_return_t DiskArbDiskAppearedWithMountpointPing_auto(char *, unsigned int,
							 char *);
#define DISK_ARB_NETWORK_DISK_FLAG 8
#endif
#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>
#endif /* AFS_DARWIN_ENV */

#ifndef MOUNT_AFS
#define	MOUNT_AFS AFS_MOUNT_AFS
#endif /* MOUNT_AFS */

#ifdef AFS_SGI65_ENV
# include <sched.h>
# define SET_RTPRI(P) {  \
    struct sched_param sp; \
    sp.sched_priority = P; \
    if (sched_setscheduler(0, SCHED_RR, &sp)<0) { \
	perror("sched_setscheduler"); \
    } \
}
# define SET_AFSD_RTPRI() SET_RTPRI(68)
# define SET_RX_RTPRI()   SET_RTPRI(199)
#else
# ifdef AFS_LINUX20_ENV
#  define SET_AFSD_RTPRI()
#  define SET_RX_RTPRI() do { \
    if (setpriority(PRIO_PROCESS, 0, -10) < 0) \
	perror("setting rx priority"); \
} while (0)
# else
#  define SET_AFSD_RTPRI()
#  define SET_RX_RTPRI()
# endif
#endif

void
afsd_set_rx_rtpri(void)
{
    SET_RX_RTPRI();
}

void
afsd_set_afsd_rtpri(void)
{
    SET_AFSD_RTPRI();
}

#if !defined(AFS_SGI_ENV) && !defined(AFS_AIX32_ENV)

int
afsd_call_syscall(long param1, long param2, long param3, long param4, long param5,
	     long param6, long  param7)
{
    int error;
# ifdef AFS_LINUX20_ENV
    long eparm[4];
    struct afsprocdata syscall_data;
    int fd = open(PROC_SYSCALL_FNAME, O_RDWR);
    if (fd < 0)
	fd = open(PROC_SYSCALL_ARLA_FNAME, O_RDWR);
    eparm[0] = param4;
    eparm[1] = param5;
    eparm[2] = param6;
    eparm[3] = param7;

    param4 = (long)eparm;

    syscall_data.syscall = AFSCALL_CALL;
    syscall_data.param1 = param1;
    syscall_data.param2 = param2;
    syscall_data.param3 = param3;
    syscall_data.param4 = param4;
    if (fd > 0) {
	error = ioctl(fd, VIOC_SYSCALL, &syscall_data);
	close(fd);
    } else
# endif /* AFS_LINUX20_ENV */
# ifdef AFS_DARWIN80_ENV
    struct afssysargs syscall_data;
    void *ioctldata;
    int fd = open(SYSCALL_DEV_FNAME,O_RDWR);
    int syscallnum;
#ifdef AFS_DARWIN100_ENV
    int is64 = 0;
    struct afssysargs64 syscall64_data;
    if (sizeof(param1) == 8) {
	syscallnum = VIOC_SYSCALL64;
	is64 = 1;
	ioctldata = &syscall64_data;
	syscall64_data.syscall = (int)AFSCALL_CALL;
	syscall64_data.param1 = param1;
	syscall64_data.param2 = param2;
	syscall64_data.param3 = param3;
	syscall64_data.param4 = param4;
	syscall64_data.param5 = param5;
	syscall64_data.param6 = param6;
    } else {
#endif
	syscallnum = VIOC_SYSCALL;
        ioctldata = &syscall_data;
	syscall_data.syscall = AFSCALL_CALL;
	syscall_data.param1 = param1;
	syscall_data.param2 = param2;
	syscall_data.param3 = param3;
	syscall_data.param4 = param4;
	syscall_data.param5 = param5;
	syscall_data.param6 = param6;
#ifdef AFS_DARWIN100_ENV
    }
#endif
    if(fd >= 0) {
        error = ioctl(fd, syscallnum, ioctldata);
        close(fd);
    } else {
        error = -1;
    }
    if (!error) {
#ifdef AFS_DARWIN100_ENV
        if (is64)
            error=syscall64_data.retval;
        else
#endif
            error=syscall_data.retval;
    }
# elif defined(AFS_SUN511_ENV)
	{
	    int rval;
	    rval = ioctl_sun_afs_syscall(AFSCALL_CALL, param1, param2, param3,
	                                 param4, param5, param6, &error);
	    if (rval) {
		error = rval;
	    }
	}
# else /* AFS_DARWIN80_ENV */
    error =
	syscall(AFS_SYSCALL, AFSCALL_CALL, param1, param2, param3, param4,
		param5, param6, param7);
# endif /* !AFS_DARWIN80_ENV */

    if (afsd_debug) {
#ifdef AFS_NBSD40_ENV
        char *s = strerror(errno);
        printf("SScall(%d, %d, %d)=%d (%d, %s)\n", AFS_SYSCALL, AFSCALL_CALL,
                param1, error, errno, s);
#else
	printf("SScall(%d, %d, %ld)=%d ", AFS_SYSCALL, AFSCALL_CALL, param1,
	       error);
#endif
    }

    return (error);
}
#else /* !AFS_SGI_ENV && !AFS_AIX32_ENV */
# if defined(AFS_SGI_ENV)
int
afsd_call_syscall(call, parm0, parm1, parm2, parm3, parm4)
{

    int error;

    error = afs_syscall(call, parm0, parm1, parm2, parm3, parm4);
    if (afsd_verbose)
	printf("SScall(%d, %d)=%d ", call, parm0, error);

    return error;
}
# else /* AFS_SGI_ENV */
int
afsd_call_syscall(call, parm0, parm1, parm2, parm3, parm4, parm5, parm6)
{

    return syscall(AFSCALL_CALL, call, parm0, parm1, parm2, parm3, parm4,
		   parm5, parm6);
}
# endif /* !AFS_SGI_ENV */
#endif /* AFS_SGI_ENV || AFS_AIX32_ENV */


#ifdef	AFS_AIX_ENV
/* Special handling for AIX's afs mount operation since they require much more
 * miscl. information before making the vmount(2) syscall */
#include <sys/vfs.h>

#define	ROUNDUP(x)  (((x) + 3) & ~3)

aix_vmount(const char *cacheMountDir)
{
    struct vmount *vmountp;
    int size, error;

    size = sizeof(struct vmount) + ROUNDUP(strlen(cacheMountDir) + 1) + 5 * 4;
    /* Malloc the vmount structure */
    if ((vmountp = (struct vmount *)malloc(size)) == (struct vmount *)NULL) {
	printf("Can't allocate space for the vmount structure (AIX)\n");
	exit(1);
    }

    /* zero out the vmount structure */
    memset(vmountp, '\0', size);

    /* transfer info into the vmount structure */
    vmountp->vmt_revision = VMT_REVISION;
    vmountp->vmt_length = size;
    vmountp->vmt_fsid.fsid_dev = 0;
    vmountp->vmt_fsid.fsid_type = AFS_MOUNT_AFS;
    vmountp->vmt_vfsnumber = 0;
    vmountp->vmt_time = 0;	/* We'll put the time soon! */
    vmountp->vmt_flags = VFS_DEVMOUNT;	/* read/write permission */
    vmountp->vmt_gfstype = AFS_MOUNT_AFS;
    vmountdata(vmountp, "AFS", cacheMountDir, "", "", "", "rw");

    /* Do the actual mount system call */
    error = vmount(vmountp, size);
    free(vmountp);
    return (error);
}

vmountdata(struct vmount * vmtp, char *obj, char *stub, char *host,
	   char *hostsname, char *info, char *args)
{
    struct data {
	short vmt_off;
	short vmt_size;
    } *vdp, *vdprev;
    int size;

    vdp = (struct data *)vmtp->vmt_data;
    vdp->vmt_off = sizeof(struct vmount);
    size = ROUNDUP(strlen(obj) + 1);
    vdp->vmt_size = size;
    strcpy(vmt2dataptr(vmtp, VMT_OBJECT), obj);

    vdprev = vdp;
    vdp++;
    vdp->vmt_off = vdprev->vmt_off + size;
    size = ROUNDUP(strlen(stub) + 1);
    vdp->vmt_size = size;
    strcpy(vmt2dataptr(vmtp, VMT_STUB), stub);

    vdprev = vdp;
    vdp++;
    vdp->vmt_off = vdprev->vmt_off + size;
    size = ROUNDUP(strlen(host) + 1);
    vdp->vmt_size = size;
    strcpy(vmt2dataptr(vmtp, VMT_HOST), host);

    vdprev = vdp;
    vdp++;
    vdp->vmt_off = vdprev->vmt_off + size;
    size = ROUNDUP(strlen(hostsname) + 1);
    vdp->vmt_size = size;
    strcpy(vmt2dataptr(vmtp, VMT_HOSTNAME), hostsname);


    vdprev = vdp;
    vdp++;
    vdp->vmt_off = vdprev->vmt_off + size;
    size = ROUNDUP(strlen(info) + 1);
    vdp->vmt_size = size;
    strcpy(vmt2dataptr(vmtp, VMT_INFO), info);

    vdprev = vdp;
    vdp++;
    vdp->vmt_off = vdprev->vmt_off + size;
    size = ROUNDUP(strlen(args) + 1);
    vdp->vmt_size = size;
    strcpy(vmt2dataptr(vmtp, VMT_ARGS), args);
}
#endif /* AFS_AIX_ENV */

#ifdef	AFS_HPUX_ENV
#define	MOUNTED_TABLE	MNT_MNTTAB
#else
#ifdef	AFS_SUN5_ENV
#define	MOUNTED_TABLE	MNTTAB
#else
#define	MOUNTED_TABLE	MOUNTED
#endif
#endif

static int
HandleMTab(char *cacheMountDir)
{
#if (defined (AFS_SUN_ENV) || defined (AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV)) && !defined(AFS_SUN58_ENV)
    FILE *tfilep;
#ifdef	AFS_SUN5_ENV
    char tbuf[16];
    struct mnttab tmntent;

    memset(&tmntent, '\0', sizeof(struct mnttab));
    if (!(tfilep = fopen(MOUNTED_TABLE, "a+"))) {
	printf("Can't open %s\n", MOUNTED_TABLE);
	perror(MNTTAB);
	exit(-1);
    }
    tmntent.mnt_special = "AFS";
    tmntent.mnt_mountp = cacheMountDir;
    tmntent.mnt_fstype = "xx";
    tmntent.mnt_mntopts = "rw";
    sprintf(tbuf, "%ld", (long)time((time_t *) 0));
    tmntent.mnt_time = tbuf;
    putmntent(tfilep, &tmntent);
    fclose(tfilep);
#else
#if defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV)
    struct mntent tmntent;
    char *dir;
    int i;

    tfilep = setmntent("/etc/mtab", "a+");
    if (!tfilep) {
	printf("Can't open /etc/mtab for writing (errno %d); not adding "
	       "an entry for AFS\n", errno);
	return 1;
    }

    dir = strdup(cacheMountDir);

    /* trim trailing slashes; don't look at dir[0] in case we are somehow
     * just "/" */
    for (i = strlen(dir)-1; i > 0; i--) {
	if (dir[i] == '/') {
	    dir[i] = '\0';
	} else {
	    break;
	}
    }

    tmntent.mnt_fsname = "AFS";
    tmntent.mnt_dir = dir;
    tmntent.mnt_type = "afs";
    tmntent.mnt_opts = "rw";
    tmntent.mnt_freq = 1;
    tmntent.mnt_passno = 3;
    addmntent(tfilep, &tmntent);
    endmntent(tfilep);

    free(dir);
    dir = NULL;
#else
    struct mntent tmntent;

    memset(&tmntent, '\0', sizeof(struct mntent));
    tfilep = setmntent(MOUNTED_TABLE, "a+");
    if (!tfilep) {
	printf("Can't open %s for write; Not adding afs entry to it\n",
	       MOUNTED_TABLE);
	return 1;
    }
    tmntent.mnt_fsname = "AFS";
    tmntent.mnt_dir = cacheMountDir;
    tmntent.mnt_type = "xx";
    tmntent.mnt_opts = "rw";
    tmntent.mnt_freq = 1;
    tmntent.mnt_passno = 3;
#ifdef	AFS_HPUX_ENV
    tmntent.mnt_type = "afs";
    tmntent.mnt_time = time(0);
    tmntent.mnt_cnode = 0;
#endif
    addmntent(tfilep, &tmntent);
    endmntent(tfilep);
#endif /* AFS_SGI_ENV */
#endif /* AFS_SUN5_ENV */
#endif /* unreasonable systems */
#ifdef AFS_DARWIN_ENV
#ifndef AFS_DARWIN100_ENV
    mach_port_t diskarb_port;
    kern_return_t status;

    status = DiskArbStart(&diskarb_port);
    if (status == KERN_SUCCESS) {
	status =
	    DiskArbDiskAppearedWithMountpointPing_auto("AFS",
						       DISK_ARB_NETWORK_DISK_FLAG,
						       cacheMountDir);
    }

    return status;
#endif
#endif /* AFS_DARWIN_ENV */
    return 0;
}

void
afsd_mount_afs(const char *rn, const char *cacheMountDir)
{
    int mountFlags;		/*Flags passed to mount() */
    char *mountDir; /* For HandleMTab() */

    mountFlags = 0;		/* Read/write file system, can do setuid() */
#if	defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
#ifdef	AFS_SUN5_ENV
    mountFlags |= MS_DATA;
#else
    mountFlags |= M_NEWTYPE;	/* This searches by name in vfs_conf.c so don't need to recompile vfs.c because MOUNT_MAXTYPE has changed; it seems that Sun fixed this at last... */
#endif
#endif

#if defined(AFS_HPUX100_ENV)
    mountFlags |= MS_DATA;
#endif

    if (afsd_verbose)
	printf("%s: Mounting the AFS root on '%s', flags: %d.\n", rn,
	    cacheMountDir, mountFlags);
#if defined(AFS_FBSD60_ENV)
    /* data must be non-const non-NULL but is otherwise ignored */
    if ((mount(MOUNT_AFS, cacheMountDir, mountFlags, &mountFlags)) < 0) {
#elif defined(AFS_FBSD_ENV)
    if ((mount("AFS", cacheMountDir, mountFlags, (caddr_t) 0)) < 0) {
#elif defined(AFS_AIX_ENV)
    if (aix_vmount(cacheMountDir)) {
#elif defined(AFS_HPUX100_ENV)
    if ((mount("", cacheMountDir, mountFlags, "afs", NULL, 0)) < 0) {
#elif defined(AFS_SUN5_ENV)
    if ((mount("AFS", cacheMountDir, mountFlags, "afs", NULL, 0)) < 0) {
#elif defined(AFS_SGI_ENV)
    mountFlags = MS_FSS;
    if ((mount(MOUNT_AFS, cacheMountDir, mountFlags, (caddr_t) MOUNT_AFS))
	< 0) {
#elif defined(AFS_LINUX20_ENV)
    if ((mount("AFS", cacheMountDir, MOUNT_AFS, 0, NULL)) < 0) {
#elif defined(AFS_NBSD50_ENV)
    if ((mount(MOUNT_AFS, cacheMountDir, mountFlags, NULL, 0)) < 0) {
#else
    /* This is the standard mount used by the suns and rts */
    if ((mount(MOUNT_AFS, cacheMountDir, mountFlags, (caddr_t) 0)) < 0) {
#endif
	printf("%s: Can't mount AFS on %s(%d)\n", rn, cacheMountDir,
		errno);
	exit(1);
    }

    mountDir = strdup(cacheMountDir);
    HandleMTab(mountDir);
    free(mountDir);
}

int
afsd_fork(int wait, afsd_callback_func cb, void *rock)
{
    int code;
    code = fork();
    if (code == 0) {
	(*cb) (rock);
	exit(1);
    } else {
	assert(code > 0);
	if (wait) {
	    assert(waitpid(code, NULL, 0) != -1);
	}
    }
    return 0;
}

int
afsd_daemon(int nochdir, int noclose)
{
    return daemon(nochdir, noclose);
}

int
afsd_check_mount(const char *rn, const char *mountdir)
{
    struct stat statbuf;

    if (stat(mountdir, &statbuf)) {
	printf("%s: Mountpoint %s missing.\n", rn, mountdir);
	return -1;
    } else if (!S_ISDIR(statbuf.st_mode)) {
	printf("%s: Mountpoint %s is not a directory.\n", rn, mountdir);
	return -1;
    } else if (mountdir[0] != '/') {
	printf("%s: Mountpoint %s is not an absolute path.\n", rn, mountdir);
	return -1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    int code;

    afsd_init();

    code = afsd_parse(argc, argv);
    if (code) {
	return -1;
    }

    return afsd_run();
}
