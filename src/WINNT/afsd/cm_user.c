/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#endif /* !DJGPP */
#include <malloc.h>
#include <string.h>

#include "afsd.h"
#include <osi.h>
#include <rx/rx.h>

#ifdef AFS_RXK5
#if defined(AFS_NT40_ENV) && defined(USING_MIT)
#include <krb5.h>
#include <rx/rxk5_ntfixprotos.h>
#endif /* AFS_NT40_ENV && MIT */
#include <rx/rxk5.h>
#include <afs/rxk5_tkt.h>
#endif /* AFS_RXK5 */

osi_rwlock_t cm_userLock;

cm_user_t *cm_rootUserp;

void cm_InitUser(void)
{
    static osi_once_t once;
        
    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_userLock, "cm_userLock");
        osi_EndOnce(&once);
    }
        
    cm_rootUserp = cm_NewUser();
}

cm_user_t *cm_NewUser(void)
{
    cm_user_t *userp;
        
    userp = malloc(sizeof(*userp));
    memset(userp, 0, sizeof(*userp));
    userp->refCount = 1;
    lock_InitializeMutex(&userp->mx, "cm_user_t");
    return userp;
}

/* must be called with locked userp */
cm_ucell_t *cm_GetUCell(cm_user_t *userp, cm_cell_t *cellp)
{
    cm_ucell_t *ucp;
        
    lock_AssertMutex(&userp->mx);
    for (ucp = userp->cellInfop; ucp; ucp=ucp->nextp) {
        if (ucp->cellp == cellp) 
            break;
    }
        
    if (!ucp) {
        ucp = malloc(sizeof(*ucp));
        memset(ucp, 0, sizeof(*ucp));
        ucp->nextp = userp->cellInfop;
        if (userp->cellInfop)
            ucp->iterator = userp->cellInfop->iterator + 1;
        else
            ucp->iterator = 1;
        userp->cellInfop = ucp;
        ucp->cellp = cellp;
    }
        
    return ucp;
}

cm_ucell_t *cm_FindUCell(cm_user_t *userp, int iterator)
{
    cm_ucell_t *ucp;
    cm_ucell_t *best;

    best = NULL;
    lock_AssertMutex(&userp->mx);
    for (ucp = userp->cellInfop; ucp; ucp = ucp->nextp) {
        if (ucp->iterator >= iterator)
            best = ucp;
        else
            break;
    }       
    return best;
}

void cm_HoldUser(cm_user_t *up)
{
    lock_ObtainWrite(&cm_userLock);
    up->refCount++;
    lock_ReleaseWrite(&cm_userLock);
}

void cm_ReleaseUser(cm_user_t *userp)
{
    cm_ucell_t *ucp;
    cm_ucell_t *ncp;

    if (userp == NULL) 
        return;

    lock_ObtainWrite(&cm_userLock);
    osi_assertx(userp->refCount-- > 0, "cm_user_t refCount 0");
    if (userp->refCount == 0) {
        lock_FinalizeMutex(&userp->mx);
        for (ucp = userp->cellInfop; ucp; ucp = ncp) {
            ncp = ucp->nextp;
            if (ucp->ticketp) 
                free(ucp->ticketp);
            free(ucp);
        }
        free(userp);
    }
    lock_ReleaseWrite(&cm_userLock);
}


void cm_HoldUserVCRef(cm_user_t *userp)
{
    lock_ObtainMutex(&userp->mx);
    userp->vcRefs++;
    lock_ReleaseMutex(&userp->mx);
}       

/* release the count of the # of connections that use this user structure.
 * When this hits zero, we know we won't be getting any new requests from
 * this user, and thus we can start GC'ing connections.  Ref count on user
 * won't hit zero until all cm_conn_t's have been GC'd, since they hold
 * refCount references to userp.
 */
void cm_ReleaseUserVCRef(cm_user_t *userp)
{
    lock_ObtainMutex(&userp->mx);
    osi_assertx(userp->vcRefs-- > 0, "cm_user_t refCount 0");
    lock_ReleaseMutex(&userp->mx);
}       


/*
 * Check if any users' tokens have expired and if they have then do the 
 * equivalent of unlogging the user for that particular cell for which 
 * the tokens have expired.
 * ref. cm_IoctlDelToken() in cm_ioctl.c 
 * This routine is called by the cm_Daemon() ie. the periodic daemon.
 * every cm_daemonTokenCheckInterval seconds 
 */
void cm_CheckTokenCache(time_t now)
{
    extern smb_vc_t *smb_allVCsp; /* global vcp list */
    smb_vc_t   *vcp;
    smb_user_t *usersp;
    cm_user_t  *userp = NULL;
    cm_ucell_t *ucellp;
    BOOL bExpired=FALSE;

    /* 
     * For every vcp, get the user and check his tokens 
     */
    lock_ObtainWrite(&smb_rctLock);
    for (vcp=smb_allVCsp; vcp; vcp=vcp->nextp) {
        for (usersp=vcp->usersp; usersp; usersp=usersp->nextp) {
            if (usersp->unp) {
                if ((userp=usersp->unp->userp)==0)
                    continue;
            } else
                continue;
            lock_ObtainMutex(&userp->mx);
            for (ucellp=userp->cellInfop; ucellp; ucellp=ucellp->nextp) {
				/* rxkad */
                if (ucellp->flags & CM_UCELLFLAG_RXKAD) {
                    if (ucellp->expirationTime < now) {
                        /* this guy's tokens have expired */
                        osi_Log3(afsd_logp, "cm_CheckTokens: Tokens for user:%s have expired expiration time:0x%x ucellp:%x", 
                                 ucellp->userName, ucellp->expirationTime, ucellp);
                        if (ucellp->ticketp) {
                            free(ucellp->ticketp);
                            ucellp->ticketp = NULL;
                        }
                        ucellp->flags &= ~CM_UCELLFLAG_RXKAD;
                        ucellp->gen++;
                        bExpired=TRUE;
                    }
                }
#ifdef AFS_RXK5				
				/* rxk5 */
                if (ucellp->flags & CM_UCELLFLAG_RXK5) {
                    if (ucellp->expirationTime < now) {
                        osi_Log3(afsd_logp, "cm_CheckTokens: K5 tokens for user:%s have expired expiration time:0x%x ucellp:%x", 
                                 ucellp->userName, ucellp->expirationTime, ucellp);
						if(ucellp->rxk5creds != NULL) {
							krb5_context k5context = rxk5_get_context(0);
							rxk5_free_creds(k5context, (rxk5_creds*) ucellp->rxk5creds);
												ucellp->rxk5creds = NULL;
    					}
                        ucellp->flags &= ~CM_UCELLFLAG_RXK5;
                        ucellp->gen++;
                        bExpired=TRUE;
                    }
                }
#endif			
            }
            lock_ReleaseMutex(&userp->mx);
            if (bExpired) {
                bExpired=FALSE;
                cm_ResetACLCache(userp);
            }
        }
    }
    lock_ReleaseWrite(&smb_rctLock);
}

#ifdef USE_ROOT_TOKENS
/*
 * Service/Parameters/RootTokens/<cellname>/
 * -> UseLSA
 * -> Keytab (required if UseLSA is 0)
 * -> Principal (required if there is more than one principal in the keytab)
 * -> Realm (required if realm is not upper-case of <cellname>
 * -> RequireEncryption 
 */

void
cm_RefreshRootTokens(void)
{

}
#endif 
