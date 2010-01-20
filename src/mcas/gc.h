/*
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

#ifndef __GC_H__
#define __GC_H__

typedef struct gc_st gc_t;

/* Most of these functions peek into a per-thread state struct. */
#include "ptst.h"

/* Initialise GC section of given per-thread state structure. */
gc_t *gc_init(void);

int gc_add_allocator(int alloc_size, char *tag);
void gc_remove_allocator(int alloc_id);

/*
 * Memory allocate/free. An unsafe free can be used when an object was
 * not made visible to other processes.
 */
void *gc_alloc(ptst_t *ptst, int alloc_id);
void gc_free(ptst_t *ptst, void *p, int alloc_id);
void gc_unsafe_free(ptst_t *ptst, void *p, int alloc_id);

/*
 * Hook registry. Allows users to hook in their own per-epoch delay
 * lists.
 */
typedef void (*hook_fn_t)(ptst_t *, void *);
int gc_add_hook(hook_fn_t fn);
void gc_remove_hook(int hook_id);
void gc_add_ptr_to_hook_list(ptst_t *ptst, void *ptr, int hook_id);

/* Per-thread entry/exit from critical regions */
void gc_enter(ptst_t *ptst);
void gc_exit(ptst_t *ptst);

/* Start-of-day initialisation of garbage collector. */
void _init_gc_subsystem(void);
void _destroy_gc_subsystem(void);

#endif /* __GC_H__ */
