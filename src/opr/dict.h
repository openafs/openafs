/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <opr/queue.h>

struct opr_dict {
    int size;
    struct opr_queue *table;
};

static_inline void
opr_dict_Prepend(struct opr_dict *dict, int index, struct opr_queue *entry)
{
    opr_queue_Prepend(&dict->table[index & (dict->size-1)], entry);
}

static_inline void
opr_dict_Append(struct opr_dict *dict, int index, struct opr_queue *entry)
{
    opr_queue_Append(&dict->table[index & (dict->size-1)], entry);
}

static_inline void
opr_dict_Promote(struct opr_dict *dict, int index, struct opr_queue *entry)
{
    opr_queue_Remove(entry);
    opr_dict_Prepend(dict, index, entry);
}

#define opr_dict_ScanBucket(dict, index, cursor) \
	opr_queue_Scan(&(dict)->table[index & ((dict)->size-1)], cursor)

#define opr_dict_ScanBucketSafe(dict, index, cursor, store) \
	opr_queue_ScanSafe(&(dict)->table[index & ((dict)->size-1)], \
			   cursor, store)

extern struct opr_dict *opr_dict_Init(unsigned int size);
extern void opr_dict_Free(struct opr_dict **dict);
