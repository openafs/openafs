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

osi_rwlock_t cm_dnlcLock;

cm_dnlcstats_t dnlcstats;	/* dnlc statistics */
int cm_useDnlc = 1; 		/* yes, start using the dnlc */
int cm_debugDnlc = 0;		/* debug dnlc */


/* Hash table invariants:
 *     1.  If nameHash[i] is NULL, list is empty
 *     2.  A single element in a hash bucket has itself as prev and next.
 */
struct nc 	*ncfreelist = (struct nc *)0;
static struct nc nameCache[NCSIZE];
struct nc*	nameHash[NHSIZE];


#define dnlcNotify(x,debug){                    \
                        HANDLE  hh;             \
                        char *ptbuf[1];         \
                        ptbuf[0] = x;           \
			if ( debug ) {		\
                            hh = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);   \
                            ReportEvent(hh,EVENTLOG_ERROR_TYPE,0,__LINE__,  \
				NULL, 1, 0, ptbuf, NULL);             	    \
                            DeregisterEventSource(hh);			\
			}						\
                     }  


static struct nc * 
GetMeAnEntry() 
{
    static unsigned int nameptr = 0; /* next bucket to pull something from */
    struct nc *tnc;
    int j;
  
    if (ncfreelist) 
    {
	tnc = ncfreelist;
	ncfreelist = tnc->next;
	return tnc;
    }

    for (j=0; j<NHSIZE+2; j++, nameptr++) 
    {
	if (nameptr >= NHSIZE) 
	    nameptr =0;
	if (nameHash[nameptr])
	    break;
    }

    tnc = nameHash[nameptr];
    if (!tnc)   
    {
	dnlcNotify("null tnc in GetMeAnEntry",1);
	osi_Log0(afsd_logp,"null tnc in GetMeAnEntry");
	return 0;
    }

    if (tnc->prev == tnc) 
    { 			/* only thing in list, don't screw around */
	nameHash[nameptr] = (struct nc *) 0;
	return (tnc);
    }

    tnc = tnc->prev;      	/* grab oldest one in this bucket */
    tnc->next->prev = tnc->prev;/* remove it from list */
    tnc->prev->next = tnc->next;

    return (tnc);
}

static void 
InsertEntry(tnc)
    struct nc *tnc;
{
    unsigned int key; 
    key = tnc->key & (NHSIZE -1);

    if(!nameHash[key]) 
    {
	nameHash[key] = tnc;
	tnc->next = tnc->prev = tnc;
    }
    else 
    {
	tnc->next = nameHash[key];
	tnc->prev = tnc->next->prev;
	tnc->next->prev = tnc;
	tnc->prev->next = tnc;
	nameHash[key] = tnc;
    }
}


void 
cm_dnlcEnter ( adp, aname, avc )
    cm_scache_t *adp;
    char        *aname;
    cm_scache_t *avc;
{
    struct nc *tnc;
    unsigned int key, skey, new=0;
    char *ts = aname;
    int safety;

    if (!cm_useDnlc)
	return ;
  
    if ( cm_debugDnlc ) 
	osi_Log3(afsd_logp,"cm_dnlcEnter dir %x name %s scache %x", 
	    adp, osi_LogSaveString(afsd_logp,aname), avc);

    dnlcHash( ts, key );  /* leaves ts pointing at the NULL */
    if (ts - aname >= CM_AFSNCNAMESIZE) 
	return ;
    skey = key & (NHSIZE -1);

    lock_ObtainWrite(&cm_dnlcLock);
    dnlcstats.enters++;
  
    for (tnc = nameHash[skey], safety=0; tnc; tnc = tnc->next, safety++ )
	if ((tnc->dirp == adp) && (!cm_stricmp(tnc->name, aname)))
	    break;				/* preexisting entry */
	else if ( tnc->next == nameHash[skey])	/* end of list */
	{
	    tnc = NULL;
	    break;
	}
	else if ( safety >NCSIZE) 
	{
	    dnlcstats.cycles++;
	    lock_ReleaseWrite(&cm_dnlcLock);

	    dnlcNotify("DNLC cycle",1);
	    if ( cm_debugDnlc )
                osi_Log0(afsd_logp, "DNLC cycle");
	    cm_dnlcPurge();
	    return;
	}
	
    if ( !tnc )
    {
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
    lock_ReleaseWrite(&cm_dnlcLock);

    if ( !tnc)
	cm_dnlcPurge();
}

/*
* if the scache entry is found, return it held
*/
cm_scache_t *
cm_dnlcLookup ( adp, sp)
  cm_scache_t *adp;
  cm_lookupSearch_t*      sp;
{
    cm_scache_t * tvc;
    unsigned int key, skey;
    char* aname = sp->searchNamep;
    char *ts = aname;
    struct nc * tnc;
    int safety, match;
  
    if (!cm_useDnlc)
	return 0;
    if ( cm_debugDnlc ) 
	osi_Log2(afsd_logp, "cm_dnlcLookup dir %x name %s", 
		adp, osi_LogSaveString(afsd_logp,aname));

    dnlcHash( ts, key );  /* leaves ts pointing at the NULL */
    if (ts - aname >= CM_AFSNCNAMESIZE) 
	return 0;

    skey = key & (NHSIZE -1);

    lock_ObtainRead(&cm_dnlcLock);
    dnlcstats.lookups++;	     /* Is a dnlcread lock sufficient? */

    for ( tvc = (cm_scache_t *) 0, tnc = nameHash[skey], safety=0; 
       tnc; tnc = tnc->next, safety++ ) 
    {
	if (tnc->dirp == adp) 
	{
	    if ( sp->caseFold ) 	/* case insensitive */
	    {
		match = cm_stricmp(tnc->name, aname);
		if ( !match )	/* something matches */
		{
			/* determine what type of match it is */
			if ( !strcmp(tnc->name, aname))
			{	
      				/* exact match, do nothing */
			}
			else if ( cm_NoneUpper(tnc->name))
				sp->LCfound = 1;
			else if ( cm_NoneLower(tnc->name))
				sp->UCfound = 1;
			else    sp->NCfound = 1;
      			tvc = tnc->vp; 
      			break;
		}
	    }
	    else			/* case sensitive */
	    {
		match = strcmp(tnc->name, aname);
		if ( !match ) /* found a match */
		{
      			tvc = tnc->vp; 
      			break;
		}
	    }
	}
	if (tnc->next == nameHash[skey]) 
    	{ 			/* end of list */
	    break;
	}
	else if (safety >NCSIZE) 
	{
	    dnlcstats.cycles++;
	    lock_ReleaseRead(&cm_dnlcLock);

	    dnlcNotify("DNLC cycle",1);	
	    if ( cm_debugDnlc ) 
		osi_Log0(afsd_logp, "DNLC cycle"); 
	    cm_dnlcPurge();
	    return(0);
	}
    }

    if (!tvc) 
	dnlcstats.misses++; 	/* Is a dnlcread lock sufficient? */
    else 
    {
	sp->found = 1;
	sp->fid.vnode  = tvc->fid.vnode; 
	sp->fid.unique = tvc->fid.unique;	
    }
    lock_ReleaseRead(&cm_dnlcLock);

    if (tvc) {
 	lock_ObtainWrite(&cm_scacheLock);
	tvc->refCount++;	/* scache entry held */
	lock_ReleaseWrite(&cm_scacheLock);
    }

    if ( cm_debugDnlc && tvc ) 
	osi_Log1(afsd_logp, "cm_dnlcLookup found %x", tvc);
    
    return tvc;
}


static int
RemoveEntry (tnc, key)
    struct nc    *tnc;
    unsigned int key;
{
    if (!tnc->prev) /* things on freelist always have null prev ptrs */
    {
	dnlcNotify("Bogus dnlc freelist", 1);
	osi_Log0(afsd_logp,"Bogus dnlc freelist");
	return 1;   /* error */
    }

    if (tnc == tnc->next)  /* only one in list */
	nameHash[key] = (struct nc *) 0;
    else 
    {
	if (tnc == nameHash[key])
	    nameHash[key]  = tnc->next;
	tnc->prev->next = tnc->next;
	tnc->next->prev = tnc->prev;
    }

    tnc->prev = (struct nc *) 0; /* everything not in hash table has 0 prev */
    tnc->key = 0; /* just for safety's sake */
    return 0;	  /* success */
}


void 
cm_dnlcRemove ( adp, aname)
    cm_scache_t *adp;
    char          *aname;
{
    unsigned int key, skey, error=0;
    int found= 0, safety;
    char *ts = aname;
    struct nc *tnc, *tmp;
  
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
    dnlcstats.removes++;

    for (tnc = nameHash[skey], safety=0; tnc; safety++) 
    {
	if ( (tnc->dirp == adp) && (tnc->key == key) 
			&& !cm_stricmp(tnc->name,aname) )
	{
	    tnc->dirp = (cm_scache_t *) 0; /* now it won't match anything */
	    tmp = tnc->next;
    	    error = RemoveEntry(tnc, skey);
	    if ( error )
		break;

	    tnc->next = ncfreelist; /* insert entry into freelist */
	    ncfreelist = tnc;
	    found = 1;		/* found atleast one entry */

	    tnc = tmp;		/* continue down the linked list */
	}
	else if (tnc->next == nameHash[skey]) /* end of list */
	    break;
	else
	    tnc = tnc->next;
	if ( safety > NCSIZE )
	{
	    dnlcstats.cycles++;
	    lock_ReleaseWrite(&cm_dnlcLock);

	    dnlcNotify("DNLC cycle",1);	
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
cm_dnlcPurgedp (adp)
  cm_scache_t *adp;
{
    int i;
    int err=0;

    if (!cm_useDnlc)
        return ;

    if ( cm_debugDnlc )
	osi_Log1(afsd_logp, "cm_dnlcPurgedp dir %x", adp);

    lock_ObtainWrite(&cm_dnlcLock);
    dnlcstats.purgeds++;

    for (i=0; i<NCSIZE && !err; i++) 
    {
	if (nameCache[i].dirp == adp ) 
	{
	    nameCache[i].dirp = nameCache[i].vp = (cm_scache_t *) 0;
	    if (nameCache[i].prev && !err) 
	    {
		err = RemoveEntry(&nameCache[i], nameCache[i].key & (NHSIZE-1));
		nameCache[i].next = ncfreelist;
		ncfreelist = &nameCache[i];
	    }
	}
    }
    lock_ReleaseWrite(&cm_dnlcLock);
    if ( err )
	cm_dnlcPurge();
}

/* remove anything pertaining to this file */
void 
cm_dnlcPurgevp ( avc )
  cm_scache_t *avc;
{
    int i;
    int err=0;

    if (!cm_useDnlc)
        return ;

    if ( cm_debugDnlc )
	osi_Log1(afsd_logp, "cm_dnlcPurgevp scache %x", avc);

    lock_ObtainWrite(&cm_dnlcLock);
    dnlcstats.purgevs++;

    for (i=0; i<NCSIZE && !err ; i++) 
    {
   	if (nameCache[i].vp == avc) 
	{
	    nameCache[i].dirp = nameCache[i].vp = (cm_scache_t *) 0;
	    /* can't simply break; because of hard links -- might be two */
	    /* different entries with same vnode */ 
	    if (!err && nameCache[i].prev) 
	    {
		err=RemoveEntry(&nameCache[i], nameCache[i].key & (NHSIZE-1));
		nameCache[i].next = ncfreelist;
		ncfreelist = &nameCache[i];
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
    dnlcstats.purges++;
    
    ncfreelist = (struct nc *) 0;
    memset (nameCache, 0, sizeof(struct nc) * NCSIZE);
    memset (nameHash, 0, sizeof(struct nc *) * NHSIZE);
    for (i=0; i<NCSIZE; i++) 
    {
	nameCache[i].next = ncfreelist;
	ncfreelist = &nameCache[i];
    }
    lock_ReleaseWrite(&cm_dnlcLock);
   
}

/* remove everything referencing a specific volume */
void
cm_dnlcPurgeVol( fidp )
  AFSFid *fidp;
{

    if (!cm_useDnlc)
        return ;

    dnlcstats.purgevols++;
    cm_dnlcPurge();
}

void 
cm_dnlcInit(void)
{
    int i;

    if (!cm_useDnlc)
        return ;
    if ( cm_debugDnlc )
	osi_Log0(afsd_logp,"cm_dnlcInit");

    lock_InitializeRWLock(&cm_dnlcLock, "cm_dnlcLock");
    memset (&dnlcstats, 0, sizeof(dnlcstats));
    lock_ObtainWrite(&cm_dnlcLock);
    ncfreelist = (struct nc *) 0;
    memset (nameCache, 0, sizeof(struct nc) * NCSIZE);
    memset (nameHash, 0, sizeof(struct nc *) * NHSIZE);
    for (i=0; i<NCSIZE; i++) 
    {
	nameCache[i].next = ncfreelist;
	ncfreelist = &nameCache[i];
    }
    lock_ReleaseWrite(&cm_dnlcLock);
}

void 
cm_dnlcShutdown(void)
{
    if ( cm_debugDnlc )
	osi_Log0(afsd_logp,"cm_dnlcShutdown");
}
