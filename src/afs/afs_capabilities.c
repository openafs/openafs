/*
 * Copyright (c) 2005, 2006
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <afsconfig.h>
#include "afs/param.h"

#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */
#include "afs/afs_stats.h"	/*Cache Manager stats */
#include "afs/afs_args.h"

struct CapEntry
{
    struct afs_q ceq;
    int klen,  vlen;
    char *key, *value;
};

struct afs_q cap_Queue;
afs_rwlock_t cap_queue_lock;
static afs_int32 cap_Initialized;

afs_int32 rxk5_InitCapabilities();

/* Internal Linkage */
  
static afs_int32 LenCapQueue(struct afs_q *ceq, int *cnt, int *len)
{
    struct CapEntry *ce;
    struct afs_q *cpq, *tq;
    *cnt = *len = 0;

    for (cpq = ceq->next; cpq != (struct afs_q*) &ceq; cpq = tq) {
	ce = (struct CapEntry *) cpq; /* todo: review */
	*len += ce->klen + ce->vlen;
	*cnt++;
	tq = QNext(cpq);
    }
    return *len;
}

static char* FormatCapBuf(struct afs_q *ceq, /* out */ afs_int32 *len) {
    struct CapEntry *ce;
    struct afs_q *cpq, *tq;
    char *str, *ptr;
    afs_int32 cnt;
    
    LenCapQueue(ceq, &cnt, len);
    *len += 3 * cnt + 1; /* formatting */
    str = (char*) afs_osi_Alloc(*len * sizeof(char));
    memset(str, 0, *len);
    ptr = str;
    for (cpq = ceq->next; cpq != (struct afs_q*) &ceq; cpq = tq) {
	ce = (struct CapEntry *) cpq;
	memcpy(ptr, ce->key, ce->klen * sizeof(char));
	ptr += ce->klen;
	ptr[0] = ':';
	ptr[1] = ':';
	ptr+=2;
	memcpy(ptr, ce->value, ce->vlen * sizeof(char));
	ptr+=ce->vlen;
	ptr[0] = '\n';
	ptr++;
	tq = QNext(cpq);
    }
    return str;
}

/* External Linkage */

int afs_AddCapability(const char* key, const char* value)
{
    afs_int32 r;
    struct CapEntry *ce;
    
    r = 0;
    ce = (struct CapEntry*) afs_osi_Alloc(sizeof(struct CapEntry));
    ce->key = afs_strdup((char*) key);
    ce->value = afs_strdup((char*) value);
    ce->klen = strlen(ce->key);
    ce->vlen = strlen(ce->value);

    /* todo: lock generally */
    ObtainWriteLock(&cap_queue_lock, 740);
    QAdd(&cap_Queue, &ce->ceq);
    ReleaseWriteLock(&cap_queue_lock);

    return r;
}

int afs_InitCapabilities()
{
    /* locks?  indices? */
    RWLOCK_INIT(&cap_queue_lock, "cap queue lock");
    QInit(&cap_Queue);

#ifdef AFS_RXK5
    rxk5_InitCapabilities();
#endif
    cap_Initialized = 1;

    return 0;
}

const char* afs_GetCapability(const char* key)
{
    struct CapEntry *ce;
    struct afs_q *cpq, *tq;
    char *v = 0;
    for (cpq = cap_Queue.next; cpq != &cap_Queue; cpq = tq) {
	ce = (struct CapEntry *) cpq;
	if(!strcmp(key, ce->key)) {
	    v = ce->value;
	    break;
	}
	tq = QNext(cpq);
    }
    return v;
}

char* afs_GetCapabilities(const char* qStr, /* out */ afs_int32 *qLen)
{

    afs_int32 all_wc, d_wc;
    char *sp, *dp, *k1, *k2, *nkey, *rslt;
    struct CapEntry *ce;
    struct afs_q rsltq, *cpq, *tq;

    if(!cap_Initialized) {
	/* log */
      afs_warn("afs_GetCapabilities: afs_GetCapabilities called but module no initialized");
      return NULL;
    }
    
    all_wc = 0;
    d_wc = 0;
    k1 = NULL;
    nkey = afs_strdup((char*) qStr);
    sp = strchr(nkey, '*');
    if((sp == nkey) && (*(sp+1) == 0)) {
	all_wc = 1;
    }
    dp = strchr(nkey, '.'); /* all platforms have strchr? */
    if(dp) {
	int pos = dp - nkey;
	d_wc = 1;
	k1 = (char*) nkey;
	k2 = dp + 1;
	if(dp) {
	    k1[pos] = 0;
	}
    }
    QInit(&rsltq);
    for (cpq = cap_Queue.next; cpq != &cap_Queue; cpq = tq) {
	int match_p = 0;
	ce = (struct CapEntry *) cpq;
	if(all_wc) {
	    match_p = 1;
	    goto loop_end;
	}
	if(d_wc && (strstr(ce->key, k1) == ce->key)) {
	    match_p = 1;
	    goto loop_end;
	}
	if(strcmp(nkey, ce->key) == 0) {
	    match_p = 1;
	}
    loop_end:
	if(match_p) {
	    QAdd(&rsltq, &ce->ceq);
	}
	tq = QNext(cpq);
    }    
    rslt = FormatCapBuf(&rsltq, qLen);
    afs_osi_FreeStr(nkey); /* osi_Frees strlen(nkey), ok here */
    return rslt;
}

#ifdef AFS_RXK5

static afs_int32 appendCapEnctype(char* dst, char* src, int *comma) {
    if(*comma == 0) {
	afs_strcat(dst, ",");
	*comma = 1;
    }
    afs_strcat(dst, src);
    return 0;
}

afs_int32 rxk5_InitCapabilities() {

    char * capStr;
    afs_int32 comma, capSize;
    
    afs_warn("rxk5_InitCapabilities called\n"); 

    comma = 0;
    capSize = 128;
    capStr = afs_osi_Alloc(capSize);
    memset(capStr, 0, capSize);

    appendCapEnctype(capStr, "1" /* DES_CBC_CRC  */, &comma);
    appendCapEnctype(capStr, "2" /* DES_CBC_MD4 */, &comma);
    appendCapEnctype(capStr, "3" /* DES_CBC_MD5 */, &comma);
    appendCapEnctype(capStr, "8" /* DES_HMAC_SHA1 */, &comma);
    appendCapEnctype(capStr, "16" /* DES3_CBC_SHA1 */, &comma);
    appendCapEnctype(capStr, "17" /* AES128_CTS_HMAC_SHA1_96  */, &comma);
    appendCapEnctype(capStr, "18" /* AES256_CTS_HMAC_SHA1_96 */, &comma);
    appendCapEnctype(capStr, "23" /* ARCFOUR_HMAC_MD5 */, &comma);
    appendCapEnctype(capStr, "24" /* ARCFOUR_HMAC_MD5_56 */, &comma);
    afs_AddCapability("rxk5.enctypes", capStr);

    osi_Free(capStr, capSize);
    return 0;
}
#endif

