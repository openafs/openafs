/*
 * (C) COPYRIGHT IBM CORPORATION 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 *  Module:	    Voltrans.c
 *  System:	    Volser
 *  Instituition:	    ITC, CMU
 *  Date:		    December, 88
 */

#include <afs/param.h>
#ifdef AFS_NT40_ENV
#include <afs/afsutil.h>
#else
#include <sys/time.h>
#endif
#include <rx/rx.h>
#include "volser.h"

static struct volser_trans *allTrans=0;
static afs_int32 transCounter = 1;

/* create a new transaction, returning ptr to same with high ref count */
struct volser_trans *NewTrans(avol, apart)
afs_int32 avol;
afs_int32 apart; {
    /* set volid, next, partition */
    register struct volser_trans *tt;
    struct timeval tp;
    struct timezone tzp;

    /* don't allow the same volume to be attached twice */
    for(tt=allTrans;tt;tt=tt->next) {
	if ((tt->volid == avol) && (tt->partition == apart)){
	    return (struct volser_trans	*) 0;	/* volume busy */
	}
    }
    tt = (struct volser_trans *) malloc(sizeof(struct volser_trans));
    bzero(tt, sizeof(struct volser_trans));
    tt->volid = avol;
    tt->partition = apart;
    tt->next = allTrans;
    tt->tid = transCounter++;
    tt->refCount = 1;
    tt->rxCallPtr = (struct rx_call *)0;
    strcpy(tt->lastProcName,"");
    gettimeofday(&tp,&tzp);
    tt->creationTime = tp.tv_sec;
    allTrans = tt;
    tt->time = FT_ApproxTime();
    return tt;
}

/* find a trans, again returning with high ref count */
struct volser_trans *FindTrans(atrans)
register afs_int32 atrans; {
    register struct volser_trans *tt;
    for(tt=allTrans;tt;tt=tt->next) {
	if (tt->tid == atrans) {
	    tt->time = FT_ApproxTime();
	    tt->refCount++;
	    return tt;
	}
    }
    return (struct volser_trans *) 0;
}

/* delete transaction if refcount == 1, otherwise queue delete for later.  Does implicit TRELE */
DeleteTrans(atrans)
register struct volser_trans *atrans; {
    register struct volser_trans *tt, **lt;
    afs_int32 error;

    if (atrans->refCount > 1) {
	/* someone else is using it now */
	atrans->refCount--;
	atrans->tflags |= TTDeleted;
	return 0;
    }
    /* otherwise we zap it ourselves */
    lt = &allTrans;
    for(tt = *lt; tt; lt = &tt->next, tt = *lt) {
	if (tt == atrans) {
	    if (tt->volume)
		VDetachVolume(&error, tt->volume);
	    tt->volume = (struct Volume *) 0;
	    *lt = tt->next;
	    free(tt);
	    return 0;
	}
    }
    return -1;	/* failed to find the transaction in the generic list */
}

/* THOLD is a macro defined in volser.h */

/* put a transaction back */
TRELE (at)
register struct volser_trans *at; {
    if (at->refCount == 0){ 
	Log("TRELE: bad refcount\n");
	return VOLSERTRELE_ERROR;
    }
    
    at->time = FT_ApproxTime();	/* we're still using it */
    if (at->refCount == 1 && (at->tflags & TTDeleted)) {
	DeleteTrans(at);
	return 0;
    }
    /* otherwise simply drop refcount */
    at->refCount--;
    return 0;
}

/* look for old transactions and delete them */
#define	OLDTRANSTIME	    600	    /* seconds */
#define	OLDTRANSWARN	    300     /* seconds */
static int GCDeletes = 0;
GCTrans() 
{
    register struct volser_trans *tt, *nt;
    afs_int32 now;
    
    now = FT_ApproxTime();

    for(tt = allTrans; tt; tt=nt) {
	nt = tt->next;		/* remember in case we zap it */
	if (tt->time + OLDTRANSWARN < now) {
	    Log("trans %u on volume %u %s than %d seconds\n",
		tt->tid, tt->volid, 
		((tt->refCount>0)?"is older":"has been idle for more"),
		(((now-tt->time)/GCWAKEUP)*GCWAKEUP));
	}
	if (tt->refCount > 0) continue;
	if (tt->time + OLDTRANSTIME < now) {
	    Log("trans %u on volume %u has timed out\n", tt->tid, tt->volid);
	    tt->refCount++;	/* we're using it now */
	    DeleteTrans(tt);	/* drops refCount or deletes it */
	    GCDeletes++;
	}
    }
    return 0;
}
/*return the head of the transaction list */
struct volser_trans *TransList()
{
    return(allTrans);
}
