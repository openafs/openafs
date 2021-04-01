/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
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

/*!
 * Test those bits of the authcon interface that we can test without involving
 * the cache manager.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/authcon.h>
#include <afs/cellconfig.h>

#include <tests/tap/basic.h>
#include "common.h"

int
main(int argc, char **argv)
{
    struct afsconf_dir *dir;
    char *dirname;
    struct rx_securityClass **classes;
    struct rx_securityClass *secClass;
    int secIndex;
    int numClasses;
    struct afsconf_typedKey *key;
    int code = 0;
    struct afsconf_bsso_info bsso;
    struct afstest_configinfo bct;

    memset(&bsso, 0, sizeof(bsso));
    memset(&bct, 0, sizeof(bct));

    afstest_SkipTestsIfBadHostname();

    plan(8);

    bct.skipkeys = 1;
    dirname = afstest_BuildTestConfig(&bct);

    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to configure directory.\n");
	code = 1;
	goto out;
    }

    rx_Init(0);

    /* Server Security objects */

    bsso.dir = dir;
    afsconf_BuildServerSecurityObjects_int(&bsso, &classes, &numClasses);
    is_int(5, numClasses, "5 security classes are returned, as expected");
    ok(classes[1] == NULL, "The rxvab class is undefined, as requested");
    free(classes);

    /* Up to date checks */

    ok(afsconf_UpToDate(dir), "Newly opened directory is up to date");
    is_int(0, afsconf_AddKey(dir,
			     1, "\x19\x16\xfe\xe6\xba\x77\x2f\xfd", 0),
	   "Adding key worked");
    ok(!afsconf_UpToDate(dir), "Directory with newly added key isn't");
    afsconf_ClientAuth(dir, &secClass, &secIndex);
    ok(afsconf_UpToDate(dir), "afsconf_ClientAuth() resets UpToDate check");
    afsconf_DeleteKey(dir, 1);
    ok(!afsconf_UpToDate(dir), "Directory with newly deleted key isn't");
    afsconf_GetLatestKeyByTypes(dir, afsconf_rxkad, 0, &key);
    ok(afsconf_UpToDate(dir), "afsconf_GetLatestKeyByTypes resest UpToDate");

out:
    afstest_rmdtemp(dirname);
    return code;
}
