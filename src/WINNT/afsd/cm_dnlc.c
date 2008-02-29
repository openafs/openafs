/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
**	This implements the directory to name cache lookup. 
**	Given a directory scache and a name, the dnlc returns the
**	vcache corresponding to the name. The vcache entries that 
**	exist in the dnlc are not refcounted. 
**
*/

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <string.h>
#include <stdlib.h>
#include <osi.h>
#include "afsd.h"
#include <WINNT/afsreg.h>

osi_rwlock_t cm_dnlcLock;

static cm_dnlcstats_t dnlcstats;	/* dnlc statistics */
static int cm_useDnlc = 1; 	/* yes, start using the dnlc */
static int cm_debugDnlc = 0;	/* debug dnlc */


/* Hash table invariants:
 *     1.  If nameHash[i] is NULL, list is empty
 *     2.  A single element in a hash bucket has itself as prev and next.
 */

/* Must be called with cm_dnlcLock write locked */
static cm_nc_t * 
GetMeAnEntry() 
{
    static unsigned int nameptr = 0; /* next bucket to pull something from */
    cm_nc_t *tnc;
    int j;
  
    if (cm_data.ncfreelist) 
    {
	tnc = cm_data.ncfreelist;
	cm_data.ncfreelist = tnc->next;
	return tnc;
    }

    for (j=0; j<NHSIZE+2; j++, nameptr++) 
    {
	if (nameptr >= NHSIZE) 
	    nameptr =0;
	if (cm_data.nameHash[nameptr])
	    break;
    }

    tnc = cm_data.nameHash[nameptr];
    if (!tnc)   
    {
	osi_Log0(afsd_logp,"null tnc in GetMeAnEntry");
	return 0;
    }

    if (tnc->prev == tnc) 
    { 			/* only thing in list, don't screw around */
	cm_data.nameHash[nameptr] = (cm_nc_t *) 0;
	return (tnc);
    }

    tnc = tnc->prev;      	/* grab oldest one in this bucket */
    tnc->next->prev = tnc->prev;/* remove it from list */
    tnc->prev->next = tnc->next;

    return (tnc);
}

static void 
InsertEntry(cm_nc_t *tnc)
{
    unsigned int key; 
    key = tnc->key & (NHSIZE -1);

    if (!cm_data.nameHash[key]) 
    {
	cm_data.nameHash[key] = tnc;
	tnc->next = tnc->prev = tnc;
    }
    else 
    {
	tnc->next = cm_data.nameHash[key];
	tnc->prev = tnc->next->prev;
	tnc->next->prev = tnc;
	tnc->prev->next = tnc;
	cm_data.nameHash[key] = tnc;
    }
}


void 
cm_dnlcEnter ( cm_scache_t *adp,
               char        *aname,
               cm_scache_t *avc )
{
    cm_nc_t *tnc;
    unsigned int key, skey, new=0;
    char *ts = aname;
    int safety;
    int writeLocked = 0;

    if (!cm_useDnlc)
	return ;

    if (!strcmp(aname,".") || !strcmp(aname,".."))
	return ;

    if ( cm_debugDnlc ) 
	osi_Log3(afsd_logp,"cm_dnlcEnter dir %x name %s scache %x", 
	    adp, osi_LogSaveString(afsd_logp,aname), avc);

    dnlcHash( ts, key );  /* leaves ts pointing at the NULL */
    if (ts - aname >= CM_AFSNCNAMESIZE) 
	return ;
    skey = key & (NHSIZE -1);

    InterlockedIncrement(&dnlcstats.enters);
    lock_ObtainRead(&cm_dnlcLock);
  retry:
    for (tnc = cm_data.nameHash[skey], safety=0; tnc; tnc = tnc->next, safety++ )
	if ((tnc->dirp == adp) && (!strcmp(tnc->name, aname)))
	    break;				/* preexisting entry */
	else if ( tnc->next == cm_data.nameHash[skey])	/* end of list */
	{
	    tnc = NULL;
	    break;
	}
	else if (safety > NCSIZE) 
	{
	    InterlockedIncrement(&dnlcstats.cycles);
            if (writeLocked)
                lock_ReleaseWrite(&cm_dnlcLock);
            else
                lock_ReleaseRead(&cm_dnlcLock);

	    if ( cm_debugDnlc )
                osi_Log0(afsd_logp, "DNLC cycle");
	    cm_dnlcPurge();
	    return;
	}
	
    if ( !tnc )
    {
        if ( !writeLocked ) {
            lock_ConvertRToW(&cm_dnlcLock);
            writeLocked = 1;
            goto retry;
        }
	new = 1;	/* entry does not exist, we are creating a new entry*/
	tnc = GetMeAnEntry();
    }
    if ( tnc )
    { 
	tnc->dirp = adp;
	tnc->vp = avc;
	tnc->key = key;
	memcpy (tnc->name, aname, ts-aname+1); /* include the NULL */

    	if ( new )	/* insert entry only if it is newly created */ 
		InsertEntry(tnc);

    }
    if (writeLocked)
        lock_ReleaseWrite(&cm_dnlcLock);
    else
        lock_ReleaseRead(&cm_dnlcLock);

    if ( !tnc)
	cm_dnlcPurge();
}

/*
* if the scache entry is found, return it held
*/
cm_scache_t *
cm_dnlcLookup (cm_scache_t *adp, cm_lookupSearch_t* sp)
{
    cm_scache_t * tvc;
    unsigned int key, skey;
    char* aname = sp->searchNamep;
    char *ts = aname;
    cm_nc_t * tnc, * tnc_begin;
    int safety, match;
  
    if (!cm_useDnlc)
	return NULL;

    if ( cm_debugDnlc ) 
	osi_Log2(afsd_logp, "cm_dnlcLookup dir %x name %s", 
		adp, osi_LogSaveString(afsd_logp,aname));

    dnlcHash( ts, key );  /* leaves ts pointing at the NULL */

    if (ts - aname >= CM_AFSNCNAMESIZE) {
        InterlockedIncrement(&dnlcstats.lookups);
        InterlockedIncrement(&dnlcstats.misses);
	return NULL;
    }

    skey = key & (NHSIZE -1);

    lock_ObtainRead(&cm_dnlcLock);
    InterlockedIncrement(&dnlcstats.lookups);

    ts = 0;
    tnc_begin = cm_data.nameHash[skey];
    for ( tvc = (cm_scache_t *) NULL, tnc = tnc_begin, safety=0; 
          tnc; tnc = tnc->next, safety++ ) 
    {
	if (tnc->dirp == adp) 
	{
        if( cm_debugDnlc ) 
            osi_Log1(afsd_logp,"Looking at [%s]",
                     osi_LogSaveString(afsd_logp,tnc->name));

	    if ( sp->caseFold ) 	/* case insensitive */
	    {
            match = cm_stricmp(tnc->name, aname);
            if ( !match )	/* something matches */
            {
                tvc = tnc->vp;
                ts = tnc->name;

                /* determine what type of match it is */
                if ( !strcmp(tnc->name, aname))
                {	
                    /* exact match. */
                    sp->ExactFound = 1;

                    if( cm_debugDnlc )
                        osi_Log1(afsd_logp,"DNLC found exact match [%s]",
                                 osi_LogSaveString(afsd_logp,tnc->name));
                    break;
                }
                else if ( cm_NoneUpper(tnc->name))
                    sp->LCfound = 1;
                else if ( cm_NoneLower(tnc->name))
                    sp->UCfound = 1;
                else    
                    sp->NCfound = 1;
                /* Don't break here. We might find an exact match yet */
            }
	    }
	    else			/* case sensitive */
	    {
            match = strcmp(tnc->name, aname);
            if ( !match ) /* found a match */
            {
                sp->ExactFound = 1;
                tvc = tnc->vp; 
                ts = tnc->name;
                break;
            }
	    }
	}
	if (tnc->next == cm_data.nameHash[skey]) 
    { 			/* end of list */
	    break;
	}
	else if (tnc->next == tnc_begin || safety > NCSIZE) 
	{
	    InterlockedIncrement(&dnlcstats.cycles);
	    lock_ReleaseRead(&cm_dnlcLock);

	    if ( cm_debugDnlc ) 
		osi_Log0(afsd_logp, "DNLC cycle"); 
	    cm_dnlcPurge();
	    return(NULL);
	}
    }

    if(cm_debugDnlc && ts) {
        osi_Log3(afsd_logp, "DNLC matched [%s] for [%s] with vnode[%ld]",
                 osi_LogSaveString(afsd_logp,ts),
                 osi_LogSaveString(afsd_logp,aname),
                 (long) tvc->fid.vnode);
    }

    if (!tvc) 
        InterlockedIncrement(&dnlcstats.misses);
    else 
    {
        sp->found = 1;
        sp->fid.vnode  = tvc->fid.vnode; 
        sp->fid.unique = tvc->fid.unique;	
    }
    lock_ReleaseRead(&cm_dnlcLock);

    if (tvc)
        cm_HoldSCache(tvc);

    if ( cm_debugDnlc && tvc ) 
        osi_Log1(afsd_logp, "cm_dnlcLookup found %x", tvc);
    
    return tvc;
}


static int
RemoveEntry (cm_nc_t *tnc, unsigned int key)
{
    if (!tnc->prev) /* things on freelist always have null prev ptrs */
    {
	osi_Log0(afsd_logp,"Bogus dnlc freelist");
	return 1;   /* error */
    }

    if (tnc == tnc->next)  /* only one in list */
	cm_data.nameHash[key] = (cm_nc_t *) 0;
    else 
    {
	if (tnc == cm_data.nameHash[key])
	    cm_data.nameHash[key]  = tnc->next;
	tnc->prev->next = tnc->next;
	tnc->next->prev = tnc->prev;
    }

    memset(tnc, 0, sizeof(cm_nc_t));
    tnc->magic = CM_DNLC_MAGIC;
    return 0;	  /* success */
}


void 
cm_dnlcRemove (cm_scache_t *adp, char *aname)
{
    unsigned int key, skey, error=0;
    int found= 0, safety;
    char *ts = aname;
    cm_nc_t *tnc, *tmp;
  
    if (!cm_useDnlc)
	return ;

    if ( cm_debugDnlc )
	osi_Log2(afsd_logp, "cm_dnlcRemove dir %x name %s", 
		adp, osi_LogSaveString(afsd_logp,aname));

    dnlcHash( ts, key );  /* leaves ts pointing at the NULL */
    if (ts - aname >= CM_AFSNCNAMESIZE) 
	return ;

    skey = key & (NHSIZE -1);
    lock_ObtainWrite(&cm_dnlcLock);
    InterlockedIncrement(&dnlcstats.removes);

    for (tnc = cm_data.nameHash[skey], safety=0; tnc; safety++) 
    {
	if ( (tnc->dirp == adp) && (tnc->key == key) 
			&& !strcmp(tnc->name,aname) )
	{
	    tmp = tnc->next;
    	    error = RemoveEntry(tnc, skey);
	    if ( error )
		break;

	    tnc->next = cm_data.ncfreelist; /* insert entry into freelist */
	    cm_data.ncfreelist = tnc;
	    found = 1;		/* found at least one entry */

	    tnc = tmp;		/* continue down the linked list */
	}
	else if (tnc->next == cm_data.nameHash[skey]) /* end of list */
	    break;
	else
	    tnc = tnc->next;
	if ( safety > NCSIZE )
	{
	    InterlockedIncrement(&dnlcstats.cycles);
	    lock_ReleaseWrite(&cm_dnlcLock);

	    if ( cm_debugDnlc ) 
		osi_Log0(afsd_logp, "DNLC cycle"); 
	    cm_dnlcPurge();
	    return;
	}
    }
    lock_ReleaseWrite(&cm_dnlcLock);

    if (!found && !error && cm_debugDnlc)
	osi_Log0(afsd_logp, "cm_dnlcRemove name not found");

    if ( error )
	cm_dnlcPurge();
}

/* remove anything pertaining to this directory */
void 
cm_dnlcPurgedp (cm_scache_t *adp)
{
    int i;
    int err=0;

    if (!cm_useDnlc)
        return ;

    if ( cm_debugDnlc )
	osi_Log1(afsd_logp, "cm_dnlcPurgedp dir %x", adp);

    lock_ObtainWrite(&cm_dnlcLock);
    InterlockedIncrement(&dnlcstats.purgeds);

    for (i=0; i<NCSIZE && !err; i++) 
    {
	if (cm_data.nameCache[i].dirp == adp ) 
	{
	    if (cm_data.nameCache[i].prev) {
                err = RemoveEntry(&cm_data.nameCache[i], cm_data.nameCache[i].key & (NHSIZE-1));
                if (!err)
                {
                    cm_data.nameCache[i].next = cm_data.ncfreelist;
                    cm_data.ncfreelist = &cm_data.nameCache[i];
                }
	    } else {
                cm_data.nameCache[i].dirp = cm_data.nameCache[i].vp = (cm_scache_t *) 0;
            }
	}
    }
    lock_ReleaseWrite(&cm_dnlcLock);
    if ( err )
	cm_dnlcPurge();
}

/* remove anything pertaining to this file */
void 
cm_dnlcPurgevp (cm_scache_t *avc)
{
    int i;
    int err=0;

    if (!cm_useDnlc)
        return ;

    if ( cm_debugDnlc )
	osi_Log1(afsd_logp, "cm_dnlcPurgevp scache %x", avc);

    lock_ObtainWrite(&cm_dnlcLock);
    InterlockedIncrement(&dnlcstats.purgevs);

    for (i=0; i<NCSIZE && !err ; i++) 
    {
   	if (cm_data.nameCache[i].vp == avc) 
	{
	    if (cm_data.nameCache[i].prev) {
                err = RemoveEntry(&cm_data.nameCache[i], cm_data.nameCache[i].key & (NHSIZE-1));
                if (!err)
                {
                    cm_data.nameCache[i].next = cm_data.ncfreelist;
                    cm_data.ncfreelist = &cm_data.nameCache[i];
                }
	    } else {
                cm_data.nameCache[i].dirp = cm_data.nameCache[i].vp = (cm_scache_t *) 0;
            }
	}
    }
    lock_ReleaseWrite(&cm_dnlcLock);
    if ( err )
	cm_dnlcPurge();
}

/* remove everything */
void cm_dnlcPurge(void)
{
    int i;

    if (!cm_useDnlc)
        return ;

    if ( cm_debugDnlc )
	osi_Log0(afsd_logp, "cm_dnlcPurge");

    lock_ObtainWrite(&cm_dnlcLock);
    InterlockedIncrement(&dnlcstats.purges);
    
    cm_data.ncfreelist = (cm_nc_t *) 0;
    memset (cm_data.nameCache, 0, sizeof(cm_nc_t) * NCSIZE);
    memset (cm_data.nameHash, 0, sizeof(cm_nc_t *) * NHSIZE);
    for (i=0; i<NCSIZE; i++)
    {
        cm_data.nameCache[i].magic = CM_DNLC_MAGIC;
        cm_data.nameCache[i].next = cm_data.ncfreelist;
        cm_data.ncfreelist = &cm_data.nameCache[i];
    }
    lock_ReleaseWrite(&cm_dnlcLock);
   
}

/* remove everything referencing a specific volume */
/* is this function ever called? */
void
cm_dnlcPurgeVol(AFSFid *fidp)
{

    if (!cm_useDnlc)
        return ;

    InterlockedIncrement(&dnlcstats.purgevols);
    cm_dnlcPurge();
}

long
cm_dnlcValidate(void)
{
    int i, purged = 0;
    cm_nc_t * ncp;

  retry:
    // are all nameCache entries marked with the magic bit?
    for (i=0; i<NCSIZE; i++)
    {
        if (cm_data.nameCache[i].magic != CM_DNLC_MAGIC) {
            afsi_log("cm_dnlcValidate failure: cm_data.nameCache[%d].magic != CM_DNLC_MAGIC", i);
            fprintf(stderr, "cm_dnlcValidate failure: cm_data.nameCache[%d].magic != CM_DNLC_MAGIC\n", i);
            goto purge;
        }
        if (cm_data.nameCache[i].next &&
            cm_data.nameCache[i].next->magic != CM_DNLC_MAGIC) {
            afsi_log("cm_dnlcValidate failure: cm_data.nameCache[%d].next->magic != CM_DNLC_MAGIC", i);
            fprintf(stderr, "cm_dnlcValidate failure: cm_data.nameCache[%d].next->magic != CM_DNLC_MAGIC\n", i);
            goto purge;
        }
        if (cm_data.nameCache[i].prev &&
            cm_data.nameCache[i].prev->magic != CM_DNLC_MAGIC) {
            afsi_log("cm_dnlcValidate failure: cm_data.nameCache[%d].prev->magic != CM_DNLC_MAGIC", i);
            fprintf(stderr, "cm_dnlcValidate failure: cm_data.nameCache[%d].prev->magic != CM_DNLC_MAGIC\n", i);
            goto purge;
        }
        if (cm_data.nameCache[i].dirp &&
            cm_data.nameCache[i].dirp->magic != CM_SCACHE_MAGIC) {
            afsi_log("cm_dnlcValidate failure: cm_data.nameCache[%d].dirp->magic != CM_SCACHE_MAGIC", i);
            fprintf(stderr, "cm_dnlcValidate failure: cm_data.nameCache[%d].dirp->magic != CM_SCACHE_MAGIC\n", i);
            goto purge;
        }
        if (cm_data.nameCache[i].vp &&
            cm_data.nameCache[i].vp->magic != CM_SCACHE_MAGIC) {
            afsi_log("cm_dnlcValidate failure: cm_data.nameCache[%d].vp->magic != CM_SCACHE_MAGIC", i);
            fprintf(stderr, "cm_dnlcValidate failure: cm_data.nameCache[%d].vp->magic != CM_SCACHE_MAGIC\n", i);
            goto purge;
        }
    }

    // are the contents of the hash table intact?
    for (i=0; i<NHSIZE;i++) {
        for (ncp = cm_data.nameHash[i]; ncp ; 
             ncp = ncp->next != cm_data.nameHash[i] ? ncp->next : NULL) {
            if (ncp->magic != CM_DNLC_MAGIC) {
                afsi_log("cm_dnlcValidate failure: ncp->magic != CM_DNLC_MAGIC");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->magic != CM_DNLC_MAGIC\n");
                goto purge;
            }
            if (ncp->prev && ncp->prev->magic != CM_DNLC_MAGIC) {
                afsi_log("cm_dnlcValidate failure: ncp->prev->magic != CM_DNLC_MAGIC");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->prev->magic != CM_DNLC_MAGIC\n");
                goto purge;
            }
            if (ncp->dirp && ncp->dirp->magic != CM_SCACHE_MAGIC) {
                afsi_log("cm_dnlcValidate failure: ncp->dirp->magic != CM_DNLC_MAGIC");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->dirp->magic != CM_DNLC_MAGIC\n");
                goto purge;
            }
            if (ncp->vp && ncp->vp->magic != CM_SCACHE_MAGIC) {
                afsi_log("cm_dnlcValidate failure: ncp->vp->magic != CM_DNLC_MAGIC");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->vp->magic != CM_DNLC_MAGIC\n");
                goto purge;
            }
        }
    }

    // is the freelist stable?
    if ( cm_data.ncfreelist ) {
        for (ncp = cm_data.ncfreelist, i = 0; ncp && i < NCSIZE; 
             ncp = ncp->next != cm_data.ncfreelist ? ncp->next : NULL, i++) {
            if (ncp->magic != CM_DNLC_MAGIC) {
                afsi_log("cm_dnlcValidate failure: ncp->magic != CM_DNLC_MAGIC");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->magic != CM_DNLC_MAGIC\n");
                goto purge;
            }
            if (ncp->prev) {
                afsi_log("cm_dnlcValidate failure: ncp->prev != NULL");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->prev != NULL\n");
                goto purge;
            }
            if (ncp->key) {
                afsi_log("cm_dnlcValidate failure: ncp->key != 0");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->key != 0\n");
                goto purge;
            }
            if (ncp->dirp) {
                afsi_log("cm_dnlcValidate failure: ncp->dirp != NULL");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->dirp != NULL\n");
               goto purge;
            }
            if (ncp->vp) {
                afsi_log("cm_dnlcValidate failure: ncp->vp != NULL");
                fprintf(stderr, "cm_dnlcValidate failure: ncp->vp != NULL\n");
                goto purge;
            }
        }

        if ( i == NCSIZE && ncp ) {
            afsi_log("cm_dnlcValidate failure: dnlc freeList corrupted");
            fprintf(stderr, "cm_dnlcValidate failure: dnlc freeList corrupted\n");
            goto purge;
        }
    }
    return 0;

  purge:
    if ( purged )
        return -1;

    afsi_log("cm_dnlcValidate information: purging");
    fprintf(stderr, "cm_dnlcValidate information: purging\n");
    cm_data.ncfreelist = (cm_nc_t *) 0;
    memset (cm_data.nameCache, 0, sizeof(cm_nc_t) * NCSIZE);
    memset (cm_data.nameHash, 0, sizeof(cm_nc_t *) * NHSIZE);
    for (i=0; i<NCSIZE; i++)
    {
        cm_data.nameCache[i].magic = CM_DNLC_MAGIC;
        cm_data.nameCache[i].next = cm_data.ncfreelist;
        cm_data.ncfreelist = &cm_data.nameCache[i];
    }
    purged = 1;
    goto retry;
}

void 
cm_dnlcInit(int newFile)
{
    int i;
    HKEY parmKey;
    DWORD dummyLen;
    DWORD dwValue;
    DWORD code;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(DWORD);
        code = RegQueryValueEx(parmKey, "UseDNLC", NULL, NULL,
                                (BYTE *) &dwValue, &dummyLen);
        if (code == ERROR_SUCCESS)
            cm_useDnlc = dwValue ? 1 : 0;
        afsi_log("CM UseDNLC = %d", cm_useDnlc);

        dummyLen = sizeof(DWORD);
        code = RegQueryValueEx(parmKey, "DebugDNLC", NULL, NULL,
                                (BYTE *) &dwValue, &dummyLen);
        if (code == ERROR_SUCCESS)
            cm_debugDnlc = dwValue ? 1 : 0;
        afsi_log("CM DebugDNLC = %d", cm_debugDnlc);
        RegCloseKey (parmKey);
    }

    if ( cm_debugDnlc )
	osi_Log0(afsd_logp,"cm_dnlcInit");

    memset (&dnlcstats, 0, sizeof(dnlcstats));

    lock_InitializeRWLock(&cm_dnlcLock, "cm_dnlcLock");
    if ( newFile ) {
        lock_ObtainWrite(&cm_dnlcLock);
        cm_data.ncfreelist = (cm_nc_t *) 0;
        cm_data.nameCache = cm_data.dnlcBaseAddress;
        memset (cm_data.nameCache, 0, sizeof(cm_nc_t) * NCSIZE);
        cm_data.nameHash = (cm_nc_t **) (cm_data.nameCache + NCSIZE);
        memset (cm_data.nameHash, 0, sizeof(cm_nc_t *) * NHSIZE);
    
        for (i=0; i<NCSIZE; i++)
        {
            cm_data.nameCache[i].magic = CM_DNLC_MAGIC;
            cm_data.nameCache[i].next = cm_data.ncfreelist;
            cm_data.ncfreelist = &cm_data.nameCache[i];
        }
        lock_ReleaseWrite(&cm_dnlcLock);
    }
}

long 
cm_dnlcShutdown(void)
{
    if ( cm_debugDnlc )
	osi_Log0(afsd_logp,"cm_dnlcShutdown");

    return 0;
}
