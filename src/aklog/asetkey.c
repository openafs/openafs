/*
 * $Id$
 *
 * asetkey - Manipulates an AFS KeyFile
 *
 * Updated for Kerberos 5
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>

#define KERBEROS_APPLE_DEPRECATED(x)
#include <krb5.h>

#ifndef HAVE_KERBEROSV_HEIM_ERR_H
#include <afs/com_err.h>
#endif
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/dirpath.h>

#ifdef HAVE_KRB5_CREDS_KEYBLOCK
#define USING_MIT 1
#endif
#ifdef HAVE_KRB5_CREDS_SESSION
#define USING_HEIMDAL 1
#endif

static int
stringToType(const char *string) {
    if (strcmp(string, "rxkad") == 0)
	return afsconf_rxkad;
    if (strcmp(string, "rxkad_krb5") == 0)
	return afsconf_rxkad_krb5;
    if (strcmp(string, "rxgk") == 0)
	return afsconf_rxgk;

    return atoi(string);
}

static void
printKey(const struct rx_opaque *key)
{
    int i;

    for (i = 0; i < key->len; i++)
	printf("%02x", ((unsigned char *)key->val)[i]);
    printf("\n");
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

static struct afsconf_typedKey *
keyFromCommandLine(afsconf_keyType type, int kvno, int subType,
	           const char *string, size_t length)
{
    struct rx_opaque key;
    struct afsconf_typedKey *typedKey;
    const char *cp;
    int i;

    if (strlen(string) != 2*length) {
	printf("key %s is not in right format\n", string);
	printf(" <key> should be an %d byte hex representation \n", (int) length);
	exit(1);
    }

    rx_opaque_alloc(&key, length);
    cp = string;
    for (i = 0; i< length; i++) {
       ((char *)key.val)[i] = char2hex(*cp) * 16 + char2hex(*(cp+1));
       cp+=2;
    }

    typedKey = afsconf_typedKey_new(type, kvno, subType, &key);
    rx_opaque_freeContents(&key);
    return typedKey;
}

#ifdef USING_HEIMDAL
#define deref_key_length(key)			\
	    key->keyvalue.length

#define deref_key_contents(key)			\
	    key->keyvalue.data
#else
#define deref_key_length(key)			\
	    key->length

#define deref_key_contents(key)			\
	    key->contents
#endif

static struct afsconf_typedKey *
keyFromKeytab(int kvno, afsconf_keyType type, int subtype, const char *keytab, const char *princ)
{
    int retval;
    krb5_principal principal;
    krb5_keyblock *key;
    krb5_context context;
    struct rx_opaque buffer;
    struct afsconf_typedKey *typedKey;

    krb5_init_context(&context);

    retval = krb5_parse_name(context, princ, &principal);
    if (retval) {
	afs_com_err("asetkey", retval, "while parsing AFS principal");
	exit(1);
    }

    if (type == afsconf_rxkad) {
	retval = krb5_kt_read_service_key(context, (char *)keytab, principal,
					  kvno, ENCTYPE_DES_CBC_CRC, &key);
	if (retval == KRB5_KT_NOTFOUND)
	    retval = krb5_kt_read_service_key(context, (char *)keytab,
					      principal, kvno,
					      ENCTYPE_DES_CBC_MD5, &key);
	if (retval == KRB5_KT_NOTFOUND)
	    retval = krb5_kt_read_service_key(context, (char *)keytab,
					      principal, kvno,
					      ENCTYPE_DES_CBC_MD4, &key);
    } else if (type == afsconf_rxkad_krb5 || type == afsconf_rxgk) {
	retval = krb5_kt_read_service_key(context, (char *)keytab, principal,
					  kvno, subtype, &key);
    } else {
	retval=AFSCONF_BADKEY;
    }
    if (retval == KRB5_KT_NOTFOUND) {
	char * princname = NULL;

	krb5_unparse_name(context, principal, &princname);

	if (type == afsconf_rxkad) {
	    afs_com_err("asetkey", retval,
			"for keytab entry with Principal %s, kvno %u, "
			"DES-CBC-CRC/MD5/MD4",
			princname ? princname : princ, kvno);
	} else {
	    afs_com_err("asetkey", retval,
			"for keytab entry with Principal %s, kvno %u",
			princname ? princname : princ, kvno);
	}
	exit(1);
    }

    if (retval != 0) {
	afs_com_err("asetkey", retval, "while extracting AFS service key");
	exit(1);
    }

    if (type == afsconf_rxkad && deref_key_length(key) != 8) {
	fprintf(stderr, "Key length should be 8, but is really %u!\n",
		(unsigned int)deref_key_length(key));
	exit(1);
    }

    rx_opaque_populate(&buffer, deref_key_contents(key), deref_key_length(key));

    typedKey = afsconf_typedKey_new(type, kvno, subtype, &buffer);
    rx_opaque_freeContents(&buffer);
    krb5_free_principal(context, principal);
    krb5_free_keyblock(context, key);
    return typedKey;
}

static void
addKey(struct afsconf_dir *dir, int argc, char **argv) {
    struct afsconf_typedKey *typedKey;
    int type;
    int kvno;
    int code;

    switch (argc) {
      case 4:
	typedKey = keyFromCommandLine(afsconf_rxkad, atoi(argv[2]), 0,
				      argv[3], 8);
	break;
      case 5:
	typedKey = keyFromKeytab(atoi(argv[2]), afsconf_rxkad, 0, argv[3], argv[4]);
	break;
      case 6:
	type = stringToType(argv[2]);
	kvno = atoi(argv[3]);
	if (type == afsconf_rxkad) {
	    typedKey = keyFromCommandLine(afsconf_rxkad, kvno, 0, argv[5], 8);
	} else if (type == afsconf_rxgk || type == afsconf_rxkad_krb5) {
	    typedKey = keyFromCommandLine(type, kvno, atoi(argv[4]), argv[5], strlen(argv[5])/2);
	} else {
	    fprintf(stderr, "Unknown key type %s\n", argv[2]);
	    exit(1);
	}
	break;
      case 7:
	type = stringToType(argv[2]);
	kvno = atoi(argv[3]);
	if (type == afsconf_rxkad || type == afsconf_rxkad_krb5 || type == afsconf_rxgk) {
	    typedKey = keyFromKeytab(kvno, type, atoi(argv[4]), argv[5],
				     argv[6]);
	} else {
	    fprintf(stderr, "Unknown key type %s\n", argv[2]);
	    exit(1);
	}
	break;
      default:
	fprintf(stderr, "%s add: usage is '%s add <kvno> <keyfile> "
			"<princ>\n", argv[0], argv[0]);
	fprintf(stderr, "\tOR\n\t%s add <kvno> <key>\n", argv[0]);
	fprintf(stderr, "\tOR\n\t%s add <type> <kvno> <subtype> <key>\n",
		argv[0]);
	fprintf(stderr, "\tOR\n\t%s add <type> <kvno> <subtype> <keyfile> <princ>\n",
	        argv[0]);
	fprintf(stderr, "\t\tEx: %s add 0 \"80b6a7cd7a9dadb6\"\n", argv[0]);
		exit(1);
    }
    code = afsconf_AddTypedKey(dir, typedKey, 1);
    afsconf_typedKey_put(&typedKey);
    if (code) {
	afs_com_err("asetkey", code, "while adding new key");
	exit(1);
    }
}

static void
deleteKey(struct afsconf_dir *dir, int argc, char **argv)
{
    int type;
    int subtype;
    int kvno;
    int code;

    switch (argc) {
    case 3:
	kvno = atoi(argv[2]);
	code = afsconf_DeleteKey(dir, kvno);
	if (code) {
	    afs_com_err(argv[0], code, "while deleting key %d", kvno);
	    exit(1);
	}
	printf("Deleted rxkad key %d\n", kvno);
	break;

    case 4:
	type = stringToType(argv[2]);
	kvno = atoi(argv[3]);
	code = afsconf_DeleteKeyByType(dir, type, kvno);
	if (code) {
	    afs_com_err(argv[0], code, "while deleting key (type %d kvno %d)",
			type, kvno);
	    exit(1);
	}
	printf("Deleted key (type %d kvno %d)\n", type, kvno);
	break;

    case 5:
	type = stringToType(argv[2]);
	kvno = atoi(argv[3]);
	subtype = atoi(argv[4]);
	code = afsconf_DeleteKeyBySubType(dir, type, kvno, subtype);
	if (code) {
	    afs_com_err(argv[0], code, "while deleting key (type %d kvno %d subtype %d)\n",
			type, kvno, subtype);
	    exit(1);
	}
	printf("Deleted key (type %d kvno %d subtype %d)\n", type, kvno, subtype);
	break;

    default:
	fprintf(stderr, "%s delete: usage is '%s delete <kvno>\n",
		argv[0], argv[0]);
	fprintf(stderr, "\tOR\n\t%s delete <type> <kvno>\n", argv[0]);
	fprintf(stderr, "\tOR\n\t%s delete <type> <kvno> <subtype>\n", argv[0]);
	exit(1);
    }
}

static void
listKey(struct afsconf_dir *dir, int argc, char **argv)
{
    struct afsconf_typedKeyList *keys;
    int i;
    int code;

    code = afsconf_GetAllKeys(dir, &keys);
    if (code) {
	afs_com_err("asetkey", code, "while retrieving keys");
	exit(1);
    }
    for (i = 0; i < keys->nkeys; i++) {
	afsconf_keyType type;
	int kvno;
	int minorType;
	struct rx_opaque *keyMaterial;

	afsconf_typedKey_values(keys->keys[i], &type, &kvno, &minorType,
				    &keyMaterial);
	switch(type) {
	  case afsconf_rxkad:
	    if (kvno != -1) {
		printf("rxkad\tkvno %4d: key is: ", kvno);
		printKey(keyMaterial);
	    }
	    break;
	  case afsconf_rxkad_krb5:
	    if (kvno != -1) {
		printf("rxkad_krb5\tkvno %4d enctype %d; key is: ",
		       kvno, minorType);
		printKey(keyMaterial);
	    }
	    break;
	  case afsconf_rxgk:
	    if (kvno != -1) {
		printf("rxgk\tkvno %4d enctype %d; key is: ",
		       kvno, minorType);
		printKey(keyMaterial);
	    }
	    break;
	  default:
	    printf("unknown(%d)\tkvno %4d subtype %d; key is: ", type,
	           kvno, minorType);
	    printKey(keyMaterial);
	    break;
	  }
    }
    printf("All done.\n");
}

int
main(int argc, char *argv[])
{
    struct afsconf_dir *tdir;
    const char *confdir;

    if (argc == 1) {
	fprintf(stderr, "%s: usage is '%s <opcode> options, e.g.\n",
		argv[0], argv[0]);
	fprintf(stderr, "\t%s add <kvno> <keyfile> <princ>\n", argv[0]);
	fprintf(stderr, "\tOR\n\t%s add <kvno> <key>\n", argv[0]);
	fprintf(stderr, "\tOR\n\t%s add <type> <kvno> <subtype> <key>\n",
	        argv[0]);
	fprintf(stderr, "\tOR\n\t%s add <type> <kvno> <subtype> <keyfile> <princ>\n",
	        argv[0]);
	fprintf(stderr, "\t\tEx: %s add 0 \"80b6a7cd7a9dadb6\"\n", argv[0]);
	fprintf(stderr, "\t%s delete <kvno>\n", argv[0]);
	fprintf(stderr, "\t%s delete <type> <kvno>\n", argv[0]);
	fprintf(stderr, "\t%s delete <type> <kvno> <subtype>\n", argv[0]);
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
	addKey(tdir, argc, argv);
    }
    else if (strcmp(argv[1], "delete")==0) {
	deleteKey(tdir, argc, argv);
    }
    else if (strcmp(argv[1], "list") == 0) {
	listKey(tdir, argc, argv);

    }
    else {
	fprintf(stderr, "%s: unknown operation '%s', type '%s' for "
		"assistance\n", argv[0], argv[1], argv[0]);
	exit(1);
    }
    exit(0);
}
