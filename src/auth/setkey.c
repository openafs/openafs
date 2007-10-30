/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsreg.h>
#include <WINNT/afsevent.h>
#endif
#include <string.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include "cellconfig.h"
#include "keys.h"
#include <afs/afsutil.h>

#include "AFS_component_version_number.c"
static int char2hex(char c);
static int hex2char(char c);

int
main(argc, argv)
     int argc;
     char **argv;
{
    struct afsconf_dir *tdir;
    register afs_int32 code;
    int i;
    char *cp;

    if (argc == 1) {
	printf("setkey: usage is 'setkey <opcode> options, e.g.\n");
	printf("    setkey add <kvno> <key>\n");
	printf("      note: <key> should be an 8byte hex representation \n");
	printf("            Ex: setkey add 0 \"80b6a7cd7a9dadb6\"\n");
	printf("    setkey delete <kvno>\n");
	printf("    setkey list\n");
	exit(1);
    }

    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	printf("setkey: can't initialize conf dir '%s'\n",
	       AFSDIR_SERVER_ETC_DIRPATH);
	exit(1);
    }
    if (strcmp(argv[1], "add") == 0) {
	char tkey[8];
	if (argc != 4) {
	    printf("setkey add: usage is 'setkey add <kvno> <key>\n");
	    exit(1);
	}
	if (strlen(argv[3]) != 16) {
	    printf("key %s is not in right format\n", argv[3]);
	    printf(" <key> should be an 8byte hex representation \n");
	    printf("  Ex: setkey add 0 \"80b6a7cd7a9dadb6\"\n");
	    exit(1);
	}
	memset(tkey, 0, sizeof(tkey));
	for (i = 7, cp = argv[3] + 15; i >= 0; i--, cp -= 2)
	    tkey[i] = char2hex(*cp) + char2hex(*(cp - 1)) * 16;

	code = afsconf_AddKey(tdir, atoi(argv[2]), tkey, 1);
	if (code) {
	    printf("setkey: failed to set key, code %d.\n", (int)code);
	    exit(1);
	}
    } else if (strcmp(argv[1], "delete") == 0) {
	afs_int32 kvno;
	if (argc != 3) {
	    printf("setkey delete: usage is 'setkey delete <kvno>\n");
	    exit(1);
	}
	kvno = atoi(argv[2]);
	code = afsconf_DeleteKey(tdir, kvno);
	if (code) {
	    printf("setkey: failed to delete key %d, (code %d)\n", (int)kvno,
		   (int)code);
	    exit(1);
	}
    } else if (strcmp(argv[1], "list") == 0) {
	struct afsconf_keys tkeys;
	register int i;
	char tbuffer[9];

	code = afsconf_GetKeys(tdir, &tkeys);
	if (code) {
	    printf("setkey: failed to get keys, code %d\n", (int)code);
	    exit(1);
	}
	for (i = 0; i < tkeys.nkeys; i++) {
	    if (tkeys.key[i].kvno != -1) {
		char hexbuf[17];
		unsigned char c;
		int j;
		memcpy(tbuffer, tkeys.key[i].key, 8);
		tbuffer[8] = 0;
		for (j = 0; j < 8; j++) {
		    c = tbuffer[j];
		    hexbuf[j * 2] = hex2char(c / 16);
		    hexbuf[j * 2 + 1] = hex2char(c % 16);
		}
		hexbuf[16] = '\0';
		printf("kvno %4d: key is '%s' (0x%s)\n",
		       (int)tkeys.key[i].kvno, tbuffer, hexbuf);
	    }
	}
	printf("All done.\n");
    } else {
	printf
	    ("setkey: unknown operation '%s', type 'setkey' for assistance\n",
	     argv[1]);
	exit(1);
    }
    exit(0);
}

static int
char2hex(char c)
{
    if (c >= '0' && c <= '9')
	return (c - 48);
    if ((c >= 'a') && (c <= 'f'))
	return (c - 'a' + 10);

    if ((c >= 'A') && (c <= 'F'))
	return (c - 'A' + 10);

    return -1;
}

static int
hex2char(char c)
{
    if (c <= 9)
	return (c + 48);

    return (c - 10 + 'a');
}
