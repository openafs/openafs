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
#include <afs/opr.h>

#include <roken.h>

#ifdef IGNORE_SOME_GCC_WARNINGS
# ifdef __clang__
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# else
#  pragma GCC diagnostic warning "-Wdeprecated-declarations"
# endif
#endif

#define VFS 1

#include <afs/cmd.h>

#include "afsd.h"

#include <assert.h>
#include <afs/afsutil.h>
#include <sys/file.h>
#include <sys/wait.h>

#if defined(AFS_LINUX_ENV)
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

#include <ctype.h>

#include <afs/opr.h>
#include <afs/afs_args.h>
#include <afs/cellconfig.h>
#include <afs/afssyscalls.h>
#ifdef AFS_DARWIN_ENV
#ifdef AFS_DARWIN80_ENV
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

#define AFS_MOUNT_STR "afs"
#ifndef MOUNT_AFS
#define	MOUNT_AFS AFS_MOUNT_STR
#endif /* MOUNT_AFS */

#ifdef AFS_SGI_ENV
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
# ifdef AFS_LINUX_ENV
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

static char *afsd_syscalls[AFSOP_MAX_OPCODE + 1]; /* For syscall tracing. */

static void
afsd_init_syscall_opcodes(void)
{
#define add_opcode(x) afsd_syscalls[x] = #x
    add_opcode(AFSOP_START_RXCALLBACK);
    add_opcode(AFSOP_START_AFS);
    add_opcode(AFSOP_START_BKG);
    add_opcode(AFSOP_START_TRUNCDAEMON);
    add_opcode(AFSOP_START_CS);
    add_opcode(AFSOP_ADDCELL);
    add_opcode(AFSOP_CACHEINIT);
    add_opcode(AFSOP_CACHEINFO);
    add_opcode(AFSOP_VOLUMEINFO);
    add_opcode(AFSOP_CACHEFILE);
    add_opcode(AFSOP_CACHEINODE);
    add_opcode(AFSOP_AFSLOG);
    add_opcode(AFSOP_ROOTVOLUME);
    add_opcode(AFSOP_STARTLOG);
    add_opcode(AFSOP_ENDLOG);
    add_opcode(AFSOP_AFS_VFSMOUNT);
    add_opcode(AFSOP_ADVISEADDR);
    add_opcode(AFSOP_CLOSEWAIT);
    add_opcode(AFSOP_RXEVENT_DAEMON);
    add_opcode(AFSOP_GETMTU);
    add_opcode(AFSOP_GETIFADDRS);
    add_opcode(AFSOP_ADDCELL2);
    add_opcode(AFSOP_AFSDB_HANDLER);
    add_opcode(AFSOP_SET_DYNROOT);
    add_opcode(AFSOP_ADDCELLALIAS);
    add_opcode(AFSOP_SET_FAKESTAT);
    add_opcode(AFSOP_CELLINFO);
    add_opcode(AFSOP_SET_THISCELL);
    add_opcode(AFSOP_BASIC_INIT);
    add_opcode(AFSOP_SET_BACKUPTREE);
    add_opcode(AFSOP_SET_RXPCK);
    add_opcode(AFSOP_BUCKETPCT);
    add_opcode(AFSOP_SET_RXMAXMTU);
    add_opcode(AFSOP_BKG_HANDLER);
    add_opcode(AFSOP_GETMASK);
    add_opcode(AFSOP_SET_RXMAXFRAGS);
    add_opcode(AFSOP_SET_RMTSYS_FLAG);
    add_opcode(AFSOP_SEED_ENTROPY);
    add_opcode(AFSOP_SET_INUMCALC);
    add_opcode(AFSOP_RXLISTENER_DAEMON);
    add_opcode(AFSOP_CACHEBASEDIR);
    add_opcode(AFSOP_CACHEDIRS);
    add_opcode(AFSOP_CACHEFILES);
    add_opcode(AFSOP_SETINT);
    add_opcode(AFSOP_GO);
    add_opcode(AFSOP_CHECKLOCKS);
    add_opcode(AFSOP_SHUTDOWN);
    add_opcode(AFSOP_STOP_RXCALLBACK);
    add_opcode(AFSOP_STOP_AFS);
    add_opcode(AFSOP_STOP_BKG);
    add_opcode(AFSOP_STOP_TRUNCDAEMON);
    /* AFSOP_STOP_RXEVENT -- not a syscall opcode */
    /* AFSOP_STOP_COMPLETE -- not a syscall opcode */
    add_opcode(AFSOP_STOP_CS);
    /* AFSOP_STOP_RXK_LISTENER -- not a syscall opcode */
    add_opcode(AFSOP_STOP_AFSDB);
    add_opcode(AFSOP_STOP_NETIF);
#undef add_opcode
}

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

#if defined(AFS_LINUX_ENV)
int
os_syscall(struct afsd_syscall_args *args)
{
    int error;
    struct afsprocdata syscall_data;

    int fd = open(PROC_SYSCALL_FNAME, O_RDWR);
    if (fd < 0)
	fd = open(PROC_SYSCALL_ARLA_FNAME, O_RDWR);

    if (fd < 0)
	return -1;

    syscall_data.syscall = AFSCALL_CALL;
    syscall_data.param1 = args->syscall;
    syscall_data.param2 = args->params[0];
    syscall_data.param3 = args->params[1];
    syscall_data.param4 = (long) &args->params[2];

    error = ioctl(fd, VIOC_SYSCALL, &syscall_data);
    close(fd);

    return error;
}
#elif defined(AFS_DARWIN80_ENV)

# if defined(AFS_DARWIN100_ENV)
static int
os_syscall64(struct afsd_syscall_args *args)
{
    int error;
    struct afssysargs64 syscall64_data;
    int fd = open(SYSCALL_DEV_FNAME, O_RDWR);

    if (fd < 0)
	return -1;

    syscall64_data.syscall = (int)AFSCALL_CALL;
    syscall64_data.param1 = args->syscall;
    syscall64_data.param2 = args->params[0];
    syscall64_data.param3 = args->params[1];
    syscall64_data.param4 = args->params[2];
    syscall64_data.param5 = args->params[3];
    syscall64_data.param6 = args->params[4];

    error = ioctl(fd, VIOC_SYSCALL64, &syscall64_data);
    close(fd);

    if (error)
	return error;

    return syscall64_data.retval;
}
# endif

static int
os_syscall(struct afsd_syscall_args *args)
{
    int error;
    struct afssysargs syscall_data;
    int fd;

# ifdef AFS_DARWIN100_ENV
    if (sizeof(long) == 8)
	return os_syscall64(args);
# endif

    fd = open(SYSCALL_DEV_FNAME, O_RDWR);
    if (fd < 0)
	return -1;

    syscall_data.syscall = AFSCALL_CALL;
    syscall_data.param1 = (unsigned int)(uintptr_t)args->syscall;
    syscall_data.param2 = (unsigned int)(uintptr_t)args->params[0];
    syscall_data.param3 = (unsigned int)(uintptr_t)args->params[1];
    syscall_data.param4 = (unsigned int)(uintptr_t)args->params[2];
    syscall_data.param5 = (unsigned int)(uintptr_t)args->params[3];
    syscall_data.param6 = (unsigned int)(uintptr_t)args->params[4];

    error = ioctl(fd, VIOC_SYSCALL, syscall_data);
    close(fd);

    if (error)
	return error;

    return syscall_data.retval;
}

#elif defined(AFS_SUN511_ENV)
static int
os_syscall(struct afsd_syscall_args *args)
{
    int retval, error;

    error = ioctl_sun_afs_syscall(AFSCALL_CALL, args->syscall,
				 args->params[0], args->params[1],
				 args->params[2], args->params[3],
				 args->params[4], &retval);
    if (error)
	return error;

    return retval;
}
#elif defined(AFS_SGI_ENV)
static int
os_syscall(struct afsd_syscall_args *args)
{
    return afs_syscall(args->syscall, args->params[0], args->params[1],
		       args->params[2], args->params[3], args->params[4]);
}
#elif defined(AFS_AIX32_ENV)
static int
os_syscall(struct afsd_syscall_args *args)
{
    return syscall(AFSCALL_CALL, args->syscall,
		   args->params[0], args->params[1], args->params[2],
		   args->params[3], args->params[4], args->params[5],
		   args->params[6]);
}
#else
static int
os_syscall(struct afsd_syscall_args *args)
{
    return syscall(AFS_SYSCALL, AFSCALL_CALL, args->syscall,
		   args->params[0], args->params[1], args->params[2],
		   args->params[3], args->params[4], args->params[5]);
}
#endif

int
afsd_call_syscall(struct afsd_syscall_args *args)
{
    int error;

    error = os_syscall(args);

    if (afsd_debug) {
	char *opcode;
	char buffer[32];
        const char *syscall_str;

#if defined(AFS_SYSCALL)
	syscall_str = opr_stringize(AFS_SYSCALL);
#else
        syscall_str = "[AFS_SYSCALL]";
#endif

	if ((args->syscall < 0) ||
	    (args->syscall >= (sizeof(afsd_syscalls) / sizeof(*afsd_syscalls))))
	    opcode = NULL;
	else
	    opcode = afsd_syscalls[args->syscall];

	if (opcode == NULL) {
	    snprintf(buffer, sizeof(buffer), "unknown (%d)", args->syscall);
	    opcode = buffer;
	}

	if (error == -1) {
	    char *s = strerror(errno);
	    printf("os_syscall(%s, %d, %s, 0x%lx)=%d (%d, %s)\n",
		    syscall_str, AFSCALL_CALL, opcode,
		   (long)args->params[0], error, errno, s);
	} else {
	    printf("os_syscall(%s %d %s, 0x%lx)=%d\n",
		    syscall_str, AFSCALL_CALL, opcode,
		   (long)args->params[0], error);
	}
    }

    return error;
}

#ifdef	AFS_AIX_ENV
/* Special handling for AIX's afs mount operation since they require much more
 * miscl. information before making the vmount(2) syscall */
#include <sys/vfs.h>

#define	ROUNDUP(x)  (((x) + 3) & ~3)

static void
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

static int
aix_vmount(const char *cacheMountDir)
{
    struct vmount *vmountp;
    int size, error;

    size = sizeof(struct vmount) + ROUNDUP(strlen(cacheMountDir) + 1) + 5 * 4;
    /* Malloc and zero the vmount structure */
    if ((vmountp = calloc(1, size)) == NULL) {
	printf("Can't allocate space for the vmount structure (AIX)\n");
	exit(1);
    }

    /* transfer info into the vmount structure */
    vmountp->vmt_revision = VMT_REVISION;
    vmountp->vmt_length = size;
    vmountp->vmt_fsid.fsid_dev = 0;
    vmountp->vmt_fsid.fsid_type = AFS_FSNO;
    vmountp->vmt_vfsnumber = 0;
    vmountp->vmt_time = 0;	/* We'll put the time soon! */
    vmountp->vmt_flags = VFS_DEVMOUNT;	/* read/write permission */
    vmountp->vmt_gfstype = AFS_FSNO;
    vmountdata(vmountp, "AFS", cacheMountDir, "", "", "", "rw");

    /* Do the actual mount system call */
    error = vmount(vmountp, size);
    free(vmountp);
    return (error);
}
#endif /* AFS_AIX_ENV */

#ifdef	AFS_HPUX_ENV
#define	MOUNTED_TABLE	MNT_MNTTAB
#else
#define	MOUNTED_TABLE	MOUNTED
#endif

static int
HandleMTab(char *cacheMountDir)
{
#if (defined (AFS_HPUX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX_ENV))
    FILE *tfilep;
#if defined(AFS_SGI_ENV) || defined(AFS_LINUX_ENV)
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
#if defined(AFS_FBSD_ENV)
    /* data must be non-const non-NULL but is otherwise ignored */
    if ((mount(MOUNT_AFS, cacheMountDir, mountFlags, &mountFlags)) < 0) {
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
#elif defined(AFS_LINUX_ENV)
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
	    opr_Verify(waitpid(code, NULL, 0) != -1);
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

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    afsd_init_syscall_opcodes();
    afsd_init();

    code = afsd_parse(argc, argv);
    if (code == CMD_HELP) {
	return 0; /* Displaying help is not an error. */
    }
    if (code != 0) {
	return -1;
    }

    return afsd_run();
}
