/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <opr/uuid.h>

#include <tests/tap/basic.h>

int
main(int argc, char **argv)
{
    opr_uuid_t uuidA = {{0x4F, 0x44, 0x94, 0x47, 0x76, 0xBA, 0x47, 0x2C,
                         0x97, 0x1A, 0x86, 0x6B, 0xC0, 0x10, 0x1A, 0x4B}};
    opr_uuid_t uuidB = {{0x5D, 0x2A, 0x39, 0x36, 0x94, 0xB2, 0x48, 0x90,
                         0xA8, 0xD2, 0x7F, 0xBC, 0x1B, 0x29, 0xDA, 0x9B}};
    opr_uuid_t uuidC;
    char *str;
    int version;
    struct opr_uuid_unpacked raw;

    plan(16);

    memset(&uuidC, 0, sizeof(opr_uuid_t));

    ok(opr_uuid_isNil(&uuidC), "opr_uuid_isNil(nilUuid) works");
    ok(!opr_uuid_isNil(&uuidA), "opr_uuid_isNil(uuid) works");

    ok(opr_uuid_equal(&uuidA, &uuidA), "opr_uuid_equal(A, A) works");
    ok(!opr_uuid_equal(&uuidA, &uuidB), "opr_uuid_equal(A, B) works");
    ok(!opr_uuid_equal(&uuidA, &uuidC), "opr_uuid_equal(A, nilUid) works");

    is_int(1187447773, opr_uuid_hash(&uuidA), "opr_uuid_hash(A) works");
    is_int(1251907497, opr_uuid_hash(&uuidB), "opr_uuid_hash(B) works");

    opr_uuid_toString(&uuidA, &str);
    is_string("4f449447-76ba-472c-971a-866bc0101a4b", str,
	      "opr_uuid_toString(uuidA) works");
    opr_uuid_freeString(str);

    is_int(0, opr_uuid_fromString(&uuidC, "4F449447-76BA-472C-971A-866BC0101A4B"),
	   "opr_uuid_fromString works with conventional UUID");
    ok(opr_uuid_equal(&uuidA, &uuidC), " ... and UUID is correct");

    memset(&uuidC, 0, sizeof(opr_uuid_t));
    is_int(0, opr_uuid_fromString(&uuidC, "4F449447-76BA-472C-97-1A-866BC0101A4B"),
	   "opr_uuid_fromString works with AFS style UUID");
    ok(opr_uuid_equal(&uuidA, &uuidC), " ... and UUID is correct");

    memset(&uuidC, 0, sizeof(opr_uuid_t));
    opr_uuid_create(&uuidC);
    ok(!opr_uuid_isNil(&uuidC), "opr_uuid_create makes non-nil UUID");
    is_int(0x80, uuidC.data[8] & 0x80, "variant is DCE as expected");
    version = (uuidC.data[6] & 0xF0) >> 4;
    ok(version >=1 && version <=5, "version %d is in expected range", version);

    opr_uuid_unpack(&uuidB, &raw);
    opr_uuid_pack(&uuidC, &raw);
    ok(opr_uuid_equal(&uuidB, &uuidC),
	"opr_uuid_pack(opr_uuid_unpack()) works as expected");

    return 0;
}
