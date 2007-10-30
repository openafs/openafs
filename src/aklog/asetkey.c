/*
 * $Id$
 *
 * asetkey - Manipulates an AFS KeyFile
 *
 * Updated for Kerberos 5
 */

#include <afsconfig.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */
#include <string.h>

#include <afs/stds.h>
#include <krb5.h>

#include <afs/com_err.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/dirpath.h>

#ifdef HAVE_KRB5_CREDS_KEYBLOCK
#define USING_MIT 1
#endif
#ifdef HAVE_KRB5_CREDS_SESSION
#define USING_HEIMDAL 1
#endif

int
main(int argc, char *argv[])
{
    struct afsconf_dir *tdir;
    register long code;
    const char *confdir;

    if (argc == 1) {
	fprintf(stderr, "%s: usage is '%s <opcode> options, e.g.\n",
		argv[0], argv[0]);
	fprintf(stderr, "\t%s add <kvno> <keyfile> <princ>\n", argv[0]);
	fprintf(stderr, "\t%s delete <kvno>\n", argv[0]);
	fprintf(stderr, "\t%s list\n", argv[0]);
	exit(1);
    }

    confdir = AFSDIR_SERVER_ETC_DIRPATH;

    tdir = afsconf_Open(confdir);
    if (!tdir) {
	fprintf(stderr, "%s: can't initialize conf dir '%s'\n", argv[0],
		confdir);
	exit(1);
    }
    if (strcmp(argv[1], "add")==0) {
	krb5_context context;
	krb5_principal principal;
	krb5_keyblock *key;
	krb5_error_code retval;
	int kvno;

	if (argc != 5) {
	    fprintf(stderr, "%s add: usage is '%s add <kvno> <keyfile> "
		    "<princ>\n", argv[0], argv[0]);
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

#ifdef USING_HEIMDAL
#define deref_key_length(key) \
	  key->keyvalue.length

#define deref_key_contents(key) \
	key->keyvalue.data
#else
#define deref_key_length(key) \
	  key->length

#define deref_key_contents(key) \
	key->contents
#endif
	if (deref_key_length(key) != 8) {
		fprintf(stderr, "Key length should be 8, but is really %d!\n",
			deref_key_length(key));
		exit(1);
	}

	code = afsconf_AddKey(tdir, kvno, (char *) deref_key_contents(key), 1);
	if (code) {
	    fprintf(stderr, "%s: failed to set key, code %d.\n", argv[0], code);
	    exit(1);
	}
	krb5_free_principal(context, principal);
	krb5_free_keyblock(context, key);
    }
    else if (strcmp(argv[1], "delete")==0) {
	long kvno;
	if (argc != 3) {
	    fprintf(stderr, "%s delete: usage is '%s delete <kvno>\n",
		    argv[0], argv[0]);
	    exit(1);
	}
	kvno = atoi(argv[2]);
	code = afsconf_DeleteKey(tdir, kvno);
	if (code) {
	    fprintf(stderr, "%s: failed to delete key %d, (code %d)\n",
		    argv[0], kvno, code);
	    exit(1);
	}
    }
    else if (strcmp(argv[1], "list") == 0) {
	struct afsconf_keys tkeys;
	register int i, j;
	
	code = afsconf_GetKeys(tdir, &tkeys);
	if (code) {
	    fprintf(stderr, "%s: failed to get keys, code %d\n", argv[0], code);
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
	fprintf(stderr, "%s: unknown operation '%s', type '%s' for "
		"assistance\n", argv[0], argv[1], argv[0]);
	exit(1);
    }
    exit(0);
}
