/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
\*******************************************************************/

struct TM_Elem {
    struct TM_Elem *Next;	/* filled by package */
    struct TM_Elem *Prev;	/* filled by package */
    struct timeval TotalTime;	/* filled in by caller -- modified by package */
    struct timeval TimeLeft;	/* filled by package */
    char *BackPointer;		/* filled by caller, not interpreted by package */
};

void openafs_insque(struct TM_Elem *elementp, struct TM_Elem *quep);
void openafs_remque(struct TM_Elem *elementp);
extern int TM_eql(struct timeval *t1, struct timeval *t2);

#ifndef _TIMER_IMPL_
#define Tm_Insert(list, elem) openafs_insque(list, elem)
#define TM_Remove(list, elem) openafs_remque(elem)
extern int TM_Rescan(struct TM_Elem *tlist);
void TM_Insert(struct TM_Elem *tlistPtr, struct TM_Elem *elem);
extern struct TM_Elem *TM_GetExpired(struct TM_Elem *tlist);
extern struct TM_Elem *TM_GetEarliest(struct TM_Elem *tlist);
#endif

extern int TM_Init(struct TM_Elem **list);
extern int TM_Final(struct TM_Elem **list);

#define FOR_ALL_ELTS(var, list, body)\
	{\
	    struct TM_Elem *_LIST_, *var, *_NEXT_;\
	    _LIST_ = (list);\
	    for (var = _LIST_ -> Next; var != _LIST_; var = _NEXT_) {\
		_NEXT_ = var -> Next;\
		body\
	    }\
	}
