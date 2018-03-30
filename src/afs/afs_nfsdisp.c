/*
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 */
#include <afsconfig.h>
#include "afs/param.h"


/* Ugly Ugly Ugly  but precludes conflicting XDR macros; We want kernel xdr */
#define __XDR_INCLUDE__
#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#if defined(AFS_SUN5_ENV) && !defined(AFS_NONFSTRANS)
#include "rpc/types.h"
#include "rpc/auth.h"
#include "rpc/auth_unix.h"
#include "rpc/auth_des.h"
#include "sys/tiuser.h"
#include "rpc/svc.h"
#include "nfs/nfs.h"

#include "nfs/export.h"
/* Solaris 11.1 defines areq to areq_u.areq (and auid to areq_u.auid), for
 * shortcut accessors to the nfsauth_arg structure. Since we dare to use the
 * names areq and auid as parameter names in a lot of functions, work around
 * this by undefining it. */
#ifdef areq
# undef areq
#endif
#ifdef auid
# undef auid
#endif

#include "nfs/nfs_clnt.h"
#include "nfs/nfs_acl.h"
#include "afs/afsincludes.h"
#include "afs/afs_stats.h"
#include "afs/exporter.h"

static int xlatorinit_v2_done = 0;
static int xlatorinit_v3_done = 0;
extern int afs_nobody;
extern int afs_NFSRootOnly;

struct rfs_disp_tbl {
    void (*dis_proc) ();
    xdrproc_t dis_xdrargs;
    xdrproc_t dis_fastxdrargs;
    int dis_argsz;
    xdrproc_t dis_xdrres;
    xdrproc_t dis_fastxdrres;
    int dis_ressz;
    void (*dis_resfree) ();
    int dis_flags;
      fhandle_t(*dis_getfh) ();
};

struct afs_nfs_disp_tbl {
    void (*afs_proc) ();
    void (*orig_proc) ();
};
struct afs_nfs2_resp {
    enum nfsstat status;
};

#ifndef ACL2_NPROC
#if defined(AFS_SUN510_ENV)
#define ACL2_NPROC      6
#else
#define ACL2_NPROC      5
#endif
#endif
struct afs_nfs_disp_tbl afs_rfs_disp_tbl[RFS_NPROC];
struct afs_nfs_disp_tbl afs_acl_disp_tbl[ACL2_NPROC];

static int
is_afs_fh(fhandle_t * fhp)
{
    if ((fhp->fh_fsid.val[0] == AFS_VFSMAGIC)
	&& (fhp->fh_fsid.val[1] == AFS_VFSFSID))
	return 1;
    return 0;
}

afs_int32
nfs2_to_afs_call(int which, void *args, fhandle_t ** fhpp,
		 fhandle_t ** fh2pp)
{
    struct vnode *vp;
    fhandle_t *fhp1 = 0;
    fhandle_t *fhp2 = 0;
    int errorcode;

    afs_Trace1(afs_iclSetp, CM_TRACE_NFSIN, ICL_TYPE_INT32, which);
    *fh2pp = NULL;
    switch (which) {
    case RFS_GETATTR:
    case RFS_READLINK:
    case RFS_STATFS:
	fhp1 = args;
	break;
    case RFS_SETATTR:
	{
	    struct nfssaargs *sargs = args;
	    fhp1 = (fhandle_t *) & sargs->saa_fh;
	    break;
	}
    case RFS_LOOKUP:
	{
	    struct nfsdiropargs *sargs = args;
	    fhp1 = sargs->da_fhandle;
	    break;
	}
    case RFS_READ:
	{
	    struct nfsreadargs *sargs = args;
	    fhp1 = (fhandle_t *) & sargs->ra_fhandle;
	    break;
	}
    case RFS_WRITE:
	{
	    struct nfswriteargs *sargs = args;
	    fhp1 = (fhandle_t *) & sargs->wa_fhandle;
	    break;
	}
    case RFS_CREATE:
	{
	    struct nfscreatargs *sargs = args;
	    fhp1 = sargs->ca_da.da_fhandle;
	    break;
	}
    case RFS_REMOVE:
	{
	    struct nfsdiropargs *sargs = args;
	    fhp1 = sargs->da_fhandle;
	    break;
	}
    case RFS_RENAME:
	{
	    struct nfsrnmargs *sargs = args;
	    fhp1 = sargs->rna_from.da_fhandle;
	    fhp2 = sargs->rna_to.da_fhandle;
	    break;
	}
    case RFS_LINK:
	{
	    struct nfslinkargs *sargs = args;
	    fhp1 = sargs->la_from;
	    fhp2 = sargs->la_to.da_fhandle;
	    break;
	}
    case RFS_SYMLINK:
	{
	    struct nfsslargs *sargs = args;
	    fhp1 = sargs->sla_from.da_fhandle;
	    break;
	}
    case RFS_MKDIR:
	{
	    struct nfscreatargs *sargs = args;
	    fhp1 = sargs->ca_da.da_fhandle;
	    break;
	}
    case RFS_RMDIR:
	{
	    struct nfsdiropargs *sargs = args;
	    fhp1 = sargs->da_fhandle;
	    break;
	}
    case RFS_READDIR:
	{
	    struct nfsrddirargs *sargs = args;
	    fhp1 = (fhandle_t *) & sargs->rda_fh;
	    break;
	}
    default:
	return NULL;
    }

    /* Ok if arg 1 is in AFS or if 2 args and arg 2 is in AFS */
    if (fhp1 && is_afs_fh(fhp1)) {
	*fhpp = fhp1;
	if (fhp2)
	    *fh2pp = fhp2;
	return 1;
    }
    if (fhp2 && is_afs_fh(fhp2)) {
	*fhpp = fhp1;
	*fh2pp = fhp2;
	return 1;
    }
    return NULL;
}

afs_int32
acl2_to_afs_call(int which, void *args, fhandle_t ** fhpp)
{
    fhandle_t *fhp;

    switch (which) {
    case ACLPROC2_NULL:
	{
	    return NULL;
	}
    case ACLPROC2_GETACL:
	{
	    struct GETACL2args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
    case ACLPROC2_SETACL:
	{
	    struct SETACL2args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
    case ACLPROC2_GETATTR:
	{
	    struct GETATTR2args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
    case ACLPROC2_ACCESS:
	{
	    struct ACCESS2args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
#if defined(AFS_SUN510_ENV)
    case ACLPROC2_GETXATTRDIR:
	{
	    struct GETXATTRDIR2args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
#endif
    default:
	return NULL;
    }

    if (fhp && is_afs_fh(fhp)) {
	*fhpp = fhp;
	return 1;
    }

    return NULL;
}

int
afs_nfs2_dispatcher(int type, afs_int32 which, void *argp,
		    struct exportinfo *exp, struct svc_req *rp,
		    afs_ucred_t *crp)
{
    afs_int32 call = 0;
    afs_int32 code = 0;
    afs_int32 client = 0;
    struct sockaddr *sa;
    fhandle_t *fh = argp;
    fhandle_t *fh2 = NULL;

    if (!xlatorinit_v2_done)
	return 2;

    sa = (struct sockaddr *)svc_getrpccaller(rp->rq_xprt)->buf;
    if (sa->sa_family == AF_INET)
	client = ((struct sockaddr_in *)sa)->sin_addr.s_addr;

    AFS_GLOCK();
    code = 0;
    switch (type) {
    case 0:
	code = (client && nfs2_to_afs_call(which, argp, &fh, &fh2));
	break;
    case 1:
	code = (client && acl2_to_afs_call(which, argp, &fh));
	break;
    default:
	break;
    }

    if (code) {
	struct afs_exporter *out = 0;
	afs_int32 dummy;
	static int once = 0;
	struct SmallFid Sfid;

	memcpy(&Sfid, fh->fh_data, SIZEOF_SMALLFID);

	afs_Trace2(afs_iclSetp, CM_TRACE_NFSIN1, ICL_TYPE_POINTER, client,
		   ICL_TYPE_FID, &Sfid);

	/* We ran */
	call = 1;
	if (!once && exp) {
	    afs_nobody = exp->exi_export.ex_anon;
	    once = 1;
	}
	code =
	    afs_nfsclient_reqhandler(NULL, &crp, client, &dummy, &out);

	if (!code && out)
	    EXP_RELE(out);

	if (code == EINVAL)
	    call = 2;
    }

    AFS_GUNLOCK();
    return call;
}

void
afs_nfs2_smallfidder(struct nfsdiropres *dr)
{
    fhandle_t *fhp = (fhandle_t *) & dr->dr_fhandle;
    afs_int32 addr[2];
    struct vcache *vcp;

#if defined(AFS_SUN5_64BIT_ENV)
    /* See also afs_fid() */
    memcpy(addr, fhp->fh_data, SIZEOF_SMALLFID);
    addr[1] = (addr[1] >> 48) & 0xffff;
#else
    memcpy(addr, fhp->fh_data, 2 * sizeof(long));
#endif

    AFS_GLOCK();
    vcp = VTOAFS((struct vnode *)addr[0]);

    if (addr[1] == AFS_XLATOR_MAGIC) {
	if (dr->dr_status == NFS_OK) {
	    struct SmallFid Sfid;
	    struct cell *tcell;

	    /* Make up and copy out a SmallFid */
	    tcell = afs_GetCell(vcp->f.fid.Cell, READ_LOCK);
	    Sfid.Volume = vcp->f.fid.Fid.Volume;
	    Sfid.CellAndUnique =
		((tcell->cellIndex << 24) | (vcp->f.fid.Fid.Unique & 0xffffff));
	    afs_PutCell(tcell, READ_LOCK);
	    Sfid.Vnode = (u_short) (vcp->f.fid.Fid.Vnode & 0xffff);
	    fhp->fh_len = SIZEOF_SMALLFID;
	    memcpy(dr->dr_fhandle.fh_data, &Sfid, fhp->fh_len);

	    afs_Trace3(afs_iclSetp, CM_TRACE_NFSOUT, ICL_TYPE_INT32, 0,
		       ICL_TYPE_POINTER, vcp, ICL_TYPE_FID, &Sfid);
	}

	/* If we have a ref, release it */
	if (vcp->vrefCount >= 1)
	    AFS_RELE(AFSTOV(vcp));
    }
    AFS_GUNLOCK();
}

void
afs_nfs2_noaccess(struct afs_nfs2_resp *resp)
{
    resp->status = NFSERR_ACCES;
}

void
afs_nfs2_null(void *args, void *xp, void *exp, void *rp, void *crp)
{
}

void
afs_nfs2_root(void *args, void *xp, void *exp, void *rp, void *crp)
{
}

void
afs_nfs2_writecache(void *args, void *xp, void *exp, void *rp, void *crp)
{
}

void
afs_nfs2_getattr(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_GETATTR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_GETATTR].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_setattr(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_SETATTR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_SETATTR].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_lookup(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_LOOKUP, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else {
	(*afs_rfs_disp_tbl[RFS_LOOKUP].orig_proc) (args, xp, exp, rp, crp);
	if (afs_NFSRootOnly && call)
	    afs_nfs2_smallfidder(xp);
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_readlink(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_READLINK, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_READLINK].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_read(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_READ, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_READ].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_write(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_WRITE, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_WRITE].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_create(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_CREATE, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else {
	(*afs_rfs_disp_tbl[RFS_CREATE].orig_proc) (args, xp, exp, rp, crp);
	if (afs_NFSRootOnly && call)
	    afs_nfs2_smallfidder(xp);
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_remove(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_REMOVE, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_REMOVE].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_rename(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_RENAME, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_RENAME].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_link(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_LINK, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_LINK].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_symlink(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_SYMLINK, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_SYMLINK].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_mkdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_MKDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else {
	(*afs_rfs_disp_tbl[RFS_MKDIR].orig_proc) (args, xp, exp, rp, crp);
	if (afs_NFSRootOnly && call)
	    afs_nfs2_smallfidder(xp);
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_rmdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_RMDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_RMDIR].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_readdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_READDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_READDIR].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs2_statfs(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs2_dispatcher(0, RFS_STATFS, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_rfs_disp_tbl[RFS_STATFS].orig_proc) (args, xp, exp, rp, crp);
    curthread->t_cred = svcred;
    return;
}

struct afs_nfs_disp_tbl afs_rfs_disp_tbl[RFS_NPROC] = {
    {afs_nfs2_null},
    {afs_nfs2_getattr},
    {afs_nfs2_setattr},
    {afs_nfs2_root},
    {afs_nfs2_lookup},
    {afs_nfs2_readlink},
    {afs_nfs2_read},
    {afs_nfs2_writecache},
    {afs_nfs2_write},
    {afs_nfs2_create},
    {afs_nfs2_remove},
    {afs_nfs2_rename},
    {afs_nfs2_link},
    {afs_nfs2_symlink},
    {afs_nfs2_mkdir},
    {afs_nfs2_rmdir},
    {afs_nfs2_readdir},
    {afs_nfs2_statfs}
};

void
afs_acl2_getacl(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs2_dispatcher(1, ACLPROC2_GETACL, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_acl_disp_tbl[ACLPROC2_GETACL].orig_proc) (args, xp, exp, rp,
							crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_acl2_setacl(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs2_dispatcher(1, ACLPROC2_SETACL, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_acl_disp_tbl[ACLPROC2_SETACL].orig_proc) (args, xp, exp, rp,
							crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_acl2_getattr(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs2_dispatcher(1, ACLPROC2_GETATTR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_acl_disp_tbl[ACLPROC2_GETATTR].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_acl2_access(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs2_dispatcher(1, ACLPROC2_ACCESS, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_acl_disp_tbl[ACLPROC2_ACCESS].orig_proc) (args, xp, exp, rp,
							crp);
    curthread->t_cred = svcred;
    return;
}

#if defined(AFS_SUN510_ENV)
void
afs_acl2_getxattrdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs2_dispatcher(1, ACLPROC2_GETXATTRDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs2_noaccess(xp);
    else
	(*afs_acl_disp_tbl[ACLPROC2_GETXATTRDIR].orig_proc) (args, xp, exp, rp,
							crp);
    curthread->t_cred = svcred;
    return;
}
#endif

struct afs_nfs_disp_tbl afs_acl_disp_tbl[ACL2_NPROC] = {
    {afs_nfs2_null},
    {afs_acl2_getacl},
    {afs_acl2_setacl},
    {afs_acl2_getattr},
    {afs_acl2_access},
#if defined(AFS_SUN510_ENV)
    {afs_acl2_getxattrdir}
#endif
};

/* Munge the dispatch tables to link us in first */
void
afs_xlatorinit_v2(struct rfs_disp_tbl *_rfs_tbl,
		  struct rfs_disp_tbl *_acl_tbl)
{
    int i;

    if (xlatorinit_v2_done++)
	return;

    for (i = 0; i < RFS_NPROC; i++) {
	afs_rfs_disp_tbl[i].orig_proc = _rfs_tbl[i].dis_proc;
	_rfs_tbl[i].dis_proc = afs_rfs_disp_tbl[i].afs_proc;
    }

    for (i = 0; i < 5; i++) {
	afs_acl_disp_tbl[i].orig_proc = _acl_tbl[i].dis_proc;
	_acl_tbl[i].dis_proc = afs_acl_disp_tbl[i].afs_proc;
    }
}

#ifndef RFS3_NPROC
#define RFS3_NPROC      22
#endif

#ifndef ACL3_NPROC
#if defined(AFS_SUN510_ENV)
#define ACL3_NPROC      4
#else
#define ACL3_NPROC      3
#endif
#endif

struct afs_nfs_disp_tbl afs_rfs3_disp_tbl[RFS3_NPROC];
struct afs_nfs_disp_tbl afs_acl3_disp_tbl[ACL3_NPROC];

struct afs_nfs3_resp {
    nfsstat3 status;
    bool_t flags;
};
typedef struct afs_nfs3_resp afs_nfs3_resp;

static int
is_afs_fh3(nfs_fh3 * fhp)
{
    if ((fhp->fh3_fsid.val[0] == AFS_VFSMAGIC)
	&& (fhp->fh3_fsid.val[1] == AFS_VFSFSID))
	return 1;
    return 0;
}

void
afs_nfs3_noaccess(struct afs_nfs3_resp *resp)
{
    resp->status = NFS3ERR_ACCES;
    resp->flags = FALSE;
}

void
afs_nfs3_notsupp(struct afs_nfs3_resp *resp)
{
    resp->status = NFS3ERR_NOTSUPP;
    resp->flags = FALSE;
}

afs_int32
nfs3_to_afs_call(int which, void *args, nfs_fh3 ** fhpp, nfs_fh3 ** fh2pp)
{
    struct vnode *vp;
    nfs_fh3 *fhp1 = 0;
    nfs_fh3 *fhp2 = 0;
    int errorcode;

    afs_Trace1(afs_iclSetp, CM_TRACE_NFS3IN, ICL_TYPE_INT32, which);
    *fh2pp = NULL;
    switch (which) {
    case NFSPROC3_GETATTR:
	{
	    GETATTR3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->object;
	    break;
	}
    case NFSPROC3_SETATTR:
	{
	    SETATTR3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->object;
	    break;
	}
    case NFSPROC3_LOOKUP:
	{
	    LOOKUP3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->what.dirp;
	    break;
	}
    case NFSPROC3_ACCESS:
	{
	    ACCESS3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->object;
	    break;
	}
    case NFSPROC3_READLINK:
	{
	    READLINK3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->symlink;
	    break;
	}
    case NFSPROC3_READ:
	{
	    READ3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->file;
	    break;
	}
    case NFSPROC3_WRITE:
	{
	    WRITE3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->file;
	    break;
	}
    case NFSPROC3_CREATE:
	{
	    CREATE3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->where.dirp;
	    break;
	}
    case NFSPROC3_MKDIR:
	{
	    MKDIR3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->where.dirp;
	    break;
	}
    case NFSPROC3_SYMLINK:
	{
	    SYMLINK3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->where.dirp;
	    break;
	}
    case NFSPROC3_MKNOD:
	{
	    MKNOD3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->where.dirp;
	    break;
	}
    case NFSPROC3_REMOVE:
	{
	    REMOVE3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->object.dirp;
	    break;
	}
    case NFSPROC3_RMDIR:
	{
	    RMDIR3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->object.dirp;
	    break;
	}
    case NFSPROC3_RENAME:
	{
	    RENAME3args *arg = args;
	    fhp1 = (nfs_fh3 *) arg->from.dirp;
	    fhp2 = (nfs_fh3 *) arg->to.dirp;
	    break;
	}
    case NFSPROC3_LINK:
	{
	    LINK3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->file;
	    fhp2 = (nfs_fh3 *) arg->link.dirp;
	    break;
	}
    case NFSPROC3_READDIR:
	{
	    READDIR3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->dir;
	    break;
	}
    case NFSPROC3_READDIRPLUS:
	{
	    READDIRPLUS3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->dir;
	    break;
	}
    case NFSPROC3_FSSTAT:
	{
	    FSSTAT3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->fsroot;
	    break;
	}
    case NFSPROC3_FSINFO:
	{
	    FSINFO3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->fsroot;
	    break;
	}
    case NFSPROC3_PATHCONF:
	{
	    PATHCONF3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->object;
	    break;
	}
    case NFSPROC3_COMMIT:
	{
	    COMMIT3args *arg = args;
	    fhp1 = (nfs_fh3 *) & arg->file;
	    break;
	}
    default:
	return NULL;
    }

    if (fhp1 && is_afs_fh3(fhp1)) {
	*fhpp = fhp1;
	if (fhp2)
	    *fh2pp = fhp2;
	return 1;
    }
    if (fhp2 && is_afs_fh3(fhp2)) {
	*fhpp = fhp1;
	*fh2pp = fhp2;
	return 1;
    }
    return NULL;
}

afs_int32
acl3_to_afs_call(int which, void *args, nfs_fh3 ** fhpp)
{
    nfs_fh3 *fhp;

    switch (which) {
    case ACLPROC3_GETACL:
	{
	    struct GETACL3args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
    case ACLPROC3_SETACL:
	{
	    struct SETACL3args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
#if defined(AFS_SUN510_ENV)
    case ACLPROC3_GETXATTRDIR:
	{
	    struct GETXATTRDIR3args *sargs = args;
	    fhp = &sargs->fh;
	    break;
	}
#endif
    default:
	return NULL;
    }

    if (fhp && is_afs_fh3(fhp)) {
	*fhpp = fhp;
	return 1;
    }

    return NULL;
}

int
afs_nfs3_dispatcher(int type, afs_int32 which, void *argp,
		    struct exportinfo *exp, struct svc_req *rp,
		    afs_ucred_t *crp)
{
    afs_int32 call = 0;
    afs_int32 code = 0;
    afs_int32 client = 0;
    struct sockaddr *sa;
    nfs_fh3 *fh = (nfs_fh3 *) argp;
    nfs_fh3 *fh2 = (nfs_fh3 *) 0;

    if (!xlatorinit_v3_done)
	return 2;

    sa = (struct sockaddr *)svc_getrpccaller(rp->rq_xprt)->buf;
    if (sa == NULL)
	return 3;

    if (sa->sa_family == AF_INET)
	client = ((struct sockaddr_in *)sa)->sin_addr.s_addr;

    AFS_GLOCK();
    code = 0;
    switch (type) {
    case 0:
	code = (client && nfs3_to_afs_call(which, argp, &fh, &fh2));
	break;
    case 1:
	code = (client && acl3_to_afs_call(which, argp, &fh));
	break;
    default:
	break;
    }

    if (code) {
	struct afs_exporter *out = 0;
	afs_int32 dummy;
	static int once = 0;
	struct SmallFid Sfid;

	memcpy(&Sfid, fh->fh3_data, SIZEOF_SMALLFID);

	afs_Trace2(afs_iclSetp, CM_TRACE_NFS3IN1, ICL_TYPE_INT32, client,
		   ICL_TYPE_FID, &Sfid);

	call = 1;
	if (!once && exp) {
	    afs_nobody = exp->exi_export.ex_anon;
	    once = 1;
	}
	code =
	    afs_nfsclient_reqhandler((struct afs_exporter *)0, &crp, client,
				     &dummy, &out);

	if (!code && out)
	    EXP_RELE(out);


	if (code == EINVAL)
	    call = 2;
    }

    AFS_GUNLOCK();
    return call;
}

void
afs_nfs3_smallfidder(struct nfs_fh3 *fhp, int status)
{
    afs_int32 addr[2];
    struct vcache *vcp;

#if defined(AFS_SUN5_64BIT_ENV)
    /* See also afs_fid() */
    memcpy(addr, fhp->fh3_data, 10);
    addr[1] = (addr[1] >> 48) & 0xffff;
#else
    memcpy(addr, fhp->fh3_data, 2 * sizeof(long));
#endif

    AFS_GLOCK();
    vcp = VTOAFS((struct vnode *)addr[0]);

    /* See also afs_osi_vget */
    if (addr[1] == AFS_XLATOR_MAGIC) {
	if (status == NFS_OK) {
	    struct SmallFid Sfid;
	    struct cell *tcell;

	    /* Make up and copy out a SmallFid */
	    tcell = afs_GetCell(vcp->f.fid.Cell, READ_LOCK);
	    Sfid.Volume = vcp->f.fid.Fid.Volume;
	    Sfid.CellAndUnique =
		((tcell->cellIndex << 24) | (vcp->f.fid.Fid.Unique & 0xffffff));
	    afs_PutCell(tcell, READ_LOCK);
	    Sfid.Vnode = (u_short) (vcp->f.fid.Fid.Vnode & 0xffff);
	    fhp->fh3_len = SIZEOF_SMALLFID;
	    memcpy(fhp->fh3_data, &Sfid, fhp->fh3_len);

	    afs_Trace3(afs_iclSetp, CM_TRACE_NFS3OUT, ICL_TYPE_INT32, status,
		       ICL_TYPE_POINTER, vcp, ICL_TYPE_FID, &Sfid);
	}

	/* If we have a ref, release it */
	if (vcp->vrefCount >= 1)
	    AFS_RELE(AFSTOV(vcp));
    }
    AFS_GUNLOCK();
}

void
afs_nfs3_getattr(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_GETATTR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_GETATTR].orig_proc) (args, xp, exp, rp,
							  crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_setattr(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_SETATTR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_SETATTR].orig_proc) (args, xp, exp, rp,
							  crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_lookup(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_LOOKUP, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else {
	(*afs_rfs3_disp_tbl[NFSPROC3_LOOKUP].orig_proc) (args, xp, exp, rp,
							 crp);
	if (afs_NFSRootOnly && call) {
	    LOOKUP3res *resp = xp;
	    afs_nfs3_smallfidder(&resp->resok.object, resp->status);
	}
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_access(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_ACCESS, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_ACCESS].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_readlink(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_READLINK, args, exp, rp,
			    crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_READLINK].orig_proc) (args, xp, exp, rp,
							   crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_read(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs3_dispatcher(0, NFSPROC3_READ, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_READ].orig_proc) (args, xp, exp, rp,
						       crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_write(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_WRITE, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_WRITE].orig_proc) (args, xp, exp, rp,
							crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_create(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_CREATE, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else {
	(*afs_rfs3_disp_tbl[NFSPROC3_CREATE].orig_proc) (args, xp, exp, rp,
							 crp);
	if (afs_NFSRootOnly && call) {
	    CREATE3res *resp = (CREATE3res *) xp;
	    afs_nfs3_smallfidder(&resp->resok.obj.handle, resp->status);
	}
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_mkdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_MKDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else {
	(*afs_rfs3_disp_tbl[NFSPROC3_MKDIR].orig_proc) (args, xp, exp, rp,
							crp);
	if (afs_NFSRootOnly && call) {
	    MKDIR3res *resp = (MKDIR3res *) xp;
	    afs_nfs3_smallfidder(&resp->resok.obj.handle, resp->status);
	}
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_symlink(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_SYMLINK, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else {
	(*afs_rfs3_disp_tbl[NFSPROC3_SYMLINK].orig_proc) (args, xp, exp, rp,
							  crp);
	if (afs_NFSRootOnly && call) {
	    SYMLINK3res *resp = (SYMLINK3res *) xp;
	    afs_nfs3_smallfidder(&resp->resok.obj.handle, resp->status);
	}
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_mknod(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_MKNOD, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else {
	(*afs_rfs3_disp_tbl[NFSPROC3_MKNOD].orig_proc) (args, xp, exp, rp,
							crp);
	if (afs_NFSRootOnly && call) {
	    MKNOD3res *resp = xp;
	    afs_nfs3_smallfidder(&resp->resok.obj.handle, resp->status);
	}
    }
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_remove(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_REMOVE, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_REMOVE].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_rmdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_RMDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_RMDIR].orig_proc) (args, xp, exp, rp,
							crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_rename(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_RENAME, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_RENAME].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_link(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call = afs_nfs3_dispatcher(0, NFSPROC3_LINK, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_LINK].orig_proc) (args, xp, exp, rp,
						       crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_readdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_READDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_READDIR].orig_proc) (args, xp, exp, rp,
							  crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_readdirplus(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_READDIRPLUS, args, exp, rp,
			    crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else if (call == 1)
	afs_nfs3_notsupp(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_READDIRPLUS].orig_proc) (args, xp, exp,
							      rp, crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_fsstat(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_FSSTAT, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_FSSTAT].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_fsinfo(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_FSINFO, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_FSINFO].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_pathconf(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_PATHCONF, args, exp, rp,
			    crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_PATHCONF].orig_proc) (args, xp, exp, rp,
							   crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_nfs3_commit(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_nfs3_resp dummy;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(0, NFSPROC3_COMMIT, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_rfs3_disp_tbl[NFSPROC3_COMMIT].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

struct afs_nfs_disp_tbl afs_rfs3_disp_tbl[22] = {
    {afs_nfs2_null},
    {afs_nfs3_getattr},
    {afs_nfs3_setattr},
    {afs_nfs3_lookup},
    {afs_nfs3_access},
    {afs_nfs3_readlink},
    {afs_nfs3_read},
    {afs_nfs3_write},
    {afs_nfs3_create},
    {afs_nfs3_mkdir},
    {afs_nfs3_symlink},
    {afs_nfs3_mknod},
    {afs_nfs3_remove},
    {afs_nfs3_rmdir},
    {afs_nfs3_rename},
    {afs_nfs3_link},
    {afs_nfs3_readdir},
    {afs_nfs3_readdirplus},
    {afs_nfs3_fsstat},
    {afs_nfs3_fsinfo},
    {afs_nfs3_pathconf},
    {afs_nfs3_commit}
};

void
afs_acl3_getacl(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(1, ACLPROC3_GETACL, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_acl3_disp_tbl[ACLPROC3_GETACL].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

void
afs_acl3_setacl(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(1, ACLPROC3_SETACL, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_acl3_disp_tbl[ACLPROC3_SETACL].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}

#if defined(AFS_SUN510_ENV)
void
afs_acl3_getxattrdir(void *args, void *xp, void *exp, void *rp, void *crp)
{
    u_int call;
    afs_ucred_t *svcred = curthread->t_cred;
    curthread->t_cred = crp;
    call =
	afs_nfs3_dispatcher(1, ACLPROC3_GETXATTRDIR, args, exp, rp, crp);
    if (call > 1)
	afs_nfs3_noaccess(xp);
    else
	(*afs_acl3_disp_tbl[ACLPROC3_GETXATTRDIR].orig_proc) (args, xp, exp, rp,
							 crp);
    curthread->t_cred = svcred;
    return;
}
#endif

struct afs_nfs_disp_tbl afs_acl3_disp_tbl[ACL3_NPROC] = {
    {afs_nfs2_null},
    {afs_acl3_getacl},
    {afs_acl3_setacl},
#if defined(AFS_SUN510_ENV)
    {afs_acl3_getxattrdir},
#endif
};

/* Munge the dispatch tables to link us in first */
void
afs_xlatorinit_v3(struct rfs_disp_tbl *_rfs_tbl,
		  struct rfs_disp_tbl *_acl_tbl)
{
    int i;

    if (xlatorinit_v3_done++)
	return;

    for (i = 0; i < 22; i++) {
	afs_rfs3_disp_tbl[i].orig_proc = _rfs_tbl[i].dis_proc;
	_rfs_tbl[i].dis_proc = afs_rfs3_disp_tbl[i].afs_proc;
    }

    for (i = 0; i < 3; i++) {
	afs_acl3_disp_tbl[i].orig_proc = _acl_tbl[i].dis_proc;
	_acl_tbl[i].dis_proc = afs_acl3_disp_tbl[i].afs_proc;
    }
}
#endif /* !defined(AFS_NONFSTRANS) */
