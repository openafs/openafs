/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from $FreeBSD: src/sys/tools/vnode_if.awk,v 1.37 2002/09/26 04:48:43 jeff Exp $
 */

extern struct vnodeop_desc vop_default_desc;
struct vop_islocked_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_islocked_desc;
static __inline int
VOP_ISLOCKED(struct vnode *vp, struct thread *td)
{
    struct vop_islocked_args a;
    int rc;
    a.a_desc = VDESC(vop_islocked);
    a.a_vp = vp;
    a.a_td = td;
    rc = VCALL(vp, VOFFSET(vop_islocked), &a);
    CTR2(KTR_VOP, "VOP_ISLOCKED(vp 0x%lX, td 0x%lX)", vp, td);
    if (rc == 0) {
    } else {
    }
    return (rc);
}
struct vop_lookup_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode **a_vpp;
    struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_lookup_desc;
static __inline int
VOP_LOOKUP(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp)
{
    struct vop_lookup_args a;
    int rc;
    a.a_desc = VDESC(vop_lookup);
    a.a_dvp = dvp;
    a.a_vpp = vpp;
    a.a_cnp = cnp;
#ifdef	DEBUG_VFS_LOCKS
    vop_lookup_pre(&a);
#endif
    rc = VCALL(dvp, VOFFSET(vop_lookup), &a);
    CTR3(KTR_VOP, "VOP_LOOKUP(dvp 0x%lX, vpp 0x%lX, cnp 0x%lX)", dvp, vpp,
	 cnp);
    if (rc == 0) {
    } else {
    }
#ifdef	DEBUG_VFS_LOCKS
    vop_lookup_post(&a, rc);
#endif
    return (rc);
}
struct vop_cachedlookup_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode **a_vpp;
    struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_cachedlookup_desc;
static __inline int
VOP_CACHEDLOOKUP(struct vnode *dvp, struct vnode **vpp,
		 struct componentname *cnp)
{
    struct vop_cachedlookup_args a;
    int rc;
    a.a_desc = VDESC(vop_cachedlookup);
    a.a_dvp = dvp;
    a.a_vpp = vpp;
    a.a_cnp = cnp;
    ASSERT_VI_UNLOCKED(dvp, "VOP_CACHEDLOOKUP");
    ASSERT_VOP_LOCKED(dvp, "VOP_CACHEDLOOKUP");
    rc = VCALL(dvp, VOFFSET(vop_cachedlookup), &a);
    CTR3(KTR_VOP, "VOP_CACHEDLOOKUP(dvp 0x%lX, vpp 0x%lX, cnp 0x%lX)", dvp,
	 vpp, cnp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_CACHEDLOOKUP");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_CACHEDLOOKUP");
    }
    return (rc);
}
struct vop_create_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode **a_vpp;
    struct componentname *a_cnp;
    struct vattr *a_vap;
};
extern struct vnodeop_desc vop_create_desc;
static __inline int
VOP_CREATE(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
	   struct vattr *vap)
{
    struct vop_create_args a;
    int rc;
    a.a_desc = VDESC(vop_create);
    a.a_dvp = dvp;
    a.a_vpp = vpp;
    a.a_cnp = cnp;
    a.a_vap = vap;
    ASSERT_VI_UNLOCKED(dvp, "VOP_CREATE");
    ASSERT_VOP_LOCKED(dvp, "VOP_CREATE");
    rc = VCALL(dvp, VOFFSET(vop_create), &a);
    CTR4(KTR_VOP, "VOP_CREATE(dvp 0x%lX, vpp 0x%lX, cnp 0x%lX, vap 0x%lX)",
	 dvp, vpp, cnp, vap);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_CREATE");
	ASSERT_VOP_LOCKED(dvp, "VOP_CREATE");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_CREATE");
	ASSERT_VOP_LOCKED(dvp, "VOP_CREATE");
    }
    return (rc);
}
struct vop_whiteout_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct componentname *a_cnp;
    int a_flags;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int
VOP_WHITEOUT(struct vnode *dvp, struct componentname *cnp, int flags)
{
    struct vop_whiteout_args a;
    int rc;
    a.a_desc = VDESC(vop_whiteout);
    a.a_dvp = dvp;
    a.a_cnp = cnp;
    a.a_flags = flags;
    ASSERT_VI_UNLOCKED(dvp, "VOP_WHITEOUT");
    ASSERT_VOP_LOCKED(dvp, "VOP_WHITEOUT");
    rc = VCALL(dvp, VOFFSET(vop_whiteout), &a);
    CTR3(KTR_VOP, "VOP_WHITEOUT(dvp 0x%lX, cnp 0x%lX, flags %ld)", dvp, cnp,
	 flags);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_WHITEOUT");
	ASSERT_VOP_LOCKED(dvp, "VOP_WHITEOUT");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_WHITEOUT");
	ASSERT_VOP_LOCKED(dvp, "VOP_WHITEOUT");
    }
    return (rc);
}
struct vop_mknod_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode **a_vpp;
    struct componentname *a_cnp;
    struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mknod_desc;
static __inline int
VOP_MKNOD(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
	  struct vattr *vap)
{
    struct vop_mknod_args a;
    int rc;
    a.a_desc = VDESC(vop_mknod);
    a.a_dvp = dvp;
    a.a_vpp = vpp;
    a.a_cnp = cnp;
    a.a_vap = vap;
    ASSERT_VI_UNLOCKED(dvp, "VOP_MKNOD");
    ASSERT_VOP_LOCKED(dvp, "VOP_MKNOD");
    rc = VCALL(dvp, VOFFSET(vop_mknod), &a);
    CTR4(KTR_VOP, "VOP_MKNOD(dvp 0x%lX, vpp 0x%lX, cnp 0x%lX, vap 0x%lX)",
	 dvp, vpp, cnp, vap);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_MKNOD");
	ASSERT_VOP_LOCKED(dvp, "VOP_MKNOD");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_MKNOD");
	ASSERT_VOP_LOCKED(dvp, "VOP_MKNOD");
    }
    return (rc);
}
struct vop_open_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_mode;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_open_desc;
static __inline int
VOP_OPEN(struct vnode *vp, int mode, struct ucred *cred, struct thread *td)
{
    struct vop_open_args a;
    int rc;
    a.a_desc = VDESC(vop_open);
    a.a_vp = vp;
    a.a_mode = mode;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_OPEN");
    ASSERT_VOP_LOCKED(vp, "VOP_OPEN");
    rc = VCALL(vp, VOFFSET(vop_open), &a);
    CTR4(KTR_VOP, "VOP_OPEN(vp 0x%lX, mode %ld, cred 0x%lX, td 0x%lX)", vp,
	 mode, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_OPEN");
	ASSERT_VOP_LOCKED(vp, "VOP_OPEN");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_OPEN");
	ASSERT_VOP_LOCKED(vp, "VOP_OPEN");
    }
    return (rc);
}
struct vop_close_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_fflag;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_close_desc;
static __inline int
VOP_CLOSE(struct vnode *vp, int fflag, struct ucred *cred, struct thread *td)
{
    struct vop_close_args a;
    int rc;
    a.a_desc = VDESC(vop_close);
    a.a_vp = vp;
    a.a_fflag = fflag;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_CLOSE");
    ASSERT_VOP_UNLOCKED(vp, "VOP_CLOSE");
    rc = VCALL(vp, VOFFSET(vop_close), &a);
    CTR4(KTR_VOP, "VOP_CLOSE(vp 0x%lX, fflag %ld, cred 0x%lX, td 0x%lX)", vp,
	 fflag, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_CLOSE");
	ASSERT_VOP_UNLOCKED(vp, "VOP_CLOSE");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_CLOSE");
	ASSERT_VOP_UNLOCKED(vp, "VOP_CLOSE");
    }
    return (rc);
}
struct vop_access_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_mode;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_access_desc;
static __inline int
VOP_ACCESS(struct vnode *vp, int mode, struct ucred *cred, struct thread *td)
{
    struct vop_access_args a;
    int rc;
    a.a_desc = VDESC(vop_access);
    a.a_vp = vp;
    a.a_mode = mode;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_ACCESS");
    ASSERT_VOP_LOCKED(vp, "VOP_ACCESS");
    rc = VCALL(vp, VOFFSET(vop_access), &a);
    CTR4(KTR_VOP, "VOP_ACCESS(vp 0x%lX, mode %ld, cred 0x%lX, td 0x%lX)", vp,
	 mode, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_ACCESS");
	ASSERT_VOP_LOCKED(vp, "VOP_ACCESS");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_ACCESS");
	ASSERT_VOP_LOCKED(vp, "VOP_ACCESS");
    }
    return (rc);
}
struct vop_getattr_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct vattr *a_vap;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_getattr_desc;
static __inline int
VOP_GETATTR(struct vnode *vp, struct vattr *vap, struct ucred *cred,
	    struct thread *td)
{
    struct vop_getattr_args a;
    int rc;
    a.a_desc = VDESC(vop_getattr);
    a.a_vp = vp;
    a.a_vap = vap;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_GETATTR");
    ASSERT_VOP_LOCKED(vp, "VOP_GETATTR");
    rc = VCALL(vp, VOFFSET(vop_getattr), &a);
    CTR4(KTR_VOP, "VOP_GETATTR(vp 0x%lX, vap 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, vap, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_GETATTR");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_GETATTR");
    }
    return (rc);
}
struct vop_setattr_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct vattr *a_vap;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_setattr_desc;
static __inline int
VOP_SETATTR(struct vnode *vp, struct vattr *vap, struct ucred *cred,
	    struct thread *td)
{
    struct vop_setattr_args a;
    int rc;
    a.a_desc = VDESC(vop_setattr);
    a.a_vp = vp;
    a.a_vap = vap;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_SETATTR");
    ASSERT_VOP_LOCKED(vp, "VOP_SETATTR");
    rc = VCALL(vp, VOFFSET(vop_setattr), &a);
    CTR4(KTR_VOP, "VOP_SETATTR(vp 0x%lX, vap 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, vap, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_SETATTR");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_SETATTR");
    }
    return (rc);
}
struct vop_read_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct uio *a_uio;
    int a_ioflag;
    struct ucred *a_cred;
};
extern struct vnodeop_desc vop_read_desc;
static __inline int
VOP_READ(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *cred)
{
    struct vop_read_args a;
    int rc;
    a.a_desc = VDESC(vop_read);
    a.a_vp = vp;
    a.a_uio = uio;
    a.a_ioflag = ioflag;
    a.a_cred = cred;
    ASSERT_VI_UNLOCKED(vp, "VOP_READ");
    ASSERT_VOP_LOCKED(vp, "VOP_READ");
    rc = VCALL(vp, VOFFSET(vop_read), &a);
    CTR4(KTR_VOP, "VOP_READ(vp 0x%lX, uio 0x%lX, ioflag %ld, cred 0x%lX)", vp,
	 uio, ioflag, cred);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_READ");
	ASSERT_VOP_LOCKED(vp, "VOP_READ");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_READ");
	ASSERT_VOP_LOCKED(vp, "VOP_READ");
    }
    return (rc);
}
struct vop_write_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct uio *a_uio;
    int a_ioflag;
    struct ucred *a_cred;
};
extern struct vnodeop_desc vop_write_desc;
static __inline int
VOP_WRITE(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *cred)
{
    struct vop_write_args a;
    int rc;
    a.a_desc = VDESC(vop_write);
    a.a_vp = vp;
    a.a_uio = uio;
    a.a_ioflag = ioflag;
    a.a_cred = cred;
    ASSERT_VI_UNLOCKED(vp, "VOP_WRITE");
    ASSERT_VOP_LOCKED(vp, "VOP_WRITE");
    rc = VCALL(vp, VOFFSET(vop_write), &a);
    CTR4(KTR_VOP, "VOP_WRITE(vp 0x%lX, uio 0x%lX, ioflag %ld, cred 0x%lX)",
	 vp, uio, ioflag, cred);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_WRITE");
	ASSERT_VOP_LOCKED(vp, "VOP_WRITE");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_WRITE");
	ASSERT_VOP_LOCKED(vp, "VOP_WRITE");
    }
    return (rc);
}
struct vop_lease_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct thread *a_td;
    struct ucred *a_cred;
    int a_flag;
};
extern struct vnodeop_desc vop_lease_desc;
static __inline int
VOP_LEASE(struct vnode *vp, struct thread *td, struct ucred *cred, int flag)
{
    struct vop_lease_args a;
    int rc;
    a.a_desc = VDESC(vop_lease);
    a.a_vp = vp;
    a.a_td = td;
    a.a_cred = cred;
    a.a_flag = flag;
    ASSERT_VI_UNLOCKED(vp, "VOP_LEASE");
    rc = VCALL(vp, VOFFSET(vop_lease), &a);
    CTR4(KTR_VOP, "VOP_LEASE(vp 0x%lX, td 0x%lX, cred 0x%lX, flag %ld)", vp,
	 td, cred, flag);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_LEASE");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_LEASE");
    }
    return (rc);
}
struct vop_ioctl_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    u_long a_command;
    caddr_t a_data;
    int a_fflag;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_ioctl_desc;
static __inline int
VOP_IOCTL(struct vnode *vp, u_long command, caddr_t data, int fflag,
	  struct ucred *cred, struct thread *td)
{
    struct vop_ioctl_args a;
    int rc;
    a.a_desc = VDESC(vop_ioctl);
    a.a_vp = vp;
    a.a_command = command;
    a.a_data = data;
    a.a_fflag = fflag;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_IOCTL");
    ASSERT_VOP_UNLOCKED(vp, "VOP_IOCTL");
    rc = VCALL(vp, VOFFSET(vop_ioctl), &a);
    CTR6(KTR_VOP,
	 "VOP_IOCTL(vp 0x%lX, command %ld, data %ld, fflag %ld, cred 0x%lX, td 0x%lX)",
	 vp, command, data, fflag, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_IOCTL");
	ASSERT_VOP_UNLOCKED(vp, "VOP_IOCTL");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_IOCTL");
	ASSERT_VOP_UNLOCKED(vp, "VOP_IOCTL");
    }
    return (rc);
}
struct vop_poll_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_events;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_poll_desc;
static __inline int
VOP_POLL(struct vnode *vp, int events, struct ucred *cred, struct thread *td)
{
    struct vop_poll_args a;
    int rc;
    a.a_desc = VDESC(vop_poll);
    a.a_vp = vp;
    a.a_events = events;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_POLL");
    ASSERT_VOP_UNLOCKED(vp, "VOP_POLL");
    rc = VCALL(vp, VOFFSET(vop_poll), &a);
    CTR4(KTR_VOP, "VOP_POLL(vp 0x%lX, events %ld, cred 0x%lX, td 0x%lX)", vp,
	 events, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_POLL");
	ASSERT_VOP_UNLOCKED(vp, "VOP_POLL");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_POLL");
	ASSERT_VOP_UNLOCKED(vp, "VOP_POLL");
    }
    return (rc);
}
struct vop_kqfilter_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct knote *a_kn;
};
extern struct vnodeop_desc vop_kqfilter_desc;
static __inline int
VOP_KQFILTER(struct vnode *vp, struct knote *kn)
{
    struct vop_kqfilter_args a;
    int rc;
    a.a_desc = VDESC(vop_kqfilter);
    a.a_vp = vp;
    a.a_kn = kn;
    ASSERT_VI_UNLOCKED(vp, "VOP_KQFILTER");
    ASSERT_VOP_UNLOCKED(vp, "VOP_KQFILTER");
    rc = VCALL(vp, VOFFSET(vop_kqfilter), &a);
    CTR2(KTR_VOP, "VOP_KQFILTER(vp 0x%lX, kn 0x%lX)", vp, kn);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_KQFILTER");
	ASSERT_VOP_UNLOCKED(vp, "VOP_KQFILTER");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_KQFILTER");
	ASSERT_VOP_UNLOCKED(vp, "VOP_KQFILTER");
    }
    return (rc);
}
struct vop_revoke_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_flags;
};
extern struct vnodeop_desc vop_revoke_desc;
static __inline int
VOP_REVOKE(struct vnode *vp, int flags)
{
    struct vop_revoke_args a;
    int rc;
    a.a_desc = VDESC(vop_revoke);
    a.a_vp = vp;
    a.a_flags = flags;
    ASSERT_VI_UNLOCKED(vp, "VOP_REVOKE");
    ASSERT_VOP_UNLOCKED(vp, "VOP_REVOKE");
    rc = VCALL(vp, VOFFSET(vop_revoke), &a);
    CTR2(KTR_VOP, "VOP_REVOKE(vp 0x%lX, flags %ld)", vp, flags);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_REVOKE");
	ASSERT_VOP_UNLOCKED(vp, "VOP_REVOKE");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_REVOKE");
	ASSERT_VOP_UNLOCKED(vp, "VOP_REVOKE");
    }
    return (rc);
}
struct vop_fsync_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct ucred *a_cred;
    int a_waitfor;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_fsync_desc;
static __inline int
VOP_FSYNC(struct vnode *vp, struct ucred *cred, int waitfor,
	  struct thread *td)
{
    struct vop_fsync_args a;
    int rc;
    a.a_desc = VDESC(vop_fsync);
    a.a_vp = vp;
    a.a_cred = cred;
    a.a_waitfor = waitfor;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_FSYNC");
    ASSERT_VOP_LOCKED(vp, "VOP_FSYNC");
    rc = VCALL(vp, VOFFSET(vop_fsync), &a);
    CTR4(KTR_VOP, "VOP_FSYNC(vp 0x%lX, cred 0x%lX, waitfor %ld, td 0x%lX)",
	 vp, cred, waitfor, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_FSYNC");
	ASSERT_VOP_LOCKED(vp, "VOP_FSYNC");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_FSYNC");
	ASSERT_VOP_LOCKED(vp, "VOP_FSYNC");
    }
    return (rc);
}
struct vop_remove_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode *a_vp;
    struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_remove_desc;
static __inline int
VOP_REMOVE(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
    struct vop_remove_args a;
    int rc;
    a.a_desc = VDESC(vop_remove);
    a.a_dvp = dvp;
    a.a_vp = vp;
    a.a_cnp = cnp;
    ASSERT_VI_UNLOCKED(dvp, "VOP_REMOVE");
    ASSERT_VOP_LOCKED(dvp, "VOP_REMOVE");
    ASSERT_VI_UNLOCKED(vp, "VOP_REMOVE");
    ASSERT_VOP_LOCKED(vp, "VOP_REMOVE");
    rc = VCALL(dvp, VOFFSET(vop_remove), &a);
    CTR3(KTR_VOP, "VOP_REMOVE(dvp 0x%lX, vp 0x%lX, cnp 0x%lX)", dvp, vp, cnp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_REMOVE");
	ASSERT_VOP_LOCKED(dvp, "VOP_REMOVE");
	ASSERT_VI_UNLOCKED(vp, "VOP_REMOVE");
	ASSERT_VOP_LOCKED(vp, "VOP_REMOVE");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_REMOVE");
	ASSERT_VOP_LOCKED(dvp, "VOP_REMOVE");
	ASSERT_VI_UNLOCKED(vp, "VOP_REMOVE");
	ASSERT_VOP_LOCKED(vp, "VOP_REMOVE");
    }
    return (rc);
}
struct vop_link_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_tdvp;
    struct vnode *a_vp;
    struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_link_desc;
static __inline int
VOP_LINK(struct vnode *tdvp, struct vnode *vp, struct componentname *cnp)
{
    struct vop_link_args a;
    int rc;
    a.a_desc = VDESC(vop_link);
    a.a_tdvp = tdvp;
    a.a_vp = vp;
    a.a_cnp = cnp;
    ASSERT_VI_UNLOCKED(tdvp, "VOP_LINK");
    ASSERT_VOP_LOCKED(tdvp, "VOP_LINK");
    ASSERT_VI_UNLOCKED(vp, "VOP_LINK");
    ASSERT_VOP_LOCKED(vp, "VOP_LINK");
    rc = VCALL(tdvp, VOFFSET(vop_link), &a);
    CTR3(KTR_VOP, "VOP_LINK(tdvp 0x%lX, vp 0x%lX, cnp 0x%lX)", tdvp, vp, cnp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(tdvp, "VOP_LINK");
	ASSERT_VOP_LOCKED(tdvp, "VOP_LINK");
	ASSERT_VI_UNLOCKED(vp, "VOP_LINK");
	ASSERT_VOP_LOCKED(vp, "VOP_LINK");
    } else {
	ASSERT_VI_UNLOCKED(tdvp, "VOP_LINK");
	ASSERT_VOP_LOCKED(tdvp, "VOP_LINK");
	ASSERT_VI_UNLOCKED(vp, "VOP_LINK");
	ASSERT_VOP_LOCKED(vp, "VOP_LINK");
    }
    return (rc);
}
struct vop_rename_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_fdvp;
    struct vnode *a_fvp;
    struct componentname *a_fcnp;
    struct vnode *a_tdvp;
    struct vnode *a_tvp;
    struct componentname *a_tcnp;
};
extern struct vnodeop_desc vop_rename_desc;
static __inline int
VOP_RENAME(struct vnode *fdvp, struct vnode *fvp, struct componentname *fcnp,
	   struct vnode *tdvp, struct vnode *tvp, struct componentname *tcnp)
{
    struct vop_rename_args a;
    int rc;
    a.a_desc = VDESC(vop_rename);
    a.a_fdvp = fdvp;
    a.a_fvp = fvp;
    a.a_fcnp = fcnp;
    a.a_tdvp = tdvp;
    a.a_tvp = tvp;
    a.a_tcnp = tcnp;
#ifdef	DEBUG_VFS_LOCKS
    vop_rename_pre(&a);
#endif
    rc = VCALL(fdvp, VOFFSET(vop_rename), &a);
    CTR6(KTR_VOP,
	 "VOP_RENAME(fdvp 0x%lX, fvp 0x%lX, fcnp 0x%lX, tdvp 0x%lX, tvp 0x%lX, tcnp 0x%lX)",
	 fdvp, fvp, fcnp, tdvp, tvp, tcnp);
    if (rc == 0) {
    } else {
    }
    return (rc);
}
struct vop_mkdir_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode **a_vpp;
    struct componentname *a_cnp;
    struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mkdir_desc;
static __inline int
VOP_MKDIR(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
	  struct vattr *vap)
{
    struct vop_mkdir_args a;
    int rc;
    a.a_desc = VDESC(vop_mkdir);
    a.a_dvp = dvp;
    a.a_vpp = vpp;
    a.a_cnp = cnp;
    a.a_vap = vap;
    ASSERT_VI_UNLOCKED(dvp, "VOP_MKDIR");
    ASSERT_VOP_LOCKED(dvp, "VOP_MKDIR");
    rc = VCALL(dvp, VOFFSET(vop_mkdir), &a);
    CTR4(KTR_VOP, "VOP_MKDIR(dvp 0x%lX, vpp 0x%lX, cnp 0x%lX, vap 0x%lX)",
	 dvp, vpp, cnp, vap);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_MKDIR");
	ASSERT_VOP_LOCKED(dvp, "VOP_MKDIR");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_MKDIR");
	ASSERT_VOP_LOCKED(dvp, "VOP_MKDIR");
    }
    return (rc);
}
struct vop_rmdir_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode *a_vp;
    struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_rmdir_desc;
static __inline int
VOP_RMDIR(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
    struct vop_rmdir_args a;
    int rc;
    a.a_desc = VDESC(vop_rmdir);
    a.a_dvp = dvp;
    a.a_vp = vp;
    a.a_cnp = cnp;
    ASSERT_VI_UNLOCKED(dvp, "VOP_RMDIR");
    ASSERT_VOP_LOCKED(dvp, "VOP_RMDIR");
    ASSERT_VI_UNLOCKED(vp, "VOP_RMDIR");
    ASSERT_VOP_LOCKED(vp, "VOP_RMDIR");
    rc = VCALL(dvp, VOFFSET(vop_rmdir), &a);
    CTR3(KTR_VOP, "VOP_RMDIR(dvp 0x%lX, vp 0x%lX, cnp 0x%lX)", dvp, vp, cnp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_RMDIR");
	ASSERT_VOP_LOCKED(dvp, "VOP_RMDIR");
	ASSERT_VI_UNLOCKED(vp, "VOP_RMDIR");
	ASSERT_VOP_LOCKED(vp, "VOP_RMDIR");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_RMDIR");
	ASSERT_VOP_LOCKED(dvp, "VOP_RMDIR");
	ASSERT_VI_UNLOCKED(vp, "VOP_RMDIR");
	ASSERT_VOP_LOCKED(vp, "VOP_RMDIR");
    }
    return (rc);
}
struct vop_symlink_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_dvp;
    struct vnode **a_vpp;
    struct componentname *a_cnp;
    struct vattr *a_vap;
    char *a_target;
};
extern struct vnodeop_desc vop_symlink_desc;
static __inline int
VOP_SYMLINK(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
	    struct vattr *vap, char *target)
{
    struct vop_symlink_args a;
    int rc;
    a.a_desc = VDESC(vop_symlink);
    a.a_dvp = dvp;
    a.a_vpp = vpp;
    a.a_cnp = cnp;
    a.a_vap = vap;
    a.a_target = target;
    ASSERT_VI_UNLOCKED(dvp, "VOP_SYMLINK");
    ASSERT_VOP_LOCKED(dvp, "VOP_SYMLINK");
    rc = VCALL(dvp, VOFFSET(vop_symlink), &a);
    CTR5(KTR_VOP,
	 "VOP_SYMLINK(dvp 0x%lX, vpp 0x%lX, cnp 0x%lX, vap 0x%lX, target 0x%lX)",
	 dvp, vpp, cnp, vap, target);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(dvp, "VOP_SYMLINK");
	ASSERT_VOP_LOCKED(dvp, "VOP_SYMLINK");
    } else {
	ASSERT_VI_UNLOCKED(dvp, "VOP_SYMLINK");
	ASSERT_VOP_LOCKED(dvp, "VOP_SYMLINK");
    }
    return (rc);
}
struct vop_readdir_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct uio *a_uio;
    struct ucred *a_cred;
    int *a_eofflag;
    int *a_ncookies;
    u_long **a_cookies;
};
extern struct vnodeop_desc vop_readdir_desc;
static __inline int
VOP_READDIR(struct vnode *vp, struct uio *uio, struct ucred *cred,
	    int *eofflag, int *ncookies, u_long ** cookies)
{
    struct vop_readdir_args a;
    int rc;
    a.a_desc = VDESC(vop_readdir);
    a.a_vp = vp;
    a.a_uio = uio;
    a.a_cred = cred;
    a.a_eofflag = eofflag;
    a.a_ncookies = ncookies;
    a.a_cookies = cookies;
    ASSERT_VI_UNLOCKED(vp, "VOP_READDIR");
    ASSERT_VOP_LOCKED(vp, "VOP_READDIR");
    rc = VCALL(vp, VOFFSET(vop_readdir), &a);
    CTR6(KTR_VOP,
	 "VOP_READDIR(vp 0x%lX, uio 0x%lX, cred 0x%lX, eofflag 0x%lX, ncookies 0x%lX, cookies 0x%lX)",
	 vp, uio, cred, eofflag, ncookies, cookies);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_READDIR");
	ASSERT_VOP_LOCKED(vp, "VOP_READDIR");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_READDIR");
	ASSERT_VOP_LOCKED(vp, "VOP_READDIR");
    }
    return (rc);
}
struct vop_readlink_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct uio *a_uio;
    struct ucred *a_cred;
};
extern struct vnodeop_desc vop_readlink_desc;
static __inline int
VOP_READLINK(struct vnode *vp, struct uio *uio, struct ucred *cred)
{
    struct vop_readlink_args a;
    int rc;
    a.a_desc = VDESC(vop_readlink);
    a.a_vp = vp;
    a.a_uio = uio;
    a.a_cred = cred;
    ASSERT_VI_UNLOCKED(vp, "VOP_READLINK");
    ASSERT_VOP_LOCKED(vp, "VOP_READLINK");
    rc = VCALL(vp, VOFFSET(vop_readlink), &a);
    CTR3(KTR_VOP, "VOP_READLINK(vp 0x%lX, uio 0x%lX, cred 0x%lX)", vp, uio,
	 cred);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_READLINK");
	ASSERT_VOP_LOCKED(vp, "VOP_READLINK");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_READLINK");
	ASSERT_VOP_LOCKED(vp, "VOP_READLINK");
    }
    return (rc);
}
struct vop_inactive_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_inactive_desc;
static __inline int
VOP_INACTIVE(struct vnode *vp, struct thread *td)
{
    struct vop_inactive_args a;
    int rc;
    a.a_desc = VDESC(vop_inactive);
    a.a_vp = vp;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_INACTIVE");
    ASSERT_VOP_LOCKED(vp, "VOP_INACTIVE");
    rc = VCALL(vp, VOFFSET(vop_inactive), &a);
    CTR2(KTR_VOP, "VOP_INACTIVE(vp 0x%lX, td 0x%lX)", vp, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_INACTIVE");
	ASSERT_VOP_UNLOCKED(vp, "VOP_INACTIVE");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_INACTIVE");
	ASSERT_VOP_UNLOCKED(vp, "VOP_INACTIVE");
    }
    return (rc);
}
struct vop_reclaim_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_reclaim_desc;
static __inline int
VOP_RECLAIM(struct vnode *vp, struct thread *td)
{
    struct vop_reclaim_args a;
    int rc;
    a.a_desc = VDESC(vop_reclaim);
    a.a_vp = vp;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_RECLAIM");
    ASSERT_VOP_UNLOCKED(vp, "VOP_RECLAIM");
    rc = VCALL(vp, VOFFSET(vop_reclaim), &a);
    CTR2(KTR_VOP, "VOP_RECLAIM(vp 0x%lX, td 0x%lX)", vp, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_RECLAIM");
	ASSERT_VOP_UNLOCKED(vp, "VOP_RECLAIM");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_RECLAIM");
	ASSERT_VOP_UNLOCKED(vp, "VOP_RECLAIM");
    }
    return (rc);
}
struct vop_lock_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_flags;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_lock_desc;
static __inline int
VOP_LOCK(struct vnode *vp, int flags, struct thread *td)
{
    struct vop_lock_args a;
    int rc;
    a.a_desc = VDESC(vop_lock);
    a.a_vp = vp;
    a.a_flags = flags;
    a.a_td = td;
#ifdef	DEBUG_VFS_LOCKS
    vop_lock_pre(&a);
#endif
    rc = VCALL(vp, VOFFSET(vop_lock), &a);
    CTR3(KTR_VOP, "VOP_LOCK(vp 0x%lX, flags %ld, td 0x%lX)", vp, flags, td);
    if (rc == 0) {
    } else {
    }
#ifdef	DEBUG_VFS_LOCKS
    vop_lock_post(&a, rc);
#endif
    return (rc);
}
struct vop_unlock_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_flags;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_unlock_desc;
static __inline int
VOP_UNLOCK(struct vnode *vp, int flags, struct thread *td)
{
    struct vop_unlock_args a;
    int rc;
    a.a_desc = VDESC(vop_unlock);
    a.a_vp = vp;
    a.a_flags = flags;
    a.a_td = td;
#ifdef	DEBUG_VFS_LOCKS
    vop_unlock_pre(&a);
#endif
    rc = VCALL(vp, VOFFSET(vop_unlock), &a);
    CTR3(KTR_VOP, "VOP_UNLOCK(vp 0x%lX, flags %ld, td 0x%lX)", vp, flags, td);
    if (rc == 0) {
    } else {
    }
#ifdef	DEBUG_VFS_LOCKS
    vop_unlock_post(&a, rc);
#endif
    return (rc);
}
struct vop_bmap_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    daddr_t a_bn;
    struct vnode **a_vpp;
    daddr_t *a_bnp;
    int *a_runp;
    int *a_runb;
};
extern struct vnodeop_desc vop_bmap_desc;
static __inline int
VOP_BMAP(struct vnode *vp, daddr_t bn, struct vnode **vpp, daddr_t * bnp,
	 int *runp, int *runb)
{
    struct vop_bmap_args a;
    int rc;
    a.a_desc = VDESC(vop_bmap);
    a.a_vp = vp;
    a.a_bn = bn;
    a.a_vpp = vpp;
    a.a_bnp = bnp;
    a.a_runp = runp;
    a.a_runb = runb;
    ASSERT_VI_UNLOCKED(vp, "VOP_BMAP");
    ASSERT_VOP_LOCKED(vp, "VOP_BMAP");
    rc = VCALL(vp, VOFFSET(vop_bmap), &a);
    CTR6(KTR_VOP,
	 "VOP_BMAP(vp 0x%lX, bn %ld, vpp 0x%lX, bnp 0x%lX, runp 0x%lX, runb 0x%lX)",
	 vp, bn, vpp, bnp, runp, runb);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_BMAP");
	ASSERT_VOP_LOCKED(vp, "VOP_BMAP");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_BMAP");
	ASSERT_VOP_LOCKED(vp, "VOP_BMAP");
    }
    return (rc);
}
struct vop_strategy_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct buf *a_bp;
};
extern struct vnodeop_desc vop_strategy_desc;
static __inline int
VOP_STRATEGY(struct vnode *vp, struct buf *bp)
{
    struct vop_strategy_args a;
    int rc;
    a.a_desc = VDESC(vop_strategy);
    a.a_vp = vp;
    a.a_bp = bp;
#ifdef	DEBUG_VFS_LOCKS
    vop_strategy_pre(&a);
#endif
    rc = VCALL(vp, VOFFSET(vop_strategy), &a);
    CTR2(KTR_VOP, "VOP_STRATEGY(vp 0x%lX, bp 0x%lX)", vp, bp);
    if (rc == 0) {
    } else {
    }
    return (rc);
}
struct vop_getwritemount_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct mount **a_mpp;
};
extern struct vnodeop_desc vop_getwritemount_desc;
static __inline int
VOP_GETWRITEMOUNT(struct vnode *vp, struct mount **mpp)
{
    struct vop_getwritemount_args a;
    int rc;
    a.a_desc = VDESC(vop_getwritemount);
    a.a_vp = vp;
    a.a_mpp = mpp;
    ASSERT_VI_UNLOCKED(vp, "VOP_GETWRITEMOUNT");
    rc = VCALL(vp, VOFFSET(vop_getwritemount), &a);
    CTR2(KTR_VOP, "VOP_GETWRITEMOUNT(vp 0x%lX, mpp 0x%lX)", vp, mpp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETWRITEMOUNT");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETWRITEMOUNT");
    }
    return (rc);
}
struct vop_print_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
};
extern struct vnodeop_desc vop_print_desc;
static __inline int
VOP_PRINT(struct vnode *vp)
{
    struct vop_print_args a;
    int rc;
    a.a_desc = VDESC(vop_print);
    a.a_vp = vp;
    ASSERT_VI_UNLOCKED(vp, "VOP_PRINT");
    rc = VCALL(vp, VOFFSET(vop_print), &a);
    CTR1(KTR_VOP, "VOP_PRINT(vp 0x%lX)", vp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_PRINT");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_PRINT");
    }
    return (rc);
}
struct vop_pathconf_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_name;
    register_t *a_retval;
};
extern struct vnodeop_desc vop_pathconf_desc;
static __inline int
VOP_PATHCONF(struct vnode *vp, int name, register_t * retval)
{
    struct vop_pathconf_args a;
    int rc;
    a.a_desc = VDESC(vop_pathconf);
    a.a_vp = vp;
    a.a_name = name;
    a.a_retval = retval;
    ASSERT_VI_UNLOCKED(vp, "VOP_PATHCONF");
    ASSERT_VOP_LOCKED(vp, "VOP_PATHCONF");
    rc = VCALL(vp, VOFFSET(vop_pathconf), &a);
    CTR3(KTR_VOP, "VOP_PATHCONF(vp 0x%lX, name %ld, retval 0x%lX)", vp, name,
	 retval);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_PATHCONF");
	ASSERT_VOP_LOCKED(vp, "VOP_PATHCONF");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_PATHCONF");
	ASSERT_VOP_LOCKED(vp, "VOP_PATHCONF");
    }
    return (rc);
}
struct vop_advlock_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    caddr_t a_id;
    int a_op;
    struct flock *a_fl;
    int a_flags;
};
extern struct vnodeop_desc vop_advlock_desc;
static __inline int
VOP_ADVLOCK(struct vnode *vp, caddr_t id, int op, struct flock *fl, int flags)
{
    struct vop_advlock_args a;
    int rc;
    a.a_desc = VDESC(vop_advlock);
    a.a_vp = vp;
    a.a_id = id;
    a.a_op = op;
    a.a_fl = fl;
    a.a_flags = flags;
    ASSERT_VI_UNLOCKED(vp, "VOP_ADVLOCK");
    ASSERT_VOP_UNLOCKED(vp, "VOP_ADVLOCK");
    rc = VCALL(vp, VOFFSET(vop_advlock), &a);
    CTR5(KTR_VOP,
	 "VOP_ADVLOCK(vp 0x%lX, id %ld, op %ld, fl 0x%lX, flags %ld)", vp, id,
	 op, fl, flags);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_ADVLOCK");
	ASSERT_VOP_UNLOCKED(vp, "VOP_ADVLOCK");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_ADVLOCK");
	ASSERT_VOP_UNLOCKED(vp, "VOP_ADVLOCK");
    }
    return (rc);
}
struct vop_reallocblks_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct cluster_save *a_buflist;
};
extern struct vnodeop_desc vop_reallocblks_desc;
static __inline int
VOP_REALLOCBLKS(struct vnode *vp, struct cluster_save *buflist)
{
    struct vop_reallocblks_args a;
    int rc;
    a.a_desc = VDESC(vop_reallocblks);
    a.a_vp = vp;
    a.a_buflist = buflist;
    ASSERT_VI_UNLOCKED(vp, "VOP_REALLOCBLKS");
    ASSERT_VOP_LOCKED(vp, "VOP_REALLOCBLKS");
    rc = VCALL(vp, VOFFSET(vop_reallocblks), &a);
    CTR2(KTR_VOP, "VOP_REALLOCBLKS(vp 0x%lX, buflist 0x%lX)", vp, buflist);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_REALLOCBLKS");
	ASSERT_VOP_LOCKED(vp, "VOP_REALLOCBLKS");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_REALLOCBLKS");
	ASSERT_VOP_LOCKED(vp, "VOP_REALLOCBLKS");
    }
    return (rc);
}
struct vop_getpages_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    vm_page_t *a_m;
    int a_count;
    int a_reqpage;
    vm_ooffset_t a_offset;
};
extern struct vnodeop_desc vop_getpages_desc;
static __inline int
VOP_GETPAGES(struct vnode *vp, vm_page_t * m, int count, int reqpage,
	     vm_ooffset_t offset)
{
    struct vop_getpages_args a;
    int rc;
    a.a_desc = VDESC(vop_getpages);
    a.a_vp = vp;
    a.a_m = m;
    a.a_count = count;
    a.a_reqpage = reqpage;
    a.a_offset = offset;
    ASSERT_VI_UNLOCKED(vp, "VOP_GETPAGES");
    ASSERT_VOP_LOCKED(vp, "VOP_GETPAGES");
    rc = VCALL(vp, VOFFSET(vop_getpages), &a);
    CTR5(KTR_VOP,
	 "VOP_GETPAGES(vp 0x%lX, m 0x%lX, count %ld, reqpage %ld, offset %ld)",
	 vp, m, count, reqpage, offset);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETPAGES");
	ASSERT_VOP_LOCKED(vp, "VOP_GETPAGES");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETPAGES");
	ASSERT_VOP_LOCKED(vp, "VOP_GETPAGES");
    }
    return (rc);
}
struct vop_putpages_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    vm_page_t *a_m;
    int a_count;
    int a_sync;
    int *a_rtvals;
    vm_ooffset_t a_offset;
};
extern struct vnodeop_desc vop_putpages_desc;
static __inline int
VOP_PUTPAGES(struct vnode *vp, vm_page_t * m, int count, int sync,
	     int *rtvals, vm_ooffset_t offset)
{
    struct vop_putpages_args a;
    int rc;
    a.a_desc = VDESC(vop_putpages);
    a.a_vp = vp;
    a.a_m = m;
    a.a_count = count;
    a.a_sync = sync;
    a.a_rtvals = rtvals;
    a.a_offset = offset;
    ASSERT_VI_UNLOCKED(vp, "VOP_PUTPAGES");
    ASSERT_VOP_LOCKED(vp, "VOP_PUTPAGES");
    rc = VCALL(vp, VOFFSET(vop_putpages), &a);
    CTR6(KTR_VOP,
	 "VOP_PUTPAGES(vp 0x%lX, m 0x%lX, count %ld, sync %ld, rtvals 0x%lX, offset %ld)",
	 vp, m, count, sync, rtvals, offset);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_PUTPAGES");
	ASSERT_VOP_LOCKED(vp, "VOP_PUTPAGES");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_PUTPAGES");
	ASSERT_VOP_LOCKED(vp, "VOP_PUTPAGES");
    }
    return (rc);
}
struct vop_freeblks_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    daddr_t a_addr;
    daddr_t a_length;
};
extern struct vnodeop_desc vop_freeblks_desc;
static __inline int
VOP_FREEBLKS(struct vnode *vp, daddr_t addr, daddr_t length)
{
    struct vop_freeblks_args a;
    int rc;
    a.a_desc = VDESC(vop_freeblks);
    a.a_vp = vp;
    a.a_addr = addr;
    a.a_length = length;
    ASSERT_VI_UNLOCKED(vp, "VOP_FREEBLKS");
    rc = VCALL(vp, VOFFSET(vop_freeblks), &a);
    CTR3(KTR_VOP, "VOP_FREEBLKS(vp 0x%lX, addr %ld, length %ld)", vp, addr,
	 length);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_FREEBLKS");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_FREEBLKS");
    }
    return (rc);
}
struct vop_getacl_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    acl_type_t a_type;
    struct acl *a_aclp;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_getacl_desc;
static __inline int
VOP_GETACL(struct vnode *vp, acl_type_t type, struct acl *aclp,
	   struct ucred *cred, struct thread *td)
{
    struct vop_getacl_args a;
    int rc;
    a.a_desc = VDESC(vop_getacl);
    a.a_vp = vp;
    a.a_type = type;
    a.a_aclp = aclp;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_GETACL");
    ASSERT_VOP_LOCKED(vp, "VOP_GETACL");
    rc = VCALL(vp, VOFFSET(vop_getacl), &a);
    CTR5(KTR_VOP,
	 "VOP_GETACL(vp 0x%lX, type %ld, aclp 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, type, aclp, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETACL");
	ASSERT_VOP_LOCKED(vp, "VOP_GETACL");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETACL");
	ASSERT_VOP_LOCKED(vp, "VOP_GETACL");
    }
    return (rc);
}
struct vop_setacl_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    acl_type_t a_type;
    struct acl *a_aclp;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_setacl_desc;
static __inline int
VOP_SETACL(struct vnode *vp, acl_type_t type, struct acl *aclp,
	   struct ucred *cred, struct thread *td)
{
    struct vop_setacl_args a;
    int rc;
    a.a_desc = VDESC(vop_setacl);
    a.a_vp = vp;
    a.a_type = type;
    a.a_aclp = aclp;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_SETACL");
    ASSERT_VOP_LOCKED(vp, "VOP_SETACL");
    rc = VCALL(vp, VOFFSET(vop_setacl), &a);
    CTR5(KTR_VOP,
	 "VOP_SETACL(vp 0x%lX, type %ld, aclp 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, type, aclp, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETACL");
	ASSERT_VOP_LOCKED(vp, "VOP_SETACL");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETACL");
	ASSERT_VOP_LOCKED(vp, "VOP_SETACL");
    }
    return (rc);
}
struct vop_aclcheck_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    acl_type_t a_type;
    struct acl *a_aclp;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_aclcheck_desc;
static __inline int
VOP_ACLCHECK(struct vnode *vp, acl_type_t type, struct acl *aclp,
	     struct ucred *cred, struct thread *td)
{
    struct vop_aclcheck_args a;
    int rc;
    a.a_desc = VDESC(vop_aclcheck);
    a.a_vp = vp;
    a.a_type = type;
    a.a_aclp = aclp;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_ACLCHECK");
    rc = VCALL(vp, VOFFSET(vop_aclcheck), &a);
    CTR5(KTR_VOP,
	 "VOP_ACLCHECK(vp 0x%lX, type %ld, aclp 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, type, aclp, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_ACLCHECK");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_ACLCHECK");
    }
    return (rc);
}
struct vop_closeextattr_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_commit;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_closeextattr_desc;
static __inline int
VOP_CLOSEEXTATTR(struct vnode *vp, int commit, struct ucred *cred,
		 struct thread *td)
{
    struct vop_closeextattr_args a;
    int rc;
    a.a_desc = VDESC(vop_closeextattr);
    a.a_vp = vp;
    a.a_commit = commit;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_CLOSEEXTATTR");
    ASSERT_VOP_LOCKED(vp, "VOP_CLOSEEXTATTR");
    rc = VCALL(vp, VOFFSET(vop_closeextattr), &a);
    CTR4(KTR_VOP,
	 "VOP_CLOSEEXTATTR(vp 0x%lX, commit %ld, cred 0x%lX, td 0x%lX)", vp,
	 commit, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_CLOSEEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_CLOSEEXTATTR");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_CLOSEEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_CLOSEEXTATTR");
    }
    return (rc);
}
struct vop_getextattr_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_attrnamespace;
    const char *a_name;
    struct uio *a_uio;
    size_t *a_size;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_getextattr_desc;
static __inline int
VOP_GETEXTATTR(struct vnode *vp, int attrnamespace, const char *name,
	       struct uio *uio, size_t * size, struct ucred *cred,
	       struct thread *td)
{
    struct vop_getextattr_args a;
    int rc;
    a.a_desc = VDESC(vop_getextattr);
    a.a_vp = vp;
    a.a_attrnamespace = attrnamespace;
    a.a_name = name;
    a.a_uio = uio;
    a.a_size = size;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_GETEXTATTR");
    ASSERT_VOP_LOCKED(vp, "VOP_GETEXTATTR");
    rc = VCALL(vp, VOFFSET(vop_getextattr), &a);
    CTR6(KTR_VOP,
	 "VOP_GETEXTATTR(vp 0x%lX, attrnamespace %ld, name 0x%lX, uio 0x%lX, size 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, attrnamespace, name, uio, size, cred);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_GETEXTATTR");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_GETEXTATTR");
    }
    return (rc);
}
struct vop_openextattr_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_openextattr_desc;
static __inline int
VOP_OPENEXTATTR(struct vnode *vp, struct ucred *cred, struct thread *td)
{
    struct vop_openextattr_args a;
    int rc;
    a.a_desc = VDESC(vop_openextattr);
    a.a_vp = vp;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_OPENEXTATTR");
    ASSERT_VOP_LOCKED(vp, "VOP_OPENEXTATTR");
    rc = VCALL(vp, VOFFSET(vop_openextattr), &a);
    CTR3(KTR_VOP, "VOP_OPENEXTATTR(vp 0x%lX, cred 0x%lX, td 0x%lX)", vp, cred,
	 td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_OPENEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_OPENEXTATTR");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_OPENEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_OPENEXTATTR");
    }
    return (rc);
}
struct vop_setextattr_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    int a_attrnamespace;
    const char *a_name;
    struct uio *a_uio;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_setextattr_desc;
static __inline int
VOP_SETEXTATTR(struct vnode *vp, int attrnamespace, const char *name,
	       struct uio *uio, struct ucred *cred, struct thread *td)
{
    struct vop_setextattr_args a;
    int rc;
    a.a_desc = VDESC(vop_setextattr);
    a.a_vp = vp;
    a.a_attrnamespace = attrnamespace;
    a.a_name = name;
    a.a_uio = uio;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_SETEXTATTR");
    ASSERT_VOP_LOCKED(vp, "VOP_SETEXTATTR");
    rc = VCALL(vp, VOFFSET(vop_setextattr), &a);
    CTR6(KTR_VOP,
	 "VOP_SETEXTATTR(vp 0x%lX, attrnamespace %ld, name 0x%lX, uio 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, attrnamespace, name, uio, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_SETEXTATTR");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETEXTATTR");
	ASSERT_VOP_LOCKED(vp, "VOP_SETEXTATTR");
    }
    return (rc);
}
struct vop_createvobject_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_createvobject_desc;
static __inline int
VOP_CREATEVOBJECT(struct vnode *vp, struct ucred *cred, struct thread *td)
{
    struct vop_createvobject_args a;
    int rc;
    a.a_desc = VDESC(vop_createvobject);
    a.a_vp = vp;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_CREATEVOBJECT");
    ASSERT_VOP_LOCKED(vp, "VOP_CREATEVOBJECT");
    rc = VCALL(vp, VOFFSET(vop_createvobject), &a);
    CTR3(KTR_VOP, "VOP_CREATEVOBJECT(vp 0x%lX, cred 0x%lX, td 0x%lX)", vp,
	 cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_CREATEVOBJECT");
	ASSERT_VOP_LOCKED(vp, "VOP_CREATEVOBJECT");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_CREATEVOBJECT");
	ASSERT_VOP_LOCKED(vp, "VOP_CREATEVOBJECT");
    }
    return (rc);
}
struct vop_destroyvobject_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
};
extern struct vnodeop_desc vop_destroyvobject_desc;
static __inline int
VOP_DESTROYVOBJECT(struct vnode *vp)
{
    struct vop_destroyvobject_args a;
    int rc;
    a.a_desc = VDESC(vop_destroyvobject);
    a.a_vp = vp;
    ASSERT_VI_UNLOCKED(vp, "VOP_DESTROYVOBJECT");
    ASSERT_VOP_LOCKED(vp, "VOP_DESTROYVOBJECT");
    rc = VCALL(vp, VOFFSET(vop_destroyvobject), &a);
    CTR1(KTR_VOP, "VOP_DESTROYVOBJECT(vp 0x%lX)", vp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_DESTROYVOBJECT");
	ASSERT_VOP_LOCKED(vp, "VOP_DESTROYVOBJECT");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_DESTROYVOBJECT");
	ASSERT_VOP_LOCKED(vp, "VOP_DESTROYVOBJECT");
    }
    return (rc);
}
struct vop_getvobject_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct vm_object **a_objpp;
};
extern struct vnodeop_desc vop_getvobject_desc;
static __inline int
VOP_GETVOBJECT(struct vnode *vp, struct vm_object **objpp)
{
    struct vop_getvobject_args a;
    int rc;
    a.a_desc = VDESC(vop_getvobject);
    a.a_vp = vp;
    a.a_objpp = objpp;
    ASSERT_VI_UNLOCKED(vp, "VOP_GETVOBJECT");
    ASSERT_VOP_LOCKED(vp, "VOP_GETVOBJECT");
    rc = VCALL(vp, VOFFSET(vop_getvobject), &a);
    CTR2(KTR_VOP, "VOP_GETVOBJECT(vp 0x%lX, objpp 0x%lX)", vp, objpp);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETVOBJECT");
	ASSERT_VOP_LOCKED(vp, "VOP_GETVOBJECT");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_GETVOBJECT");
	ASSERT_VOP_LOCKED(vp, "VOP_GETVOBJECT");
    }
    return (rc);
}
struct vop_refreshlabel_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_refreshlabel_desc;
static __inline int
VOP_REFRESHLABEL(struct vnode *vp, struct ucred *cred, struct thread *td)
{
    struct vop_refreshlabel_args a;
    int rc;
    a.a_desc = VDESC(vop_refreshlabel);
    a.a_vp = vp;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_REFRESHLABEL");
    ASSERT_VOP_LOCKED(vp, "VOP_REFRESHLABEL");
    rc = VCALL(vp, VOFFSET(vop_refreshlabel), &a);
    CTR3(KTR_VOP, "VOP_REFRESHLABEL(vp 0x%lX, cred 0x%lX, td 0x%lX)", vp,
	 cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_REFRESHLABEL");
	ASSERT_VOP_LOCKED(vp, "VOP_REFRESHLABEL");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_REFRESHLABEL");
	ASSERT_VOP_LOCKED(vp, "VOP_REFRESHLABEL");
    }
    return (rc);
}
struct vop_setlabel_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct label *a_label;
    struct ucred *a_cred;
    struct thread *a_td;
};
extern struct vnodeop_desc vop_setlabel_desc;
static __inline int
VOP_SETLABEL(struct vnode *vp, struct label *label, struct ucred *cred,
	     struct thread *td)
{
    struct vop_setlabel_args a;
    int rc;
    a.a_desc = VDESC(vop_setlabel);
    a.a_vp = vp;
    a.a_label = label;
    a.a_cred = cred;
    a.a_td = td;
    ASSERT_VI_UNLOCKED(vp, "VOP_SETLABEL");
    ASSERT_VOP_LOCKED(vp, "VOP_SETLABEL");
    rc = VCALL(vp, VOFFSET(vop_setlabel), &a);
    CTR4(KTR_VOP, "VOP_SETLABEL(vp 0x%lX, label 0x%lX, cred 0x%lX, td 0x%lX)",
	 vp, label, cred, td);
    if (rc == 0) {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETLABEL");
	ASSERT_VOP_LOCKED(vp, "VOP_SETLABEL");
    } else {
	ASSERT_VI_UNLOCKED(vp, "VOP_SETLABEL");
	ASSERT_VOP_LOCKED(vp, "VOP_SETLABEL");
    }
    return (rc);
}
