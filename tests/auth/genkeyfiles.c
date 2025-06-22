/*
 * Copyright (c) 2025 Sine Nomine Associates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/opr.h>
#include <afs/rfc3961.h>

#include <roken.h>

#include <tests/tap/basic.h>

#include "common.h"

/*
 * genkeyfiles - A simple program to generate a KeyFile and KeyFileExt (in the
 * current directory) to be used by the test suite.
 *
 * We don't generate these files every time we run the test suite; instead, the
 * generated files are part of the source tree, to make sure we don't
 * accidentally change the format of these files. The test auth/genkeyfiles-t
 * makes sure that this program generates the same KeyFile/KeyFileExt files in
 * the tree.
 */

static void
copyfile(struct afsconf_dir *dir, const char *src, const char *dest)
{
    static const int bufsize = 4096;

    int src_fd, dest_fd;
    char *buf;
    char *src_path;
    ssize_t len;

    src_path = afstest_asprintf("%s/%s", dir->name, src);

    src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
	err(1, "Cannot read %s", src_path);
    }

    dest_fd = open(dest, O_WRONLY | O_CREAT, 0644);
    if (dest_fd < 0) {
	err(1, "Cannot create %s", dest);
    }

    buf = bmalloc(bufsize);
    for (;;) {
	len = read(src_fd, buf, bufsize);
	if (len == 0) {
	    break;
	}
	if (len < 0) {
	    errx(1, "Error reading from %s", src_path);
	}
	if (write(dest_fd, buf, len) != len) {
	    errx(1, "Error writing to %s", dest);
	}
    }

    if (close(src_fd) < 0) {
	err(1, "Error closing %s", src_path);
    }
    if (close(dest_fd) < 0) {
	err(1, "Error closing %s", dest);
    }

    warnx("Copied %s -> %s", src_path, dest);

    free(buf);
    free(src_path);
}

#define SUBTYPE_AES128 ETYPE_AES128_CTS_HMAC_SHA1_96
#define SUBTYPE_AES256 ETYPE_AES256_CTS_HMAC_SHA1_96

#define OPAQUE(x) { (sizeof(x) - 1), (x) }

static struct {
    int kvno;
    afsconf_keyType ktype;
    int subtype;
    struct rx_opaque keydata;
} *testkey, test_key_list[] = {
    {1, afsconf_rxkad, 0, OPAQUE("\x01\x02\x04\x08\x10\x20\x40\x80") },
    {2, afsconf_rxkad, 0, OPAQUE("\x04\x04\x04\x04\x04\x04\x04\x04") },
    {4, afsconf_rxkad, 0, OPAQUE("\x19\x16\xfe\xe6\xba\x77\x2f\xfd") },

    {5, afsconf_rxkad_krb5, SUBTYPE_AES128,
     OPAQUE("\x4a\x75\x4f\x90\xeb\x12\xc4\x3f\xcd\x18\x67\xe1\xe4\x4d\xf4\x0c") },

    {6, afsconf_rxkad_krb5, SUBTYPE_AES128,
     OPAQUE("\x95\xc1\x9d\x84\xea\x16\x87\x14\xad\x23\x90\x7b\xe4\xfc\xc0\xe8") },
    {6, afsconf_rxkad_krb5, SUBTYPE_AES256,
     OPAQUE("\x92\xc2\x59\x58\xe4\x6e\x70\x37\x66\x05\x26\x22\x1f\xef\x31\xc5"
	    "\x7b\xff\x5b\x73\xa9\xe2\x75\xd3\x3c\x3f\x57\x64\x14\xe5\x2f\x63") },

    {0}
};

int
main(int argc, char **argv)
{
    struct afsconf_dir *dir;
    char *dirname;
    struct afsconf_typedKey *key;
    struct afstest_configinfo bct;
    int overwrite = 1;

    memset(&bct, 0, sizeof(bct));

    bct.skipkeys = 1;

    dirname = afstest_BuildTestConfig(&bct);
    if (dirname == NULL) {
	errx(1, "Cannot create config dir");
    }

    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	errx(1, "Cannot open config dir");
    }

    for (testkey = test_key_list; testkey->kvno != 0; testkey++) {
	key = afsconf_typedKey_new(testkey->ktype, testkey->kvno,
				   testkey->subtype, &testkey->keydata);
	opr_Assert(key != NULL);

	opr_Verify(afsconf_AddTypedKey(dir, key, overwrite) == 0);
	afsconf_typedKey_put(&key);
    }

    copyfile(dir, "KeyFile", "KeyFile");
    copyfile(dir, "KeyFileExt", "KeyFileExt");

    opr_Verify(afsconf_Close(dir) == 0);

    afstest_rmdtemp(dirname);

    return 0;
}
