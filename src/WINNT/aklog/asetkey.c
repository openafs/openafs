/*
 * $Id$
 *
 * asetkey - Manipulates an AFS KeyFile
 *
 * Updated for Kerberos 5
 */
/*
 * Copyright (c) 2007 Secure Endpoints Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Neither the name of the Secure Endpoints Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <winsock.h>

#include <stdio.h>
#include <sys/types.h>
#include <krb5.h>

#include <afs/stds.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#ifndef PRE_AFS35
#include <afs/dirpath.h>
#endif /* !PRE_AFS35 */
#include <afs/com_err.h>

void
validate_krb5_availability(void)
{
#ifndef _WIN64
#define KRB5LIB "krb5_32.dll"
#else
#define KRB5LIB "krb5_64.dll"
#endif
    HINSTANCE h = LoadLibrary(KRB5LIB);
    if (h) 
        FreeLibrary(h);
    else {
        fprintf(stderr, "Kerberos for Windows library %s is not available.\n", KRB5LIB);
        exit(2);
    }
}

int
main(int argc, char **argv)
{
    struct afsconf_dir *tdir;
    register long code;
    const char *confdir;

    validate_krb5_availability();

    if (argc == 1) {
	printf("asetkey: usage is 'setkey <opcode> options, e.g.\n");
	printf("    asetkey add <kvno> <keyfile> <princ>\n");
	printf("    asetkey delete <kvno>\n");
	printf("    asetkey list\n");
	exit(1);
    }

#ifdef PRE_AFS35
    confdir = AFSCONF_SERVERNAME;
#else /* PRE_AFS35 */
    confdir = AFSDIR_SERVER_ETC_DIRPATH;
#endif /* PRE_AFS35 */

    tdir = afsconf_Open(confdir);
    if (!tdir) {
	printf("asetkey: can't initialize conf dir '%s'\n", confdir);
	exit(1);
    }
    if (strcmp(argv[1], "add")==0) {
	krb5_context context;
	krb5_principal principal;
	krb5_keyblock *key;
	krb5_error_code retval;
	int kvno;

	if (argc != 5) {
	    printf("asetkey add: usage is 'asetkey add <kvno> <keyfile> <princ>\n");
	    exit(1);
	}

	krb5_init_context(&context);

	kvno = atoi(argv[2]);
	retval = krb5_parse_name(context, argv[4], &principal);
	if (retval != 0) {
		afs_com_err(argv[0], retval, "while parsing AFS principal");
		exit(1);
	}
	retval = krb5_kt_read_service_key(context, argv[3], principal, kvno,
					  ENCTYPE_DES_CBC_CRC, &key);
	if (retval != 0) {
		afs_com_err(argv[0], retval, "while extracting AFS service key");
		exit(1);
	}

	if (key->length != 8) {
		printf("Key length should be 8, but is really %d!\n",
		       key->length);
		exit(1);
	}

	code = afsconf_AddKey(tdir, kvno, key->contents, 1);
	if (code) {
	    printf("asetkey: failed to set key, code %d.\n", code);
	    exit(1);
	}
	krb5_free_principal(context, principal);
	krb5_free_keyblock(context, key);
    }
    else if (strcmp(argv[1], "delete")==0) {
	long kvno;
	if (argc != 3) {
	    printf("asetkey delete: usage is 'asetkey delete <kvno>\n");
	    exit(1);
	}
	kvno = atoi(argv[2]);
	code = afsconf_DeleteKey(tdir, kvno);
	if (code) {
	    printf("asetkey: failed to delete key %d, (code %d)\n", kvno, code);
	    exit(1);
	}
    }
    else if (strcmp(argv[1], "list") == 0) {
	struct afsconf_keys tkeys;
	register int i, j;
	
	code = afsconf_GetKeys(tdir, &tkeys);
	if (code) {
	    printf("asetkey: failed to get keys, code %d\n", code);
	    exit(1);
	}
	for(i=0;i<tkeys.nkeys;i++) {
	    if (tkeys.key[i].kvno != -1) {
		printf("kvno %4d: key is: ", tkeys.key[i].kvno);
		for (j = 0; j < 8; j++)
			printf("%02x", (unsigned char) tkeys.key[i].key[j]);
		printf("\n");
	    }
	}
	printf("All done.\n");
    }
    else {
	printf("asetkey: unknown operation '%s', type 'asetkey' for assistance\n", argv[1]);
	exit(1);
    }
    exit(0);
}
