/*
 * Copyright (c) 2005, 2006, 2007
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

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/afs_args.h>
#include <osi.h>

#include "afsd.h"

#ifdef AFS_RXK5
/* BEWARE: this code uses "u".  Must include heimdal krb5.h (u field name)
 * before libuafs afs/sysincludes.h (libuafs makes u a function.)
 */
#if defined(USING_K5SSL)
#include "k5ssl.h"
#else
#include <krb5.h>
#include <rx/rxk5_ntfixprotos.h>
#endif
#endif

/*
  * Queues implemented with both pointers and short offsets into a disk file.
  */
struct afs_q {
    struct afs_q *next;
    struct afs_q *prev;
};

/*
  * Operations on circular queues implemented with pointers.  Note: these queue
  * objects are always located at the beginning of the structures they are linking.
  */
#define	QInit(q)    ((q)->prev = (q)->next = (q))
#define	QAdd(q,e)   ((e)->next = (q)->next, (e)->prev = (q), \
			(q)->next->prev = (e), (q)->next = (e))
#define	QRemove(e)  ((e)->next->prev = (e)->prev, (e)->prev->next = (e)->next, (e)->prev = NULL, (e)->next = NULL)
#define	QNext(e)    ((e)->next)
#define QPrev(e)    ((e)->prev)
#define QEmpty(q)   ((q)->prev == (q))
/* this one takes q1 and sticks it on the end of q2 - that is, the other end, not the end
 * that things are added onto.  q1 shouldn't be empty, it's silly */
#define QCat(q1,q2) ((q2)->prev->next = (q1)->next, (q1)->next->prev=(q2)->prev, (q1)->prev->next=(q2), (q2)->prev=(q1)->prev, (q1)->prev=(q1)->next=(q1))

#define afs_strdup strdup
#define afs_osi_Alloc osi_Alloc
#define afs_osi_Free osi_Free

struct PropEntry
{
    struct afs_q ceq;
    int klen,  vlen;
    char *key, *value;
};

struct afs_q prop_Queue;
osi_rwlock_t prop_queue_lock;
static afs_int32 prop_Initialized;

afs_int32 rxk5_InitProperties();

/* Internal Linkage */
  
static afs_int32 LenPropQueue(struct afs_q *ceq, afs_int32 *cnt, afs_int32 *len)
{
    struct PropEntry *ce;
    struct afs_q *cpq, *tq;
    *cnt = *len = 0;

    for (cpq = ceq->next; cpq != (struct afs_q*) ceq; cpq = tq) {
	ce = (struct PropEntry *) cpq; /* todo: review */
	*len += ce->klen + ce->vlen;
	++ (*cnt);
	tq = QNext(cpq);
    }
    return *len;
}

static char *
FormatPropBuf(struct afs_q *ceq, /* out */ afs_int32 *len)
{
    struct PropEntry *ce;
    struct afs_q *cpq, *tq;
    char *str, *ptr;
    afs_int32 cnt;
    
    LenPropQueue(ceq, &cnt, len);
    *len += 2 * cnt + 1; /* formatting */
    str = (char*) afs_osi_Alloc(*len * sizeof(char));
    ptr = str;
    for (cpq = ceq->next; cpq != (struct afs_q*) ceq; cpq = tq) {
	ce = (struct PropEntry *) cpq;
	memcpy(ptr, ce->key, ce->klen);
	ptr += ce->klen;
	*ptr++ = 0;
	memcpy(ptr, ce->value, ce->vlen);
	ptr += ce->vlen;
	*ptr++ = 0;
	tq = QNext(cpq);
    }
    *ptr++ = 0;
    return str;
}

/* External Linkage */

int afs_AddProperty(const char* key, const char* value)
{
    afs_int32 r;
    struct PropEntry *ce;
    
    r = 0;
    ce = (struct PropEntry*) afs_osi_Alloc(sizeof(struct PropEntry));
    ce->key = afs_strdup((char*) key);
    ce->value = afs_strdup((char*) value);
    ce->klen = strlen(ce->key);
    ce->vlen = strlen(ce->value);

    lock_ObtainWrite(&prop_queue_lock);    
    QAdd(&prop_Queue, &ce->ceq);
    lock_ReleaseWrite(&prop_queue_lock);
    return r;
}

int afs_InitProperties()
{
    lock_InitializeRWLock(&prop_queue_lock, "prop queue lock");    
    QInit(&prop_Queue);

#ifdef AFS_RXK5
    rxk5_InitProperties();
#endif
    prop_Initialized = 1;

    return 0;
}

const char* afs_GetProperty(const char* key)
{
    struct PropEntry *ce;
    struct afs_q *cpq, *tq;
    char *v = 0;
    for (cpq = prop_Queue.next; cpq != &prop_Queue; cpq = tq) {
	ce = (struct PropEntry *) cpq;
	if(!strcmp(key, ce->key)) {
	    v = ce->value;
	    break;
	}
	tq = QNext(cpq);
    }
    return v;
}

int
afs_Property_Match(const char *pattern, const char *key)
{
    for (;;) {
	if (*pattern == *key) {
	    if (!*pattern) return 1;
	    ++pattern; ++key;
	    continue;
	}
	/* *. matches "the rest of this field" */
	/* *\0 matches "the rest of the key" */
	/* *X means match up to X */
	if (*pattern == '*') {
	    ++pattern;
	    while (*key && *key != *pattern) ++key;
	    continue;
	}
	break;
    }
    return 0;
}

/*
 * return a special string with embedded nulls.
 * BEWARE.
 *  Returns key value key value ... 0
 *  each key & value is null terminated.  an "empty" key (length=0)
 *  terminates the list.
 * input is also a list of strings, but delimited by qStrlen.
 */
char*
afs_GetProperties(const char* qStr, int qStrlen, /* out */ afs_int32 *qLen)
{
    char *rslt = 0;
    struct PropEntry *ce, *tq;
    struct afs_q rsltq, *cpq;
    char **keys = 0, *cp;
    int keylen, numkeys, i;

    if(!prop_Initialized) {
	/* log */
      osi_Log0(afsd_logp, "afs_GetProperties: afs_GetProperties called but module no initialized");
      return NULL;
    }
    keylen = numkeys = 0;
    for (i = 0; i < qStrlen; ++i) {
	if (!qStr[i]) ++numkeys;
    }
    keylen = qStrlen + numkeys*sizeof *keys;
    keys = afs_osi_Alloc(keylen);
    if (!keys) return NULL;	/* XXX */
    cp = (char *)(keys + numkeys);
    memcpy(cp, qStr, qStrlen);
    for (i = 0; i < numkeys; ++i) {
	keys[i] = cp;
	cp += strlen(cp)+1;
    }

    QInit(&rsltq);
    tq = (void *) &prop_Queue;
    while (&(tq = (void*)QNext(&tq->ceq))->ceq != &prop_Queue) {
	for (i = 0; i < numkeys; ++i) {
	    if (afs_Property_Match(keys[i], tq->key)) {
		ce = (struct PropEntry*) afs_osi_Alloc(sizeof(struct PropEntry));
		if (!ce) goto Done;
		*ce = *tq;
		QAdd(&rsltq, &ce->ceq);
		break;
	    }
	}
    }
    rslt = FormatPropBuf(&rsltq, qLen);
Done:
    while ((cpq = QNext(&rsltq)) && cpq != &rsltq) {
	QRemove(cpq);
	afs_osi_Free(cpq, sizeof(struct PropEntry));
    }
    if (keys)
	afs_osi_Free(keys, keylen);
    return rslt;
}

#ifdef AFS_RXK5

#if !defined(USING_K5SSL)
static int
krb5i_iterate_enctypes(int (*f)(void *, krb5_enctype,
	char *const *,
	void (*)(unsigned int *, unsigned int *),
	void (*)(unsigned int *, unsigned int *)),
    void *a)
{
    krb5_enctype ke;
    int i, r;

    for (i = -30; i < 60; ++i) {
	ke = ((46-i)^36)-8;	/* 18 17 16 23 8 3 2 1 24, +- */
	if (!krb5_c_valid_enctype(ke)) continue;
	r = f(a,ke,0,0,0);
	if (r != -1) return r;
    }
    return 0;
}
#endif

struct rxk5_prop_arg {
    struct afs_q q;
    char number[20];
};

int
afs_prop_rxk5_helper(void *a, krb5_enctype enctype,
        char *const *names,
        void (*block_size)(unsigned int *, unsigned int *),
        void (*key_size)(unsigned int *, unsigned int *))
{
    struct rxk5_prop_arg *q = (struct rxk5_prop_arg *) a;
    struct rxk5_prop_arg *t;
    if ((t = afs_osi_Alloc(sizeof *t))) {
	sprintf(t->number, "%d", enctype);
	QAdd(&q->q, &t->q);
    }
    return -1;
}

afs_int32
rxk5_InitProperties()
{
    char *propStr, *p;
    afs_int32 propSize;
    struct rxk5_prop_arg arg[1], *ap;
    
    osi_Log0(afsd_logp, "rxk5_InitProperties called\n"); 	/* XXX */
    QInit(&arg->q);

    krb5i_iterate_enctypes(afs_prop_rxk5_helper, arg);

    propSize = 0;
    for (ap = (void*)QNext(&arg->q); ap != arg; ap = (void*)QNext(&ap->q)) {
	propSize += 1 + strlen(ap->number);
    }
    propStr = afs_osi_Alloc(propSize);
    p = propStr;
    while ((ap = (void*)QNext(&arg->q)) != arg) {
	QRemove(&ap->q);
	if (propStr != p) *p++ = ' ';
	strcpy(p, ap->number);
	p += strlen(p);
	afs_osi_Free(ap, sizeof *ap);
    }
    afs_AddProperty("rxk5.enctypes", propStr);

    osi_Free(propStr, propSize);
    return 0;
}
#endif
