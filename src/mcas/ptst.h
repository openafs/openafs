/******************************************************************************
 * ptst.h
 *
 * Per-thread state management.
 *
 * Copyright (c) 2002-2003, K A Fraser
Copyright (c) 2003, Keir Fraser All rights reserved.

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

#ifndef __PTST_H__
#define __PTST_H__

typedef struct ptst_st ptst_t;

#include "gc.h"
#include "random.h"

struct ptst_st
{
    /* Thread id */
    unsigned int id;

    /* State management */
    ptst_t      *next;
    unsigned int count;
    /* Utility structures */
    gc_t        *gc;
    rand_t       rand;
};

extern pthread_key_t ptst_key;

/*
 * Enter/leave a critical region. A thread gets a state handle for
 * use during critical regions.
 */
ptst_t *critical_enter(void);
#define critical_exit(_p) gc_exit(_p)

/* Iterators */
extern ptst_t *ptst_list;
#define ptst_first()  (ptst_list)
#define ptst_next(_p) ((_p)->next)

/* Called once at start-of-day for entire application. */
void _init_ptst_subsystem(void);

#endif /* __PTST_H__ */
