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

#include <afsconfig.h>
#include <afs/param.h>

#include <tests/tap/basic.h>
#include <opr/dict.h>

struct myentry {
    int value;
    struct opr_queue d;
};

int
find(struct opr_dict *dict, int value, struct myentry **entryp)
{
    struct myentry *ent;
    struct opr_queue *cursor;
    int length = 0;

    for (opr_dict_ScanBucket(dict, value, cursor)) {
	ent = opr_queue_Entry(cursor, struct myentry, d);
	length++;

	if (ent->value == value) {
	   if (entryp)
		*entryp = ent;
	   return length;
	}
    }
    return 0;
}

int
main(void)
{
    struct opr_dict *dict;
    struct myentry *entry;
    int members[] = {1,2,3,4,5,6,7,8,9,10,17,0};
    int i;

    plan(10);

    ok(opr_dict_Init(3) == NULL,
       "Initialising a dictionary with a bad size fails");

    dict = opr_dict_Init(8);
    ok(dict != NULL,
       "Initialising a dictionary succeeds");

    for (i = 0; members[i] !=0; i++) {
	entry = malloc(sizeof(struct myentry));
	entry->value = members[i];
	opr_dict_Append(dict, entry->value, &entry->d);
    }
    ok(1, "Hash populated successfully");

    is_int(1, find(dict, 1, NULL),
	   "Entry 1 is first in hash chain");
    is_int(2, find(dict, 9, NULL),
	   "Entry 9 is second in hash chain");
    is_int(3, find(dict, 17, NULL),
	   "Entry 17 is third in hash chain");
    is_int(1, find(dict, 2, NULL),
	   "Entry 2 is first in hash chain");
    is_int(1, find(dict, 8, NULL),
	   "Entry 8 is first in hash chain");

    find(dict, 17, &entry);
    ok(entry != NULL && entry->value == 17, "Retrieved entry 17");
    opr_dict_Promote(dict, entry->value, &entry->d);
    is_int(1, find(dict, 17, NULL),
	   "Entry 17 is first in hash chain following promotion");

    return 0;
}
