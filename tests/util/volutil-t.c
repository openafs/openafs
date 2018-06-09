/*
 * Copyright 2018, Sine Nomine Associates and others.
 * All Rights Reserved.
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

#include <afs/afsutil.h>
#include <tests/tap/basic.h>

char *out_of_range_names[] = {
    "/vicepiv",
    "/vicepzz",
    "iv",
    "zz",
    "255",
    "256",
    NULL,
};

char *invalid_names[] = {
    "",
    "/vice",
    "/vicep",
    "/vicepbogus",
    "/vicep0",
    "foo",
    "!?",
    "-1",
    NULL,
};

void
test_partition_name_to_id(void)
{
    char x, y;
    int id;
    char part[32];
    char **t;

    /*
     * Test conversion of XX to partition id,
     * where XX is "a".."z", "aa"..."iu"
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "%c", x);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "%c%c", x, y);
            is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /*
     * Test conversion of /vicepXX to partition id,
     * where XX is "a".."z", "aa"..."iu".
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "/vicep%c", x);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "/vicep%c%c", x,y );
            is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /*
     * Test conversion of vicepXX to partition id,
     * where XX is "a".."z", "aa"..."iu".
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "vicep%c", x);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "vicep%c%c", x,y );
            is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /*
     * Test numeric names in the range "0".."254" are passed through.
     */
    for (id = 0; id <= 254; id++) {
        sprintf(part, "%d", id);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }

    /*
     * Test out of range names.
     */
    for (t = out_of_range_names; *t != NULL; t++) {
        is_int(-1, volutil_GetPartitionID(*t), "\"%s\" is out of range", *t);
    }

    /*
     * Test invalid names.
     */
    for (t = invalid_names; *t != NULL; t++) {
        is_int(-1, volutil_GetPartitionID(*t), "\"%s\" is invalid", *t);
    }
}

void
test_partition_id_to_name(void)
{
    char x, y;
    int id;
    char part[32];

    /*
     * Test conversion of ids to partition names,
     * for ids of 0 to 254.
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "/vicep%c", x);
        is_string(part, volutil_PartitionName(id), "%d is \"%s\"", id, part);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "/vicep%c%c", x,y );
            is_string(part, volutil_PartitionName(id), "%d is \"%s\"", id, part);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /* Test out of range values. */
    is_string("BAD VOLUME ID", volutil_PartitionName(-1), "-1 is invalid");
    is_string("BAD VOLUME ID", volutil_PartitionName(255), "255 is invalid");
    is_string("BAD VOLUME ID", volutil_PartitionName(256), "256 is invalid");

    /* Test buffer too short (thread safe variant of volutil_PartitionName). */
    is_string("SPC", volutil_PartitionName_r(0, part, 4), "buffer too short");
}

int
main(int argc, char **argv)
{
    plan(1293);
    test_partition_name_to_id();
    test_partition_id_to_name();
    return 0;
}
