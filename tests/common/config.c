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
 * Common functions for building a configuration directory
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>
#include <afs/opr.h>

#include <afs/cellconfig.h>
#include <afs/rfc3961.h>

#include <hcrypto/des.h>

#include <tests/tap/basic.h>
#include "common.h"

#define OPAQUE(x) { (sizeof(x) - 1), (x) }

static FILE *
openConfigFile(char *dirname, char *filename) {
    char *path = NULL;
    FILE *file;

    path = afstest_asprintf("%s/%s", dirname, filename);

    file = fopen(path, "w");
    free(path);
    return file;
}

static int
add_test_keys(struct afsconf_dir *dir, struct afstest_configinfo *info)
{
    static const int subtype_aes128 = ETYPE_AES128_CTS_HMAC_SHA1_96;
    static const int subtype_aes256 = ETYPE_AES256_CTS_HMAC_SHA1_96;

    struct {
	int kvno;
	afsconf_keyType ktype;
	int subtype;
	struct rx_opaque keydata;
    } *testkey, test_key_list[] = {
	{1, afsconf_rxkad, 0, OPAQUE("\x19\x16\xfe\xe6\xba\x76\x2f\xfd") },

	{6, afsconf_rxkad_krb5, subtype_aes128,
	 OPAQUE("\x95\xc1\x9d\x84\xea\x16\x87\x14\xad\x23\x90\x7b\xe4\xfc\xc0\xe8") },
	{6, afsconf_rxkad_krb5, subtype_aes256,
	 OPAQUE("\x92\xc2\x59\x58\xe4\x6e\x70\x37\x66\x05\x26\x22\x1f\xef\x31\xc5"
		"\x7b\xff\x5b\x73\xa9\xe2\x75\xd3\x3c\x3f\x57\x64\x14\xe5\x2f\x63") },

	{0}
    };
    int code;
    int overwrite = 1;

    for (testkey = test_key_list; testkey->kvno != 0; testkey++) {
	struct afsconf_typedKey *key;

	if (info->onlydeskeys && testkey->ktype != afsconf_rxkad) {
	    continue;
	}

	key = afsconf_typedKey_new(testkey->ktype, testkey->kvno,
				   testkey->subtype, &testkey->keydata);
	opr_Assert(key != NULL);

	code = afsconf_AddTypedKey(dir, key, overwrite);
	afsconf_typedKey_put(&key);
	if (code != 0) {
	    return code;
	}
    }

    return 0;
}

/*!
 * Build a test configuration directory, containing a CellServDB and ThisCell
 * file for the "example.org" cell. Also populates the KeyFile unless
 * info->skipkeys is set.
 *
 * @param[in] info  Various details for how to create the config dir. If NULL,
 *		    use a default zeroed struct.
 * @return
 * The path to the configuration directory. This should be freed by the caller
 * using free()
 */
char *
afstest_BuildTestConfig(struct afstest_configinfo *info)
{
    char *dir = NULL;
    FILE *file;
    struct afsconf_dir *confdir = NULL;
    struct afstest_configinfo info_defaults;
    struct in_addr iaddr;
    int code;

    memset(&info_defaults, 0, sizeof(info_defaults));
    memset(&iaddr, 0, sizeof(iaddr));

    if (info == NULL) {
	info = &info_defaults;
    }

    dir = afstest_mkdtemp();
    if (dir == NULL) {
	goto error;
    }

    /* Work out which IP address to use in our CellServDB. We figure this out
     * according to the IP address which ubik is most likely to pick for one of
     * our db servers */
    iaddr.s_addr = afstest_MyHostAddr();

    file = openConfigFile(dir, "CellServDB");
    fprintf(file, ">example.org # An example cell\n");
    fprintf(file, "%s #test.example.org\n", inet_ntoa(iaddr));
    fclose(file);

    /* Create a ThisCell file */
    file = openConfigFile(dir, "ThisCell");
    fprintf(file, "example.org");
    fclose(file);

    if (!info->skipkeys) {
	confdir = afsconf_Open(dir);
	if (confdir == NULL) {
	    goto error;
	}

	code = add_test_keys(confdir, info);
	if (code != 0) {
	    goto error;
	}

	afsconf_Close(confdir);
	confdir = NULL;
    }

    return dir;

 error:
    if (confdir != NULL) {
	afsconf_Close(confdir);
    }
    if (dir != NULL) {
	afstest_rmdtemp(dir);
	free(dir);
    }
    return NULL;
}
