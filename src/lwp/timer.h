
#if !defined(lint) && !defined(LOCORE) && defined(RCS_HDRS)
#endif

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
\*******************************************************************/

struct TM_Elem {
    struct TM_Elem	*Next;		/* filled by package */
    struct TM_Elem	*Prev;		/* filled by package */
    struct timeval	TotalTime;	/* filled in by caller -- modified by package */
    struct timeval	TimeLeft;	/* filled by package */
    char		*BackPointer;	/* filled by caller, not interpreted by package */
};

#if defined(AFS_HPUX_ENV) || defined(AFS_NT40_ENV)
extern void insque(struct TM_Elem *elementp, struct TM_Elem *quep);
extern void remque(struct TM_Elem *elementp);
extern int TM_eql(struct timeval *t1, struct timeval *t2);
#endif
#ifndef _TIMER_IMPL_
#define Tm_Insert(list, elem) insque(list, elem)
#define TM_Remove(list, elem) remque(elem)
extern int TM_Rescan();
void TM_Insert();
extern struct TM_Elem *TM_GetExpired();
extern struct TM_Elem *TM_GetEarliest();
#endif

extern int TM_Final();

#define FOR_ALL_ELTS(var, list, body)\
	{\
	    register struct TM_Elem *_LIST_, *var, *_NEXT_;\
	    _LIST_ = (list);\
	    for (var = _LIST_ -> Next; var != _LIST_; var = _NEXT_) {\
		_NEXT_ = var -> Next;\
		body\
	    }\
	}
