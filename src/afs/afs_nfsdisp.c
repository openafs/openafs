/*
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 */
#include <afsconfig.h>
#include "../afs/param.h"

RCSID("$Header: /tmp/cvstemp/openafs/src/afs/afs_nfsdisp.c,v 1.1.1.3 2002/05/10 23:43:16 hartmans Exp $");

#include "../afs/stds.h"
#include "../afs/sysincludes.h" /* Standard vendor system headers */
#if defined(AFS_SUN55_ENV) && !defined(AFS_NONFSTRANS) 
#include "../rpc/types.h"
#include "../rpc/auth.h"
#include "../rpc/auth_unix.h"
#include "../rpc/auth_des.h"
#if !defined(AFS_SUN58_ENV)
#include "../rpc/auth_kerb.h"
#endif
#include "../sys/tiuser.h"
#include "../rpc/xdr.h"
#include "../rpc/svc.h"
#include "../nfs/nfs.h"
#include "../nfs/export.h"
#include "../nfs/nfs_clnt.h"
#include "../nfs/nfs_acl.h"
#include "../afs/afsincludes.h"	
#include "../afs/afs_stats.h"   
#include "../afs/exporter.h"

static int xlatorinit_v2_done=0;
static int xlatorinit_v3_done=0;

extern int afs_nobody;
extern int afs_NFSRootOnly;

/* It's bigger than this, but we don't care about anything else */
struct rfs_disp_tbl {
	void  (*dis_proc)();	
};

struct afs_nfs_disp_tbl {
    void (*afs_proc)();
    void (*orig_proc)();
};

struct afs_nfs2_resp {
    enum nfsstat status;
};

#ifndef ACL2_NPROC
#define ACL2_NPROC	5
#endif

struct afs_nfs_disp_tbl afs_rfs_disp_tbl[RFS_NPROC];
struct afs_nfs_disp_tbl afs_acl_disp_tbl[ACL2_NPROC];

static int 
is_afs_fh(fhandle_t *fhp) {
    if ((fhp->fh_fsid.val[0] == AFS_VFSMAGIC) &&
	(fhp->fh_fsid.val[1] == AFS_VFSFSID)) 
	return 1;
    return 0;
}

afs_int32 
nfs2_to_afs_call(int which, caddr_t *args, fhandle_t **fhpp, fhandle_t **fh2pp)
{
    struct vnode *vp;
    fhandle_t *fhp1=0;
    fhandle_t *fhp2=0;
    int errorcode;

    *fh2pp = (fhandle_t *)0;
    switch (which) {
	case RFS_GETATTR:
        case RFS_READLINK:
        case RFS_STATFS:
	    fhp1 = (fhandle_t *)args;
	    break;
	case RFS_SETATTR: 
	{
	    struct nfssaargs *sargs = (struct nfssaargs *)args; 
	    fhp1 = (fhandle_t *)&sargs->saa_fh;
	    break;
	} 
        case RFS_LOOKUP: 
	{
	    struct nfsdiropargs *sargs = (struct nfsdiropargs *)args;
	    fhp1 = (fhandle_t *)&sargs->da_fhandle;
	    break;
	} 
	case RFS_READ:
	{
	    struct nfsreadargs *sargs = (struct nfsreadargs *)args;
	    fhp1 = (fhandle_t *)&sargs->ra_fhandle;
	    break;
	} 
        case RFS_WRITE: 
	{
	    struct nfswriteargs *sargs = (struct nfswriteargs *)args;
	    fhp1 = (fhandle_t *)&sargs->wa_fhandle;
	    break;
	} 
        case RFS_CREATE: 
	{
	    struct nfscreatargs *sargs = (struct nfscreatargs *)args;
	    fhp1 = (fhandle_t *)&sargs->ca_da.da_fhandle;
	    break;
	} 
        case RFS_REMOVE: 
	{
	    struct nfsdiropargs *sargs = (struct nfsdiropargs *)args;
	    fhp1 = (fhandle_t *)&sargs->da_fhandle;
	    break;
	}
        case RFS_RENAME: 
	{
	    struct nfsrnmargs *sargs = (struct nfsrnmargs *)args;
	    fhp1 = (fhandle_t *)&sargs->rna_from.da_fhandle;
	    fhp2 = (fhandle_t *)&sargs->rna_to.da_fhandle;
	    break;
	} 
        case RFS_LINK: 
	{
	    struct nfslinkargs *sargs = (struct nfslinkargs *)args;
	    fhp1 = (fhandle_t *)&sargs->la_from;
	    fhp2 = (fhandle_t *)&sargs->la_to.da_fhandle;
	    break;
	} 
        case RFS_SYMLINK: 
	{
	    struct nfsslargs *sargs = (struct nfsslargs *)args;
	    fhp1 = (fhandle_t *)&sargs->sla_from.da_fhandle;
	    break;
	} 
        case RFS_MKDIR: 
	{
	    struct nfscreatargs *sargs = (struct nfscreatargs *)args;
	    fhp1 = (fhandle_t *)&sargs->ca_da.da_fhandle;
	    break;
	} 
        case RFS_RMDIR: 
	{
	    struct nfsdiropargs *sargs = (struct nfsdiropargs *)args;
	    fhp1 = (fhandle_t *)&sargs->da_fhandle;
	    break;
	} 
        case RFS_READDIR: 
	{
	    struct nfsrddirargs *sargs = (struct nfsrddirargs *)args;
	    fhp1 = (fhandle_t *)&sargs->rda_fh;
	    break;
	} 
	default:
	    return NULL;
    }

    /* Ok if arg 1 is in AFS or if 2 args and arg 2 is in AFS */
    if (is_afs_fh(fhp1)) {
	*fhpp = fhp1;
	if (fhp2) 
	    *fh2pp = fhp2;
	return 1;	    
    }
    if (is_afs_fh(fhp2)) {
	*fhpp = fhp1;
	*fh2pp = fhp2;
	return 1;	    
    }
    return NULL;
}

afs_int32 
acl2_to_afs_call(int which, caddr_t *args, fhandle_t **fhpp)
{
    fhandle_t *fhp;

    switch(which) {
    case ACLPROC2_NULL:
    {
	return NULL;
    }
    case ACLPROC2_GETACL: 
    {
	struct GETACL2args *sargs = (struct GETACL2args *) args;
	fhp = &sargs->fh;
	break;
    }
    case ACLPROC2_SETACL: 
    {
	struct SETACL2args *sargs = (struct SETACL2args *) args;
	fhp = &sargs->fh;
	break;
    }
    case ACLPROC2_GETATTR: 
    {
	struct GETATTR2args *sargs = (struct GETATTR2args *) args;
	fhp = &sargs->fh;
	break;
    }
    case ACLPROC2_ACCESS: 
    {
	struct ACCESS2args *sargs = (struct ACCESS2args *) args;
	fhp = &sargs->fh;
	break;
    }
    default:
	return NULL;
    }

    if (is_afs_fh(fhp)) {
	*fhpp = fhp;
	return 1;
    }

    return NULL;
}

int
afs_nfs2_dispatcher(int type, afs_int32 which, char *argp, 
		    struct exportinfo **expp, 
		    struct svc_req *rp, struct AFS_UCRED *crp)
{
    afs_int32 call = 0;
    afs_int32 code = 0;
    afs_int32 client = 0;
    struct sockaddr *sa;
    fhandle_t *fh = (fhandle_t *)argp;
    fhandle_t *fh2 = (fhandle_t *)0;

    if (!xlatorinit_v2_done)
	return 2;

    sa = (struct sockaddr *)svc_getrpccaller(rp->rq_xprt)->buf;
    client = ((struct sockaddr_in *)sa)->sin_addr.s_addr;

    AFS_GLOCK();
    code = 0;
    switch (type) {
    case 0:
	code = nfs2_to_afs_call(which, argp, &fh, &fh2);
	break;
    case 1:
	code = acl2_to_afs_call(which, argp, &fh);
	break;
    default:
	break;
    }

    if (code) {
	struct afs_exporter *out = 0;
	afs_int32 dummy;
	static int once = 0;
	struct SmallFid Sfid;

	memcpy((char *)&Sfid, fh->fh_data, SIZEOF_SMALLFID);

	/* We ran */
	call = 1;
	if (!once && *expp) {
	    afs_nobody = (*expp)->exi_export.ex_anon;
	    once = 1;
	}
	code = afs_nfsclient_reqhandler((struct afs_exporter *)0, &crp,
					client, &dummy, &out);
	/* Be careful to release this */
	if (!code && out)
	    EXP_RELE(out);

	/* We ran and failed */
	if (code == EINVAL) 
	    call = 2;
    }

    AFS_GUNLOCK();
    return call;
}

void 
afs_nfs2_smallfidder(struct nfsdiropres *dr)
{
    register fhandle_t *fhp = (fhandle_t *)&dr->dr_fhandle;
    afs_int32 addr[2];
    struct vcache *vcp;

#if defined(AFS_SUN57_64BIT_ENV)
    /* See also afs_fid() */    
    memcpy((char *)addr, fhp->fh_data, 10);
    addr[1] = (addr[1] >> 48) & 0xffff;
#else 
    memcpy((char *)addr, fhp->fh_data, 2 * sizeof(long));
#endif

    AFS_GLOCK();
    vcp = VTOAFS((struct vnode*)addr[0]);

    /* See also afs_osi_vget */
    if (addr[1] == AFS_XLATOR_MAGIC)
    {
	if (dr->dr_status == NFS_OK) {
	    struct SmallFid Sfid;
	    struct cell *tcell;

	    /* Make up and copy out a SmallFid */
	    tcell = afs_GetCell(vcp->fid.Cell, READ_LOCK);
	    Sfid.Volume = vcp->fid.Fid.Volume;
	    Sfid.CellAndUnique = ((tcell->cellIndex << 24) |
				  (vcp->fid.Fid.Unique & 0xffffff));
	    afs_PutCell(tcell, READ_LOCK);
	    Sfid.Vnode = (u_short)(vcp->fid.Fid.Vnode & 0xffff);
	    fhp->fh_len = SIZEOF_SMALLFID;
	    memcpy(dr->dr_fhandle.fh_data, (char*)&Sfid, fhp->fh_len);
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

void afs_nfs2_null(char *args, char *xp, char *exp, char *rp, char *crp)
{
}

void 
afs_nfs2_root(char *args, char *xp, char *exp, char *rp, char *crp)
{
}

void 
afs_nfs2_writecache(char *args, char *xp, char *exp, char *rp, char *crp)
{
}

#define NFS_V2_REQ(FUNCNAME, RFSOP, POST) \
void FUNCNAME(char *args, char *xp, char *exp, char *rp, char *crp) { \
    u_int call; \
    struct cred *svcred = curthread->t_cred; \
    curthread->t_cred = (struct cred*)crp; \
    call=afs_nfs2_dispatcher(0, RFSOP, (char *)args, &exp, rp, crp); \
    if (call>1) afs_nfs2_noaccess((struct afs_nfs2_resp *)xp); \
    else { (*afs_rfs_disp_tbl[RFSOP].orig_proc)(args, xp, exp, rp, crp); \
    if (POST && afs_NFSRootOnly && call) afs_nfs2_smallfidder(xp); } \
    curthread->t_cred = svcred; \
    return; \
}

NFS_V2_REQ(afs_nfs2_getattr, RFS_GETATTR, 0) 
NFS_V2_REQ(afs_nfs2_setattr, RFS_SETATTR, 0)
NFS_V2_REQ(afs_nfs2_lookup, RFS_LOOKUP, 1)
NFS_V2_REQ(afs_nfs2_readlink, RFS_READLINK, 0)
NFS_V2_REQ(afs_nfs2_read, RFS_READ, 0)
NFS_V2_REQ(afs_nfs2_write, RFS_WRITE, 0)
NFS_V2_REQ(afs_nfs2_create, RFS_CREATE, 1)
NFS_V2_REQ(afs_nfs2_remove, RFS_REMOVE, 0)
NFS_V2_REQ(afs_nfs2_rename, RFS_RENAME, 0)
NFS_V2_REQ(afs_nfs2_link, RFS_LINK, 0)
NFS_V2_REQ(afs_nfs2_symlink, RFS_SYMLINK, 0)
NFS_V2_REQ(afs_nfs2_mkdir, RFS_MKDIR, 1)
NFS_V2_REQ(afs_nfs2_rmdir, RFS_RMDIR, 0)
NFS_V2_REQ(afs_nfs2_readdir, RFS_READDIR, 0)
NFS_V2_REQ(afs_nfs2_statfs, RFS_STATFS, 0)

struct afs_nfs_disp_tbl afs_rfs_disp_tbl[RFS_NPROC] = {
    { afs_nfs2_null },
    { afs_nfs2_getattr },
    { afs_nfs2_setattr },
    { afs_nfs2_root },
    { afs_nfs2_lookup },
    { afs_nfs2_readlink },
    { afs_nfs2_read },
    { afs_nfs2_writecache },	
    { afs_nfs2_write },
    { afs_nfs2_create },
    { afs_nfs2_remove },	
    { afs_nfs2_rename },
    { afs_nfs2_link },	
    { afs_nfs2_symlink },
    { afs_nfs2_mkdir },
    { afs_nfs2_rmdir },
    { afs_nfs2_readdir },
    { afs_nfs2_statfs }
};

#define ACL_V2_REQ(FUNCNAME, ACLOP) \
void FUNCNAME(char *args, char *xp, char *exp, char *rp, char *crp) { \
    u_int call; \
    struct cred *svcred = curthread->t_cred; \
    curthread->t_cred = (struct cred*)crp; \
    call=afs_nfs2_dispatcher(1, ACLOP, (char *)args, &exp, rp, crp); \
    if (call>1) afs_nfs2_noaccess((struct afs_nfs2_resp *)xp); \
    else (*afs_rfs_disp_tbl[ACLOP].orig_proc)(args, xp, exp, rp, crp); \
    curthread->t_cred = svcred; \
    return; \
}

ACL_V2_REQ(afs_acl2_getacl, ACLPROC2_GETACL)
ACL_V2_REQ(afs_acl2_setacl, ACLPROC2_SETACL)
ACL_V2_REQ(afs_acl2_getattr, ACLPROC2_GETATTR)
ACL_V2_REQ(afs_acl2_access, ACLPROC2_ACCESS)

struct afs_nfs_disp_tbl afs_acl_disp_tbl[ACL2_NPROC] = {
    { afs_nfs2_null }, 
    { afs_acl2_getacl }, 
    { afs_acl2_setacl },
    { afs_acl2_getattr }, 
    { afs_acl2_access } 
};

/* Munge the dispatch tables to link us in first */
void 
afs_xlatorinit_v2(struct rfs_disp_tbl *_rfs_tbl, 
		  struct rfs_disp_tbl *_acl_tbl)
{
    int i;

    if (xlatorinit_v2_done++) return;

    for (i=0; i < RFS_NPROC; i++) {
	afs_rfs_disp_tbl[i].orig_proc = _rfs_tbl[i].dis_proc;
	_rfs_tbl[i].dis_proc = afs_rfs_disp_tbl[i].afs_proc;
    }

    for (i=0; i < ACL2_NPROC; i++) {
	afs_acl_disp_tbl[i].orig_proc = _acl_tbl[i].dis_proc;
	_acl_tbl[i].dis_proc = afs_acl_disp_tbl[i].afs_proc;
    }
}

#ifndef RFS3_NPROC
#define RFS3_NPROC	22
#endif

#ifndef ACL3_NPROC
#define ACL3_NPROC	3
#endif

struct afs_nfs_disp_tbl afs_rfs3_disp_tbl[RFS3_NPROC];
struct afs_nfs_disp_tbl afs_acl3_disp_tbl[ACL3_NPROC];

struct afs_nfs3_resp {
    nfsstat3 status;
    bool_t flags; 
};
typedef struct afs_nfs3_resp afs_nfs3_resp;

static int 
is_afs_fh3(nfs_fh3 *fhp) {
    if ((fhp->fh3_fsid.val[0] == AFS_VFSMAGIC) &&
	(fhp->fh3_fsid.val[1] == AFS_VFSFSID)) 
	return 1;
    return 0;
}

void
afs_nfs3_noaccess(struct afs_nfs3_resp *resp)
{
    resp->status = NFS3ERR_ACCES;
    resp->flags = FALSE;
}

afs_int32 
nfs3_to_afs_call(int which, caddr_t *args, nfs_fh3 **fhpp, nfs_fh3 **fh2pp)
{
    struct vnode *vp;
    nfs_fh3 *fhp1=0;
    nfs_fh3 *fhp2=0;
    int errorcode;

    *fh2pp = (nfs_fh3 *)0;
    switch (which) {
        case NFSPROC3_GETATTR: 
	{
            GETATTR3args *arg = (GETATTR3args *)args;
            fhp1 = (nfs_fh3 *) &arg->object;
            break;
        } 
        case NFSPROC3_SETATTR: 
	{
            SETATTR3args *arg = (SETATTR3args *)args;
            fhp1 = (nfs_fh3 *) &arg->object;
            break;
        } 
        case NFSPROC3_LOOKUP: 
	{
            LOOKUP3args *arg = (LOOKUP3args *)args;
            fhp1 = (nfs_fh3 *) &arg->what.dirp;
            break;
        } 
        case NFSPROC3_ACCESS: 
	{
            ACCESS3args *arg = (ACCESS3args *)args;
            fhp1 = (nfs_fh3 *) &arg->object;
            break;
        } 
        case NFSPROC3_READLINK: 
	{
            READLINK3args *arg = (READLINK3args *)args;
            fhp1 = (nfs_fh3 *) &arg->symlink;       
            break;
        } 
        case NFSPROC3_READ: 
	{
            READ3args *arg = (READ3args *)args;
            fhp1 = (nfs_fh3 *) &arg->file;
            break;
        } 
        case NFSPROC3_WRITE: 
	{
            WRITE3args *arg = (WRITE3args *)args;
            fhp1 = (nfs_fh3 *) &arg->file;
            break;
        } 
        case NFSPROC3_CREATE: 
	{
            CREATE3args *arg = (CREATE3args *)args;
            fhp1 = (nfs_fh3 *) &arg->where.dir;
            break;
        } 
        case NFSPROC3_MKDIR: 
	{
            MKDIR3args *arg = (MKDIR3args *)args;
            fhp1 = (nfs_fh3 *) &arg->where.dir;
            break;
        } 
        case NFSPROC3_SYMLINK: 
	{
            SYMLINK3args *arg = (SYMLINK3args *)args;
            fhp1 = (nfs_fh3 *) &arg->where.dir;
            break;
        } 
        case NFSPROC3_MKNOD:
	{
            MKNOD3args *arg = (MKNOD3args *)args;
            fhp1 = (nfs_fh3 *) &arg->where.dir;
            break;
        } 
        case NFSPROC3_REMOVE: 
	{
            REMOVE3args *arg = (REMOVE3args *)args;
            fhp1 = (nfs_fh3 *) &arg->object.dir;
            break;
        } 
        case NFSPROC3_RMDIR: 
	{
            RMDIR3args *arg = (RMDIR3args *)args;
            fhp1 = (nfs_fh3 *) &arg->object.dir;
            break;
        } 
        case NFSPROC3_RENAME: 
	{
            RENAME3args *arg = (RENAME3args *)args;
            fhp1 = (nfs_fh3 *) &arg->from.dir;
            fhp2 = (nfs_fh3 *) &arg->to.dir;
            break;
        } 
        case NFSPROC3_LINK: 
	{
            LINK3args *arg = (LINK3args *)args;
            fhp1 = (nfs_fh3 *) &arg->file;
            fhp2 = (nfs_fh3 *) &arg->link.dir;
            break;
        } 
        case NFSPROC3_READDIR: 
	{
            READDIR3args *arg = (READDIR3args *)args;
            fhp1 = (nfs_fh3 *) &arg->dir;
            break;
        } 
        case NFSPROC3_READDIRPLUS: 
	{
            READDIRPLUS3args *arg = (READDIRPLUS3args *)args;
            fhp1 = (nfs_fh3 *) &arg->dir;
            break;
        } 
        case NFSPROC3_FSSTAT: 
	{
            FSSTAT3args *arg = (FSSTAT3args *)args;
            fhp1 = (nfs_fh3 *) &arg->fsroot;
            break;
        } 
        case NFSPROC3_FSINFO: 
	{
            FSINFO3args *arg = (FSINFO3args *)args;
            fhp1 = (nfs_fh3 *) &arg->fsroot;
            break;
        } 
        case NFSPROC3_PATHCONF: 
	{
            PATHCONF3args *arg = (PATHCONF3args *)args;
            fhp1 = (nfs_fh3 *) &arg->object;
            break;
        } 
        case NFSPROC3_COMMIT: 
	{
            COMMIT3args *arg = (COMMIT3args *)args;
            fhp1 = (nfs_fh3 *) &arg->file;
            break;
        }
	default:
	    return NULL;
    }

    /* Ok if arg 1 is in AFS or if 2 args and arg 2 is in AFS */
    if (is_afs_fh3(fhp1)) {
	*fhpp = fhp1;
	if (fhp2) 
	    *fh2pp = fhp2;
	return 1;	    
    }
    if (is_afs_fh3(fhp2)) {
	*fhpp = fhp1;
	*fh2pp = fhp2;
	return 1;	    
    }
    return NULL;
}

afs_int32 
acl3_to_afs_call(int which, caddr_t *args, nfs_fh3 **fhpp)
{
    nfs_fh3 *fhp;

    switch(which) {
    case ACLPROC3_GETACL: 
    {
        struct GETACL3args *sargs = (struct GETACL3args *) args;
        fhp = &sargs->fh;
        break;
    }
    case ACLPROC3_SETACL: 
    {
        struct SETACL3args *sargs = (struct SETACL3args *) args;
        fhp = &sargs->fh;
        break;
    }
    default:
	return NULL;
    }

    if (is_afs_fh3(fhp)) {
	*fhpp = fhp;
	return 1;
    }

    return NULL;
}

int
afs_nfs3_dispatcher(int type, afs_int32 which, char *argp, 
		    struct exportinfo **expp, 
		    struct svc_req *rp, struct AFS_UCRED *crp)
{
    afs_int32 call = 0;
    afs_int32 code = 0;
    afs_int32 client = 0;
    struct sockaddr *sa;
    nfs_fh3 *fh = (nfs_fh3 *)argp;
    nfs_fh3 *fh2 = (nfs_fh3 *)0;

    if (!xlatorinit_v3_done)
	return 2;

    sa = (struct sockaddr *)svc_getrpccaller(rp->rq_xprt)->buf;
    client = ((struct sockaddr_in *)sa)->sin_addr.s_addr;

    AFS_GLOCK();
    code = 0;
    switch (type) {
    case 0:
	code = nfs3_to_afs_call(which, argp, &fh, &fh2);
	break;
    case 1:
	code = acl3_to_afs_call(which, argp, &fh);
	break;
    default:
	break;
    }

    if (code) {
	struct afs_exporter *out = 0;
	afs_int32 dummy;
	static int once = 0;
	struct SmallFid Sfid;

	memcpy((char *)&Sfid, fh->fh3_data, SIZEOF_SMALLFID);

	/* We ran */
	call = 1;
	if (!once && *expp) {
	    afs_nobody = (*expp)->exi_export.ex_anon;
	    once = 1;
	}
	code = afs_nfsclient_reqhandler((struct afs_exporter *)0, &crp,
					client, &dummy, &out);
	/* Be careful to release this */
	if (!code && out)
	    EXP_RELE(out);

	/* We ran and failed */
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

#if defined(AFS_SUN57_64BIT_ENV)
    /* See also afs_fid() */    
    memcpy((char *)addr, fhp->fh3_data, 10);
    addr[1] = (addr[1] >> 48) & 0xffff;
#else 
    memcpy((char *)addr, fhp->fh3_data, 2 * sizeof(long));
#endif

    AFS_GLOCK();
    vcp = VTOAFS((struct vnode*)addr[0]);

    /* See also afs_osi_vget */
    if (addr[1] == AFS_XLATOR_MAGIC)
    {
	if (status == NFS_OK) {
	    struct SmallFid Sfid;
	    struct cell *tcell;

	    /* Make up and copy out a SmallFid */
	    tcell = afs_GetCell(vcp->fid.Cell, READ_LOCK);
	    Sfid.Volume = vcp->fid.Fid.Volume;
	    Sfid.CellAndUnique = ((tcell->cellIndex << 24) |
				  (vcp->fid.Fid.Unique & 0xffffff));
	    afs_PutCell(tcell, READ_LOCK);
	    Sfid.Vnode = (u_short)(vcp->fid.Fid.Vnode & 0xffff);
	    fhp->fh3_len = SIZEOF_SMALLFID;
	    memcpy(fhp->fh3_data, (char*)&Sfid, fhp->fh3_len);
	}

	/* If we have a ref, release it */
	if (vcp->vrefCount >= 1) 
	    AFS_RELE(AFSTOV(vcp));
    } 
    AFS_GUNLOCK();
}

#define NFS_V3_REQ(FUNCNAME, NFSOP, POST, RESP, RESPP) \
void FUNCNAME(char *args, char *xp, char *exp, char *rp, char *crp) { \
    u_int call; \
    afs_nfs3_resp dummy; \
    struct cred *svcred = curthread->t_cred; \
    curthread->t_cred = (struct cred*)crp; \
    call=afs_nfs3_dispatcher(0, NFSOP, (char *)args, &exp, rp, crp); \
    if (call>1) afs_nfs3_noaccess((struct afs_nfs3_resp *)xp); \
    else { (*afs_rfs3_disp_tbl[NFSOP].orig_proc)(args, xp, exp, rp, crp); \
    if (POST && afs_NFSRootOnly && call) { \
    RESP *resp = ( RESP *)xp; \
    afs_nfs3_smallfidder( RESPP , resp->status); } } \
    curthread->t_cred = svcred; \
    return; \
}

NFS_V3_REQ(afs_nfs3_getattr, NFSPROC3_GETATTR, 0, afs_nfs3_resp, &dummy) 
NFS_V3_REQ(afs_nfs3_setattr, NFSPROC3_SETATTR, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_lookup, NFSPROC3_LOOKUP, 1, LOOKUP3res, &resp->resok.object)
NFS_V3_REQ(afs_nfs3_access, NFSPROC3_ACCESS, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_readlink, NFSPROC3_READLINK, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_read, NFSPROC3_READ, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_write, NFSPROC3_WRITE, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_create, NFSPROC3_CREATE, 1, CREATE3res, &resp->resok.obj.handle)
NFS_V3_REQ(afs_nfs3_mkdir, NFSPROC3_MKDIR, 1, MKDIR3res, &resp->resok.obj.handle)
NFS_V3_REQ(afs_nfs3_symlink, NFSPROC3_SYMLINK, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_mknod, NFSPROC3_MKNOD, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_remove, NFSPROC3_REMOVE, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_rmdir, NFSPROC3_RMDIR, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_rename, NFSPROC3_RENAME, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_link, NFSPROC3_LINK, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_readdir, NFSPROC3_READDIR, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_readdirplus, NFSPROC3_READDIRPLUS, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_fsstat, NFSPROC3_FSSTAT, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_fsinfo, NFSPROC3_FSINFO, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_pathconf, NFSPROC3_PATHCONF, 0, afs_nfs3_resp, &dummy)
NFS_V3_REQ(afs_nfs3_commit, NFSPROC3_COMMIT, 0, afs_nfs3_resp, &dummy)

struct afs_nfs_disp_tbl afs_rfs3_disp_tbl[RFS3_NPROC] = {
    { afs_nfs2_null },
    { afs_nfs3_getattr },
    { afs_nfs3_setattr },
    { afs_nfs3_lookup },
    { afs_nfs3_access },
    { afs_nfs3_readlink },
    { afs_nfs3_read },
    { afs_nfs3_write },
    { afs_nfs3_create },
    { afs_nfs3_mkdir },
    { afs_nfs3_symlink },
    { afs_nfs3_mknod },
    { afs_nfs3_remove },        
    { afs_nfs3_rmdir },
    { afs_nfs3_rename },
    { afs_nfs3_link },  
    { afs_nfs3_readdir },
    { afs_nfs3_readdirplus },
    { afs_nfs3_fsstat },
    { afs_nfs3_fsinfo },
    { afs_nfs3_pathconf },
    { afs_nfs3_commit }
};

#define ACL_V3_REQ(FUNCNAME, NFSOP) \
void FUNCNAME(char *args, char *xp, char *exp, char *rp, char *crp) { \
    u_int call; \
    struct cred *svcred = curthread->t_cred; \
    curthread->t_cred = (struct cred*)crp; \
    call=afs_nfs3_dispatcher(1, NFSOP, (char *)args, &exp, rp, crp); \
    if (call>1) afs_nfs3_noaccess((struct afs_nfs3_resp *)xp); \
    else (*afs_rfs3_disp_tbl[NFSOP].orig_proc)(args, xp, exp, rp, crp); \
    curthread->t_cred = svcred; \
    return; \
}

ACL_V3_REQ(afs_acl3_getacl, ACLPROC3_GETACL)
ACL_V3_REQ(afs_acl3_setacl, ACLPROC3_SETACL)

struct afs_nfs_disp_tbl afs_acl3_disp_tbl[ACL3_NPROC] = {
    { afs_nfs2_null }, 
    { afs_acl3_getacl }, 
    { afs_acl3_setacl },
};

/* Munge the dispatch tables to link us in first */
void 
afs_xlatorinit_v3(struct rfs_disp_tbl *_rfs_tbl, 
                  struct rfs_disp_tbl *_acl_tbl)
{
    int i;

    if (xlatorinit_v3_done++) return;

    for (i=0; i < RFS3_NPROC; i++) {
        afs_rfs3_disp_tbl[i].orig_proc = _rfs_tbl[i].dis_proc;
        _rfs_tbl[i].dis_proc = afs_rfs3_disp_tbl[i].afs_proc;
    }

    for (i=0; i < ACL3_NPROC; i++) {
        afs_acl3_disp_tbl[i].orig_proc = _acl_tbl[i].dis_proc;
        _acl_tbl[i].dis_proc = afs_acl3_disp_tbl[i].afs_proc;
    }
}
#endif /* !defined(AFS_NONFSTRANS) */
