/*
 * vi:set cin noet sw=4 tw=70:
 * Copyright 2006, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Filesystem export operations for Linux
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "nfsclient.h"
#include "h/smp_lock.h"
#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/svcauth.h>

static unsigned long authtab_addr = 0;
#if defined(module_param)
module_param(authtab_addr, long, 0);
#else
MODULE_PARM(authtab_addr, "l");
#endif
MODULE_PARM_DESC(authtab_addr, "Address of the authtab array.");

extern struct auth_ops *authtab[] __attribute__((weak));
static struct auth_ops **afs_authtab;
static struct auth_ops *afs_new_authtab[RPC_AUTH_MAXFLAVOR];
static struct auth_ops *afs_orig_authtab[RPC_AUTH_MAXFLAVOR];

static int whine_memory = 0;

afs_lock_t afs_xnfssrv;

struct nfs_server_thread {
    struct nfs_server_thread *next;	/* next in chain */
    pid_t pid;				/* pid of this thread */
    int active;				/* this thread is servicing an RPC */
    struct sockaddr_in client_addr;	/* latest client of this thread */
    int client_addrlen;
    afs_int32 uid;			/* AFS UID/PAG for this thread */
    afs_int32 code;			/* What should InitReq return? */
    int flavor;				/* auth flavor */
    uid_t client_uid;			/* UID claimed by client */
    gid_t client_gid;			/* GID claimed by client */
    gid_t client_g0, client_g1;		/* groups claimed by client */
};

static struct nfs_server_thread *nfssrv_list = 0;

static struct nfs_server_thread *find_nfs_thread(int create)
{
    struct nfs_server_thread *this;

    /* Check that this is an nfsd kernel thread */
    if (current->files != init_task.files || strcmp(current->comm, "nfsd"))
	return 0;

    ObtainWriteLock(&afs_xnfssrv, 804);
    for (this = nfssrv_list; this; this = this->next)
	if (this->pid == current->pid)
	    break;
    if (!this && create) {
	this = afs_osi_Alloc(sizeof(struct nfs_server_thread));
	if (this) {
	    this->next = nfssrv_list;
	    this->pid  = current->pid;
	    this->client_addrlen = 0;
	    nfssrv_list = this;
	    printk("afs: added nfsd task %d/%d\n",
		   current->tgid, current->pid);
	} else if (!whine_memory) {
	    whine_memory = 1;
	    printk("afs: failed to allocate memory for nfsd task %d/%d\n",
		   current->tgid, current->pid);
	}
    }
    ReleaseWriteLock(&afs_xnfssrv);
    return this;
}

static int
svcauth_afs_accept(struct svc_rqst *rqstp, u32 *authp)
{
    struct nfs_server_thread *ns;
    struct afs_exporter *outexp;
    struct AFS_UCRED *credp;
    int code;

    code = afs_orig_authtab[rqstp->rq_authop->flavour]->accept(rqstp, authp);
    if (code != SVC_OK)
	return code;

    AFS_GLOCK();
    ns = find_nfs_thread(1);
    if (!ns) {
	AFS_GUNLOCK();
	/* XXX maybe we should fail this with rpc_system_err? */
	return SVC_OK;
    }

    ns->active		= 1;
    ns->flavor		= rqstp->rq_authop->flavour;
    ns->code		= EACCES;
    ns->client_addr	= rqstp->rq_addr;
    ns->client_addrlen	= rqstp->rq_addrlen;
    ns->client_uid	= rqstp->rq_cred.cr_uid;
    ns->client_gid	= rqstp->rq_cred.cr_gid;
    if (rqstp->rq_cred.cr_group_info->ngroups > 0)
	ns->client_g0	= GROUP_AT(rqstp->rq_cred.cr_group_info, 0);
    else
	ns->client_g0	= -1;
    if (rqstp->rq_cred.cr_group_info->ngroups > 1)
	ns->client_g1	= GROUP_AT(rqstp->rq_cred.cr_group_info, 1);
    else
	ns->client_g1	= -1;

    /* NB: Don't check the length; it's not always filled in! */
    if (rqstp->rq_addr.sin_family != AF_INET) {
	printk("afs: NFS request from non-IPv4 client (family %d len %d)\n",
	       rqstp->rq_addr.sin_family, rqstp->rq_addrlen);
	goto done;
    }

    credp = crget();
    credp->cr_uid = rqstp->rq_cred.cr_uid;
    credp->cr_gid = rqstp->rq_cred.cr_gid;
    get_group_info(rqstp->rq_cred.cr_group_info);
    credp->cr_group_info = rqstp->rq_cred.cr_group_info;

    /* avoid creating wildcard entries by mapping anonymous
     * clients to afs_nobody */
    if (credp->cr_uid == -1)
	credp->cr_uid = -2;
    code = afs_nfsclient_reqhandler(0, &credp, rqstp->rq_addr.sin_addr.s_addr,
				    &ns->uid, &outexp);
    if (!code && outexp) EXP_RELE(outexp);
    if (!code) ns->code = 0;
    if (code)
	printk("afs: svcauth_afs_accept: afs_nfsclient_reqhandler: %d\n", code);
    crfree(credp);

done:
    AFS_GUNLOCK();
    return SVC_OK;
}


#if 0
/* This doesn't work, because they helpfully NULL out rqstp->authop
 * before calling us, so we have no way to tell what the original
 * auth flavor was.
 */
static int
svcauth_afs_release(struct svc_rqst *rqstp)
{
    struct nfs_server_thread *ns;

    AFS_GLOCK();
    ns = find_nfs_thread(0);
    if (ns) ns->active = 0;
    AFS_GUNLOCK();

    return afs_orig_authtab[rqstp->rq_authop->flavour]->release(rqstp);
}
#endif


int osi_linux_nfs_initreq(struct vrequest *av, struct AFS_UCRED *cr, int *code)
{
    struct nfs_server_thread *ns;

    ns = find_nfs_thread(0);
    if (!ns || !ns->active)
	return 0;

    *code = ns->code;
    if (!ns->code) {
	cr->cr_ruid = NFSXLATOR_CRED;
	av->uid = ns->uid;
    }
    return 1;
}

void osi_linux_nfssrv_init(void)
{
    int i;

    nfssrv_list = 0;
    RWLOCK_INIT(&afs_xnfssrv, "afs_xnfssrv");

    if (authtab)	   afs_authtab = authtab;
    else if (authtab_addr) afs_authtab = (struct auth_ops **)authtab_addr;
    else {
	printk("Warning: Unable to find the address of authtab\n");
	printk("NFS Translator hooks will not be installed\n");
	printk("To correct, specify authtab_addr=<authtab>\n");
	afs_authtab = 0;
	return;
    }

    for (i = 0; i < RPC_AUTH_MAXFLAVOR; i++) {
	afs_orig_authtab[i] = afs_authtab[i];
	if (!afs_orig_authtab[i] || afs_orig_authtab[i]->flavour != i ||
	    !try_module_get(afs_orig_authtab[i]->owner)) {
	    afs_orig_authtab[i] = 0;
	    continue;
	}

	afs_new_authtab[i] = afs_osi_Alloc(sizeof(struct auth_ops));
	*(afs_new_authtab[i]) = *(afs_orig_authtab[i]);
	afs_new_authtab[i]->owner = THIS_MODULE;
	afs_new_authtab[i]->accept = svcauth_afs_accept;
	/* afs_new_authtab[i]->release = svcauth_afs_release; */
	svc_auth_unregister(i);
	svc_auth_register(i, afs_new_authtab[i]);
    }
}

void osi_linux_nfssrv_shutdown(void)
{
    struct nfs_server_thread *next;
    int i;

    if (afs_authtab) {
	for (i = 0; i < RPC_AUTH_MAXFLAVOR; i++) {
	    if (!afs_orig_authtab[i])
		continue;
	    svc_auth_unregister(i);
	    svc_auth_register(i, afs_orig_authtab[i]);
	    module_put(afs_orig_authtab[i]->owner);
	    afs_osi_Free(afs_new_authtab[i], sizeof(struct auth_ops));
	}
    }

    AFS_GLOCK();
    ObtainWriteLock(&afs_xnfssrv, 805);
    while (nfssrv_list) {
	next = nfssrv_list->next;
	afs_osi_Free(nfssrv_list, sizeof(struct nfs_server_thread));
	nfssrv_list = next;
    }
    ReleaseWriteLock(&afs_xnfssrv);
    AFS_GUNLOCK();
}
