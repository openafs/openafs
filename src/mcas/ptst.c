/******************************************************************************
 * ptst.c
 *
 * Per-thread state management. Essentially the state management parts
 * of MB's garbage-collection code have been pulled out and placed here,
 * for the use of other utility routines.
 *
 * Copyright (c) 2002-2003, K A Fraser

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
    * notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    * copyright notice, this list of conditions and the following
    * disclaimer in the documentation and/or other materials provided
    * with the distribution.  Neither the name of the Keir Fraser
    * nor the names of its contributors may be used to endorse or
    * promote products derived from this software without specific
    * prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portable_defns.h"
#include "ptst.h"


pthread_key_t ptst_key;
ptst_t *ptst_list;

static unsigned int next_id;

ptst_t *critical_enter(void)
{
    ptst_t *ptst, *next, *new_next;
    unsigned int id, oid;

    ptst = (ptst_t *)pthread_getspecific(ptst_key);
    if ( ptst == NULL )
    {
        for ( ptst = ptst_first(); ptst != NULL; ptst = ptst_next(ptst) )
        {
            if ( (ptst->count == 0) && (CASIO(&ptst->count, 0, 1) == 0) )
            {
                break;
            }
        }

        if ( ptst == NULL )
        {
            ptst = ALIGNED_ALLOC(sizeof(*ptst));
            if ( ptst == NULL ) exit(1);
            memset(ptst, 0, sizeof(*ptst));
            ptst->gc = gc_init();
            rand_init(ptst);
            ptst->count = 1;
            id = next_id;
            while ( (oid = CASIO(&next_id, id, id+1)) != id ) id = oid;
            ptst->id = id;
            new_next = ptst_list;
            do {
                ptst->next = next = new_next;
                WMB_NEAR_CAS();
            }
            while ( (new_next = CASPO(&ptst_list, next, ptst)) != next );
        }

        pthread_setspecific(ptst_key, ptst);
    }

    gc_enter(ptst);
    return(ptst);
}


static void ptst_destructor(ptst_t *ptst)
{
    ptst->count = 0;
}


void _init_ptst_subsystem(void)
{
    ptst_list = NULL;
    next_id   = 0;
    WMB();
    if ( pthread_key_create(&ptst_key, (void (*)(void *))ptst_destructor) )
    {
        exit(1);
    }
}
