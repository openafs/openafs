/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#if defined(UKERNEL)
#ifdef HAVE_UNISTD_H
#define __USE_XOPEN
#endif
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/stds.h"
#include "afs/pthread_glock.h"
#include "afs/cellconfig.h"
#include "afs/afsutil.h"
#include "afs/auth.h"
#include "afs/kauth.h"
#include "afs/kautils.h"
#include "afs/pthread_glock.h"
#include "des/des.h"

#else /* defined(UKERNEL) */
#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <crypt.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#define __USE_XOPEN
#include <unistd.h>
#endif
#include <afs/cellconfig.h>
#include <afs/auth.h>
#include <afs/afsutil.h>
#include "kauth.h"
#include "kautils.h"
#endif /* defined(UKERNEL) */


/* This defines the Andrew string_to_key function.  It accepts a password
   string as input and converts it via a one-way encryption algorithm to a DES
   encryption key.  It is compatible with the original Andrew authentication
   service password database. */

static void
Andrew_StringToKey(char *str, char *cell,	/* cell for password */
		   struct ktc_encryptionKey *key)
{
    char password[8 + 1];	/* crypt's limit is 8 chars anyway */
    int i;
    int passlen;

    memset(key, 0, sizeof(struct ktc_encryptionKey));

    strncpy(password, cell, 8);
    passlen = strlen(str);
    if (passlen > 8)
	passlen = 8;

    for (i = 0; i < passlen; i++)
	password[i] ^= str[i];

    for (i = 0; i < 8; i++)
	if (password[i] == '\0')
	    password[i] = 'X';

    /* crypt only considers the first 8 characters of password but for some
     * reason returns eleven characters of result (plus the two salt chars). */
    strncpy((char *)key, (char *)crypt(password, "p1") + 2,
	    sizeof(struct ktc_encryptionKey));

    /* parity is inserted into the LSB so leftshift each byte up one bit.  This
     * allows ascii characters with a zero MSB to retain as much significance
     * as possible. */
    {
	char *keybytes = (char *)key;
	unsigned int temp;

	for (i = 0; i < 8; i++) {
	    temp = (unsigned int)keybytes[i];
	    keybytes[i] = (unsigned char)(temp << 1);
	}
    }
    des_fixup_key_parity(key);
}

static void
StringToKey(char *str, char *cell,	/* cell for password */
	    struct ktc_encryptionKey *key)
{
    des_key_schedule schedule;
    char temp_key[8];
    char ivec[8];
    char password[BUFSIZ];
    int passlen;

    strncpy(password, str, sizeof(password));
    if ((passlen = strlen(password)) < sizeof(password) - 1)
	strncat(password, cell, sizeof(password) - passlen);
    if ((passlen = strlen(password)) > sizeof(password))
	passlen = sizeof(password);

    memcpy(ivec, "kerberos", 8);
    memcpy(temp_key, "kerberos", 8);
    des_fixup_key_parity(temp_key);
    des_key_sched(temp_key, schedule);
    des_cbc_cksum(password, ivec, passlen, schedule, ivec);

    memcpy(temp_key, ivec, 8);
    des_fixup_key_parity(temp_key);
    des_key_sched(temp_key, schedule);
    des_cbc_cksum(password, key, passlen, schedule, ivec);

    des_fixup_key_parity(key);
}

void
ka_StringToKey(char *str, char *cell,	/* cell for password */
	       struct ktc_encryptionKey *key)
{
    char realm[MAXKTCREALMLEN];
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code = ka_CellToRealm(cell, realm, 0 /*local */ );
    if (code)			/* just take his word for it */
	strncpy(realm, cell, sizeof(realm));
    else			/* for backward compatibility */
	lcstring(realm, realm, sizeof(realm));
    if (strlen(str) > 8)
	StringToKey(str, realm, key);
    else
	Andrew_StringToKey(str, realm, key);
    UNLOCK_GLOBAL_MUTEX;
}

/* This prints out a prompt and reads a string from the terminal, turning off
   echoing.  If verify is requested it requests that the string be entered
   again and the two strings are compared.  The string is then converted to a
   DES encryption key. */

/* Errors:
     KAREADPW - some error returned from read_pw_string
 */

afs_int32
ka_ReadPassword(char *prompt, int verify, char *cell,
		struct ktc_encryptionKey *key)
{
    char password[BUFSIZ];
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    memset(key, 0, sizeof(struct ktc_encryptionKey));
    code = read_pw_string(password, sizeof(password), prompt, verify);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return KAREADPW;
    }
    if (strlen(password) == 0) {
	UNLOCK_GLOBAL_MUTEX;
	return KANULLPASSWORD;
    }
    ka_StringToKey(password, cell, key);
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* This performs the backslash quoting defined by AC_ParseLoginName. */

static char
map_char(str, ip)
     char *str;
     int *ip;
{
    char c = str[*ip];
    if (c == '\\') {
	c = str[++(*ip)];
	if ((c >= '0') && (c <= '7')) {
	    c = c - '0';
	    c = (c * 8) + (str[++(*ip)] - '0');
	    c = (c * 8) + (str[++(*ip)] - '0');
	}
    }
    return c;
}

/* This routine parses a string that might be entered by a user from the
   terminal.  It defines a syntax to allow a user to specify his identity in
   terms of his name, instance and cell with a single string.  These three
   output strings must be allocated by the caller to their maximum length.  The
   syntax is very simple: the first dot ('.') separates the name from the
   instance and the first atsign ('@') begins the cell name.  A backslash ('\')
   can be used to quote these special characters.  A backslash followed by an
   octal digit (zero through seven) introduces a three digit octal number which
   is interpreted as the ascii value of a single character. */

/* Errors:
     KABADARGUMENT - if no output parameters are specified.
     KABADNAME - if a component of the user name is too long or if a cell was
       specified but the cell parameter was null.
 */

afs_int32
ka_ParseLoginName(char *login, char name[MAXKTCNAMELEN],
		  char inst[MAXKTCNAMELEN], char cell[MAXKTCREALMLEN])
{
    int login_len = strlen(login);
    char rc, c;
    int i, j;
#define READNAME 1
#define READINST 2
#define READCELL 3
    int reading;

    if (!name)
	return KABADARGUMENT;
    strcpy(name, "");
    if (inst)
	strcpy(inst, "");
    if (cell)
	strcpy(cell, "");
    reading = READNAME;
    i = 0;
    j = 0;
    while (i < login_len) {
	rc = login[i];
	c = map_char(login, &i);
	switch (reading) {
	case READNAME:
	    if (rc == '@') {
		name[j] = 0;	/* finish name */
		reading = READCELL;	/* but instance is null */
		j = 0;
		break;
	    }
	    if (inst && (rc == '.')) {
		name[j] = 0;	/* finish name */
		reading = READINST;
		j = 0;
		break;
	    }
	    if (j >= MAXKTCNAMELEN - 1)
		return KABADNAME;
	    name[j++] = c;
	    break;
	case READINST:
	    if (!inst)
		return KABADNAME;
	    if (rc == '@') {
		inst[j] = 0;	/* finish name */
		reading = READCELL;
		j = 0;
		break;
	    }
	    if (j >= MAXKTCNAMELEN - 1)
		return KABADNAME;
	    inst[j++] = c;
	    break;
	case READCELL:
	    if (!cell)
		return KABADNAME;
	    if (j >= MAXKTCREALMLEN - 1)
		return KABADNAME;
	    cell[j++] = c;
	    break;
	}
	i++;
    }
    if (reading == READNAME)
	name[j] = 0;
    else if (reading == READINST) {
	if (inst)
	    inst[j] = 0;
	else
	    return KABADNAME;
    } else if (reading == READCELL) {
	if (cell)
	    cell[j] = 0;
	else
	    return KABADNAME;
    }

    /* the cell is really an authDomain and therefore is really a realm */
    if (cell)
	ucstring(cell, cell, MAXKTCREALMLEN);
    return 0;
}

/* Client side applications should call this to initialize error tables and
   connect to the correct CellServDB file. */

afs_int32
ka_Init(int flags)
{				/* reserved for future use. */
    afs_int32 code;
    static int inited = 0;

    LOCK_GLOBAL_MUTEX;
    if (inited) {
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }
    inited++;
    initialize_U_error_table();
    initialize_KA_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    code = ka_CellConfig(AFSDIR_CLIENT_ETC_DIRPATH);
    UNLOCK_GLOBAL_MUTEX;
    if (code)
	return code;
    return 0;
}

#ifdef MAIN
int
main(void)
{
    struct ktc_encryptionKey key;
    int code;
    char line[256];
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];

    printf("Enter login:");
    fgets(line, 255, stdin);
    ka_ParseLoginName(line, name, instance, cell);
    printf("'%s' '%s' '%s'\n", name, instance, cell);

}
#endif /* MAIN */
