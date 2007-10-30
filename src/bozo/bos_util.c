/* 
 *  Copyright (C) 1989 by the Massachusetts Institute of Technology
 * 
 *    Export of software employing encryption from the United States of
 *    America is assumed to require a specific license from the United
 *    States Government.  It is the responsibility of any person or
 *    organization contemplating export to obtain such a license before
 *    exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include <afs/stds.h>
#include <afs/afsutil.h>
#include <rx/rxkad.h>
#include <afs/keys.h>
#include <afs/cellconfig.h>

int
main(int argc, char **argv)
{
    struct afsconf_dir *tdir;
    register afs_int32 code;

    if (argc == 1) {
	printf("bos_util: usage is 'bos_util <opcode> options, e.g.\n");
	printf("    bos_util add <kvno>\n");
	printf("    bos_util adddes <kvno>\n");
#ifdef KERBEROS
	printf("    bos_util srvtab2keyfile <kvno> <keyfile> <princ>\n");
#endif
	printf("    bos_util delete <kvno>\n");
	printf("    bos_util list\n");
	exit(1);
    }

    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIR);
    if (!tdir) {
	printf("bos_util: can't initialize conf dir '%s'\n",
	       AFSDIR_SERVER_ETC_DIR);
	exit(1);
    }
    if (strcmp(argv[1], "add") == 0) {
	struct ktc_encryptionKey tkey;
	int kvno;
	char buf[BUFSIZ], ver[BUFSIZ];
	char *tcell = NULL;

	if (argc != 3) {
	    printf("bos_util add: usage is 'bos_util add <kvno>\n");
	    exit(1);
	}
	kvno = atoi(argv[2]);
	memset(&tkey, 0, sizeof(struct ktc_encryptionKey));

	/* prompt for key */
	code = des_read_pw_string(buf, sizeof(buf), "input key: ", 0);
	if (code || strlen(buf) == 0) {
	    printf("Bad key: \n");
	    exit(1);
	}
	code = des_read_pw_string(ver, sizeof(ver), "Retype input key: ", 0);
	if (code || strlen(ver) == 0) {
	    printf("Bad key: \n");
	    exit(1);
	}
	if (strcmp(ver, buf) != 0) {
	    printf("\nInput key mismatch\n");
	    exit(1);
	}
	ka_StringToKey(buf, tcell, &tkey);
	code = afsconf_AddKey(tdir, kvno, &tkey, 0);
	if (code) {
	    printf("bos_util: failed to set key, code %d.\n", code);
	    exit(1);
	}
    } else if (strcmp(argv[1], "adddes") == 0) {
	struct ktc_encryptionKey tkey;
	int kvno;
	register afs_int32 code;
	char buf[BUFSIZ], ver[BUFSIZ];
	char *tcell = NULL;

	if (argc != 3) {
	    printf("bos_util adddes: usage is 'bos_util adddes <kvno>\n");
	    exit(1);
	}
	kvno = atoi(argv[2]);
	memset(&tkey, 0, sizeof(struct ktc_encryptionKey));

	/* prompt for key */
	code = des_read_pw_string(buf, sizeof(buf), "input key: ", 0);
	if (code || strlen(buf) == 0) {
	    printf("Bad key: \n");
	    exit(1);
	}
	code = des_read_pw_string(ver, sizeof(ver), "Retype input key: ", 0);
	if (code || strlen(ver) == 0) {
	    printf("Bad key: \n");
	    exit(1);
	}
	if (strcmp(ver, buf) != 0) {
	    printf("\nInput key mismatch\n");
	    exit(1);
	}
	des_string_to_key(buf, &tkey);
	code = afsconf_AddKey(tdir, kvno, &tkey, 0);
	if (code) {
	    printf("bos_util: failed to set key, code %d.\n", code);
	    exit(1);
	}
    }
#ifdef KERBEROS
    else if (strcmp(argv[1], "srvtab2keyfile") == 0) {
	char tkey[8], name[255], inst[255], realm[255];
	int kvno;
	if (argc != 5) {
	    printf
		("bos_util add: usage is 'bos_util srvtab2keyfile <kvno> <keyfile> <princ>\n");
	    exit(1);
	}
	kvno = atoi(argv[2]);
	bzero(tkey, sizeof(tkey));
	code = kname_parse(name, inst, realm, argv[4]);
	if (code != 0) {
	    printf("Invalid kerberos name\n");
	    exit(1);
	}
	code = read_service_key(name, inst, realm, kvno, argv[3], tkey);
	if (code != 0) {
	    printf("Can't find key in %s\n", argv[3]);
	    exit(1);
	}
	code = afsconf_AddKey(tdir, kvno, tkey, 0);
	if (code) {
	    printf("bos_util: failed to set key, code %d.\n", code);
	    exit(1);
	}
    }
#endif
    else if (strcmp(argv[1], "delete") == 0) {
	long kvno;
	if (argc != 3) {
	    printf("bos_util delete: usage is 'bos_util delete <kvno>\n");
	    exit(1);
	}
	kvno = atoi(argv[2]);
	code = afsconf_DeleteKey(tdir, kvno);
	if (code) {
	    printf("bos_util: failed to delete key %d, (code %d)\n", kvno,
		   code);
	    exit(1);
	}
    } else if (strcmp(argv[1], "list") == 0) {
	struct afsconf_keys tkeys;
	register int i;
	unsigned char tbuffer[9];

	code = afsconf_GetKeys(tdir, &tkeys);
	if (code) {
	    printf("bos_util: failed to get keys, code %d\n", code);
	    exit(1);
	}
	for (i = 0; i < tkeys.nkeys; i++) {
	    if (tkeys.key[i].kvno != -1) {
		int count;
		unsigned char x[8];
		memcpy(tbuffer, tkeys.key[i].key, 8);
		tbuffer[8] = 0;
		printf("kvno %4d: key is '%s' '", tkeys.key[i].kvno, tbuffer);
		strcpy(x, (char *)tbuffer);
		for (count = 0; count < 8; count++)
		    printf("\\%03o", x[count]);
		printf("'\n");
	    }
	}
	printf("All done.\n");
    } else {
	printf
	    ("bos_util: unknown operation '%s', type 'bos_util' for assistance\n",
	     argv[1]);
	exit(1);
    }
    exit(0);
}
