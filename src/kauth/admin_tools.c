/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* These routines provide administrative tools for managing the AuthServer.
   There is an interactive routine that can be used to examine the database and
   make small changes as well as subroutines to permit specialized programs to
   update the database, change the server passwords, etc. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <afs/debug.h>
#include <ctype.h>
#include <string.h>

    /* These two needed for rxgen output to work */
#include <sys/types.h>
#include <rx/xdr.h>

#include <stdio.h>
#include <rx/rx.h>
#include <lock.h>
#include <ubik.h>
#ifndef AFS_NT40_ENV
#include <pwd.h>
#endif
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <afs/com_err.h>
#include <afs/afsutil.h>

#include "kauth.h"
#include "kautils.h"
#include "kaport.h"

#define CMD_PARSER_AMBIG_FIX 1	/* allow ambiguous aliases */

#define	KA_SIXHOURS	(6*3600)

static struct ubik_client *conn;
static char cell[MAXKTCREALMLEN] = "";
static char whoami[32];
static char passwd[BUFSIZ];
static char myName[510];	/* almost like whoami save with path and without : */

static int finished;
static int zero_argc;
static char **zero_argv;
afs_uint32 ka_islocked();

afs_int32
DefaultCell(void)
{
    afs_int32 code;

    if (cell[0] != 0)
	return 0;
    code = ka_ExpandCell(0, cell, 0 /*local */ );
    if (code) {
	com_err(whoami, code, "Can't expand cell name");
    }
    return code;
}

/* These are the command operation procedures. */

int
DumpUser(char *user, char *arock, int showadmin, int showkey, char *inst)
{
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    Date now = time(0);
    int code;
    char bob[KA_TIMESTR_LEN];

    struct kaentryinfo tentry;

    code = ka_ParseLoginName(user, name, instance, 0);
    if (code) {
	com_err(whoami, code, "parsing user's name '%s'", user);
	return KABADCMD;
    }

    if (!inst)
	inst = instance;
    code =
	ubik_Call(KAM_GetEntry, conn, 0, name, inst, KAMAJORVERSION, &tentry);
    if (code) {
	com_err(whoami, code, "getting information for %s.%s", name, inst);
	return code;
    }
    if (tentry.minor_version != KAMINORVERSION)
	printf("Minor version number mismatch: got %d, expected %d\n",
	       tentry.minor_version, KAMINORVERSION);
    if (showadmin && !(tentry.flags & KAFADMIN))
	return 0;
    ka_PrintUserID("\nUser data for ", name, inst, "");
    {
	char *prefix = " (";
#define NEWPREFIX "+"
	if (tentry.flags & KAFADMIN) {
	    printf("%sADMIN", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry.flags & KAFNOTGS) {
	    printf("%sNOTGS", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry.flags & KAFNOCPW) {
	    printf("%sNOCPW", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry.flags & KAFNOSEAL) {
	    printf("%sNOSEAL", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry.flags & KAFNEWASSOC) {
	    printf("%sNEWASSOC", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry.flags & KAFASSOCROOT) {
	    printf("%sASSOCROOT", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry.flags & KAFASSOC) {
	    printf("%sASSOC", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry.user_expiration <= now) {
	    printf("%sexpired", prefix);
	    prefix = NEWPREFIX;
	}
	if (strcmp(prefix, NEWPREFIX) == 0)
	    printf(")\n");
	else
	    printf("\n");
    }
    if ((!ka_KeyIsZero((char *)&tentry.key, sizeof(tentry.key))) && (showkey)) {
	printf("  key (%d):", tentry.key_version);
	ka_PrintBytes((char *)&tentry.key, sizeof(tentry.key));
    } else {
	if (tentry.keyCheckSum == 0)
	    printf("  key version is %d", tentry.key_version);
	else
	    printf("  key (%d) cksum is %u", tentry.key_version,
		   tentry.keyCheckSum);
    }
    ka_timestr(tentry.change_password_time, bob, KA_TIMESTR_LEN);
    printf(", last cpw: %s\n", bob);
    if (!tentry.misc_auth_bytes) {
	printf("  password will never expire.\n");
	printf
	    ("  An unlimited number of unsuccessful authentications is permitted.\n");
    } else {
	unsigned char misc_stuff[4];
	afs_uint32 temp;

	temp = tentry.misc_auth_bytes;
/*
      temp = ntohl(tentry.misc_auth_bytes);
*/
	unpack_long(temp, misc_stuff);

	if (!misc_stuff[0]) {
	    printf("  password will never expire.\n");
	} else {
	    ka_timestr((tentry.change_password_time +
			misc_stuff[0] * 24 * 60 * 60), bob, KA_TIMESTR_LEN);
	    printf("  password will expire: %s\n", bob);
	}

	if (!misc_stuff[2])
	    printf
		("  An unlimited number of unsuccessful authentications is permitted.\n");
	else {
	    printf
		("  %d consecutive unsuccessful authentications are permitted.\n",
		 misc_stuff[2]);

	    if (!misc_stuff[3])
		printf("  The lock time for this user is not limited.\n");
	    else
		printf("  The lock time for this user is %4.1f minutes.\n",
		       (float)((unsigned int)misc_stuff[3] << 9) / 60.0);

	    if (!(misc_stuff[1] & KA_ISLOCKED)
		|| !ka_islocked(name, instance, &temp))
		printf("  User is not locked.\n");
	    else if (temp == (afs_uint32) (-1L))
		printf("  User is locked forever.\n");
	    else {
		ka_timestr(temp, bob, KA_TIMESTR_LEN);
		printf("  User is locked until %s\n", bob);
	    }
	}

    }
    {
	char exp[KA_TIMESTR_LEN];
	ka_timestr(tentry.user_expiration, exp, KA_TIMESTR_LEN);
	if (tentry.user_expiration < now)
	    printf("  DISABLED entry at %s.", exp);
	else if (tentry.user_expiration == NEVERDATE)
	    printf("  entry never expires.");
	else
	    printf("  entry expires on %s.", exp);
    }
    printf("  Max ticket lifetime %.2f hours.\n",
	   tentry.max_ticket_lifetime / 3600.0);
    ka_timestr(tentry.modification_time, bob, KA_TIMESTR_LEN);
    printf("  last mod on %s by ", bob);
    ka_PrintUserID("", tentry.modification_user.name,
		   tentry.modification_user.instance, "\n");
    if ((tentry.reserved3 & 0xffff0000) == 0x12340000) {
	int short reused = (short)tentry.reserved3;
	if (!reused) {
	    printf("  permit password reuse\n");
	} else {
	    printf("  don't permit password reuse\n");
	}
    }
    return 0;
}

int
ListUsers(struct cmd_syndesc *as, char *arock)
{
    struct kaident name;
    afs_int32 index;
    afs_int32 count;
    afs_int32 next_index;
    int code, all = 0, showa = 0;
    int showkey = (as->parms[2].items != NULL);

    if (as->parms[0].items)
	all = 1;
    if (as->parms[1].items) {
	all = 1;
	showa = 1;
    }
    for (index = 0; 1; index = next_index) {
	code =
	    ubik_Call(KAM_ListEntry, conn, 0, index, &next_index, &count,
		      &name);
	if (code) {
	    com_err(whoami, code, "calling KAM_ListEntry");
	    break;
	}
	if (!next_index)
	    break;
	if (next_index < 0)
	    printf("next_index (%d) is negative: ", next_index);
	if (strlen(name.name) == 0)
	    printf("name is zero length: ");
	if (all)
	    DumpUser(name.name, NULL, showa, showkey, name.instance);
	else
	    ka_PrintUserID("", name.name, name.instance, "\n");
    }
    return code;
}


int
ExamineUser(struct cmd_syndesc *as, char *arock)
{
    int showkey = (as->parms[1].items != NULL);
    return DumpUser(as->parms[0].items->data, arock, 0, showkey, NULL);
}


struct OKerrors {
    int code;
    char *msg;
};

int
handle_errors(int code,		/* error code to handle */
	      struct OKerrors OKlist[],	/* list of errors & messages that should be ignored */
	      int *persist)
{				/* set this if we should retry, clear otherwise */
    int i;

    for (i = 0; OKlist[i].code; i++) {
	if (OKlist[i].code == code) {
	    printf("%s\n", OKlist[i].msg);
	    *persist = 0;	/* we're done */
	    return 0;
	}
    }

    printf(" : [%s] %s", error_table_name(code), error_message(code));
    switch (code) {
    case UNOQUORUM:
	printf(", wait one second\n");
	IOMGR_Sleep(1);
	return 0;
    case KAEMPTY:
    case RX_CALL_TIMEOUT:
	printf(" (retrying)\n");
	return 0;
    }
    printf("\n");

    *persist = 0;		/* don't retry these errors */
    return code;
}

int
CreateUser(struct cmd_syndesc *as, char *arock)
{
    int code;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    struct ktc_encryptionKey key;

    int persist = 1;
    struct OKerrors OKlist[2];
    OKlist[0].code = 0;

    code = ka_ParseLoginName(as->parms[0].items->data, name, instance, 0);
    if (code) {
	com_err(whoami, code, "parsing user's name '%s'",
		as->parms[0].items->data);
	return KABADCMD;
    }
    ka_StringToKey(as->parms[1].items->data, cell, &key);

    do {
	code = ubik_Call(KAM_CreateUser, conn, 0, name, instance, key);
	if (!code)
	    return 0;
	ka_PrintUserID("Creating user ", name, instance, " ");
	code = handle_errors(code, OKlist, &persist);
    } while (persist);
    return code;
}

int
DeleteUser(struct cmd_syndesc *as, char *arock)
{
    int code;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];

    int persist = 1;
    struct OKerrors OKlist[2];
    OKlist[0].code = 0;
    code = ka_ParseLoginName(as->parms[0].items->data, name, instance, 0);
    if (code) {
	com_err(whoami, code, "parsing user's name '%s'",
		as->parms[0].items->data);
	return KABADCMD;
    }

    do {
	code = ubik_Call(KAM_DeleteUser, conn, 0, name, instance);
	if (!code)
	    return 0;
	ka_PrintUserID("Deleting user ", name, instance, " ");
	code = handle_errors(code, OKlist, &persist);
    } while (persist);
    return code;
}

static int
read_time_interval(char *str, afs_int32 * seconds)
{
    char *s;
    int sec = 0;
    char buf[32];

    str = strncpy(buf, str, sizeof(buf));
    s = strchr(str, ':');
    if (s == 0)
	sec = atoi(str);
    else {
	*s++ = '\0';		/* separate hours and minutes */
	sec = atoi(str) * 3600 + atoi(s) * 60;
    }
    *seconds = sec;
    return 0;
}

int
parse_flags(char *name, char *inst, char *str, afs_int32 * flags)
{
    struct kaentryinfo tentry;
    int code;
    char bitspec[100];
    afs_int32 f;
    char bit[25];
    char c;
    int addop;			/* 1=add bit; 0=remove bit */
    int flag;
    int i;

    str = lcstring(bitspec, str, sizeof(bitspec));
    if (isdigit(*str)) {
	if (strncmp(str, "0x", 2) == 0)	/* 0x => hex */
	    sscanf(str, "0x%lx", &f);
	else if (*str == '0')	/* assume octal */
	    sscanf(str, "%lo", &f);
	else			/* just assume hex */
	    sscanf(str, "%lx", &f);
    } else {
	if (*str == '=') {
	    str++;
	    f = 0;
	    addop = 1;
	} else {
	    if (strchr("+-", *str))
		addop = (*str++ == '+');
	    else if (*str == '_') {
		addop = 0;
		str++;
	    } else
		addop = 1;
	    code =
		ubik_Call(KAM_GetEntry, conn, 0, name, inst, KAMAJORVERSION,
			  &tentry);
	    if (code) {
		com_err(whoami, code,
			"could get current flag value for %s.%s", name, inst);
		return -1;
	    }
	    f = tentry.flags;
	}
	while (*str) {
	    i = 0;
	    while (1) {
		c = *str;
		if (isupper(c))
		    c = tolower(c);
		if (!islower(c))
		    break;
		bit[i++] = c;
		str++;
	    }
	    bit[i] = '\0';
	    if (strcmp(bit, "admin") == 0)
		flag = KAFADMIN;
	    else if (strcmp(bit, "noadmin") == 0)
		flag = KAFADMIN, addop = !addop;
	    else if (strcmp(bit, "notgs") == 0)
		flag = KAFNOTGS;
	    else if (strcmp(bit, "tgs") == 0)
		flag = KAFNOTGS, addop = !addop;
	    else if (strcmp(bit, "noseal") == 0)
		flag = KAFNOSEAL;
	    else if (strcmp(bit, "seal") == 0)
		flag = KAFNOSEAL, addop = !addop;
	    else if (strcmp(bit, "nocpw") == 0)
		flag = KAFNOCPW;
	    else if (strcmp(bit, "cpw") == 0)
		flag = KAFNOCPW, addop = !addop;
	    else if (strcmp(bit, "newassoc") == 0)
		flag = KAFNEWASSOC;
	    else if (strcmp(bit, "nonewassoc") == 0)
		flag = KAFNEWASSOC, addop = !addop;
	    else {
		printf
		    ("Illegal bit name: %s; choices are: [no]admin, [no]tgs, [no]seal, [no]cpw\n",
		     bit);
		return -1;
	    }

	    if (addop)
		f |= flag;
	    else
		f &= ~flag;

	    if (*str == 0)
		break;
	    if (*str == '+')
		addop = 1;	/* get next op */
	    else if ((*str == '-') || (*str == '_'))
		addop = 0;
	    else {
		printf("Illegal combination operator: %c\n", *str);
		return -1;
	    }
	    str++;
	}
    }
    *flags = (f & KAF_SETTABLE_FLAGS) | KAFNORMAL;
    return 0;
}

#define seriouserror(code) ((code <0) || ((code != UNOSERVERS) && (code != UNOQUORUM) && code != UNOTSYNC))

/* return MAXLONG if locked forever */
afs_uint32
ka_islocked(char *name, char *instance, afs_uint32 * when)
{
    int count, code;
    afs_uint32 tempwhen;

    count = 0;
    *when = 0;
    do {
	tempwhen = 0;
	code =
	    ubik_CallIter(KAM_LockStatus, conn, UPUBIKONLY, &count, (long) name,
			  (long) instance, (long) &tempwhen, 0, 0, 0,
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (code) {
	    if (seriouserror(code))
		com_err(whoami, code, "");
	} else if (tempwhen) {	/* user is locked */
	    if (!*when || tempwhen < *when) {
		*when = tempwhen;
		return (*when);
	    }
	} else			/* ! tempwhen ==> user is not locked */
	    return 0;

    } while (code != UNOSERVERS);

    return (*when);
}

int
Unlock(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code, rcode = 0;
    afs_int32 count;
    char *server;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];

    code = ka_ParseLoginName(as->parms[0].items->data, name, instance, 0);
    if (code) {
	com_err(whoami, code, "parsing user's name '%s'",
		as->parms[0].items->data);
	return KABADCMD;
    }

    count = 0;
    do {
	code = ubik_CallIter(KAM_Unlock, conn, 0, &count, (long) name, (long) instance,
			     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (code && (code != UNOSERVERS)) {
	    server = "<unknown>";
	    if (conn && conn->conns[count - 1]
		&& conn->conns[count - 1]->peer) {
		server = rx_AddrStringOf(conn->conns[count - 1]->peer);
	    }
	    com_err(whoami, code,
		    "so %s.%s may still be locked (on server %s)",
		    name, instance, server);

	    if (!rcode) {
		rcode = code;
	    }
	}
    } while (code != UNOSERVERS);

    return rcode;
}

int
SetFields(struct cmd_syndesc *as, char *arock)
{
    int code;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    afs_int32 flags = 0;
    Date expiration = 0;
    afs_int32 lifetime = 0;
    afs_int32 maxAssociates = -1;
    afs_int32 pwexpiry = 0;
    afs_int32 was_spare = 0;
    char misc_auth_bytes[4];
    int i;

    code = ka_ParseLoginName(as->parms[0].items->data, name, instance, 0);
    if (code) {
	com_err(whoami, code, "parsing user's name '%s'",
		as->parms[0].items->data);
	return KABADCMD;
    }

    if (as->parms[1].items) {
	code = parse_flags(name, instance, as->parms[1].items->data, &flags);
	if (code) {
	    printf
		("Illegal flag specification: %s, should be of the form <'='|'+'|'-'|'_'>bitname{<'+'|'-'>bitname}*\n",
		 as->parms[1].items->data);
	    return KABADCMD;
	}
    }
    if (as->parms[2].items) {
	char buf[32];
	char *s = strncpy(buf, as->parms[2].items->data, sizeof(buf));
	code = ktime_DateToInt32(s, &expiration);
	if (code) {
	    printf("Illegal time format %s: %s\n", as->parms[2].items->data,
		   ktime_GetDateUsage());
	    return KABADCMD;
	}
	if (expiration == 0) {
	    fprintf(stderr, "Expiration time must be after (about) 1970.\n");
	    return KABADCMD;
	}
	if (expiration < time(0)) {
	    fprintf(stderr,
		    "Warning: expiration being set into the past, account will be disabled.\n");
	}
    }
    /*
     * TICKET lifetime...
     */
    if (as->parms[3].items) {
	code = read_time_interval(as->parms[3].items->data, &lifetime);
	if (code)
	    return KABADCMD;
    }

    /*  no point in doing this any sooner than necessary */
    for (i = 0; i < 4; misc_auth_bytes[i++] = 0);

    if (as->parms[4].items) {
	if (util_isint(as->parms[4].items->data))
	    pwexpiry = atoi(as->parms[4].items->data);
	else {
	    fprintf(stderr,
		    "Password lifetime specified must be a non-negative decimal integer.\n");
	    pwexpiry = -1;
	}
	if (pwexpiry < 0 || pwexpiry > 254) {
	    fprintf(stderr,
		    "Password lifetime range must be [0..254] days.\n");
	    fprintf(stderr, "Zero represents an unlimited lifetime.\n");
	    return KABADCMD;
	} else {
	    misc_auth_bytes[0] = pwexpiry + 1;
	}
    }

    if (as->parms[5].items) {
	char *reuse;
	reuse = (as->parms[5].items->data);

	if (!strcmp(reuse, "yes")) {
	    misc_auth_bytes[1] = KA_REUSEPW;
	} else if (strcmp(reuse, "no")) {
	    fprintf(stderr,
		    "must specify \"yes\" or \"no\": \"yes\" assumed\n");
	    misc_auth_bytes[1] = KA_REUSEPW;
	} else {
	    misc_auth_bytes[1] = KA_NOREUSEPW;
	}
    }

    if (as->parms[6].items) {
	int nfailures;


	if (util_isint(as->parms[6].items->data)
	    && ((nfailures = atoi(as->parms[6].items->data)) < 255)) {
	    misc_auth_bytes[2] = nfailures + 1;
	} else {
	    fprintf(stderr, "Failure limit must be in [0..254].\n");
	    fprintf(stderr, "Zero represents unlimited login attempts.\n");
	    return KABADCMD;
	}
    }

    if (as->parms[7].items) {
	int locktime, hrs, mins;
	char *s;

	hrs = 0;
	s = as->parms[7].items->data;
	if (strchr(s, ':'))
	    sscanf(s, "%d:%d", &hrs, &mins);
	else
	    sscanf(s, "%d", &mins);

	locktime = hrs * 60 + mins;
	if (hrs < 0 || hrs > 36 || mins < 0) {
	    fprintf(stderr,
		    "Lockout times must be either minutes or hh:mm.\n");
	    fprintf(stderr, "Lockout times must be less than 36 hours.\n");
	    return KABADCMD;
	} else if (locktime > 36 * 60) {
	    fprintf(stderr,
		    "Lockout times must be either minutes or hh:mm.\n");
	    fprintf(stderr, "Lockout times must be less than 36 hours.\n");
	    fprintf(stderr,
		    "Continuing with lock time of exactly 36 hours...\n");
	    locktime = 36 * 60;
	}
	locktime = (locktime * 60 + 511) >> 9;	/* ceil(l*60/512) */
	misc_auth_bytes[3] = locktime + 1;	/* will be 1 if user said 0 */
    }
#if ASSOCIATES
    if (as->parms[8].items) {
	maxAssociates = atoi(as->parms[6].items->data);
	if (maxAssociates < 0) {
	    printf("Illegal maximum number of associates\n");
	    return KABADCMD;
	}
    }
#endif
    was_spare = pack_long(misc_auth_bytes);

    if (was_spare || flags || expiration || lifetime || (maxAssociates >= 0))
	code =
	    ubik_Call(KAM_SetFields, conn, 0, name, instance, flags,
		      expiration, lifetime, maxAssociates, was_spare,
		      /* spare */ 0);
    else {
	printf("Must specify one of the optional parameters\n");
	return KABADCMD;
    }
    if (code)
	com_err(whoami, code, "calling KAM_SetFields for %s.%s", name,
		instance);
    return code;
}

int
StringToKey(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    char realm[MAXKTCREALMLEN];
    struct ktc_encryptionKey key;

    if (as->parms[1].items) {
	code = ka_ExpandCell(as->parms[1].items->data, realm, 0 /*local */ );
	if (code) {
	    com_err(whoami, code,
		    "expanding %s as cell name, attempting to continue",
		    as->parms[1].items->data);
	}
	ucstring(realm, realm, sizeof(realm));
    } else {
	if (code = DefaultCell())
	    return code;
	ucstring(realm, cell, sizeof(realm));
    }
    ka_StringToKey(as->parms[0].items->data, realm, &key);

    printf("Converting %s in realm '%s' yields key='",
	   as->parms[0].items->data, realm);
    ka_PrintBytes((char *)&key, sizeof(key));
    printf("'.\n");

    des_string_to_key(as->parms[0].items->data, &key);

    printf("Converting %s with the DES string to key yields key='",
	   as->parms[0].items->data);
    ka_PrintBytes((char *)&key, sizeof(key));
    printf("'.\n");

    return 0;
}

int
SetPassword(struct cmd_syndesc *as, char *arock)
{
    int code;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char realm[MAXKTCREALMLEN];
    struct ktc_encryptionKey key;
    afs_int32 kvno = 0;

    code = ka_ParseLoginName(as->parms[0].items->data, name, instance, realm);
    if (code) {
	com_err(whoami, code, "parsing user's name '%s'",
		as->parms[0].items->data);
	return KABADCMD;
    }

    if (strlen(realm) == 0)
	ucstring(realm, cell, sizeof(realm));

    if (as->parms[1].items && as->parms[2].items) {
	printf("Can't specify both a password and a key\n");
	return KABADCMD;
    } else if (as->parms[1].items) {
	(void)init_child(myName);
	(void)give_to_child(passwd);	/* old password */
	code = password_bad(as->parms[1].items->data);
	(void)terminate_child();
	if (code)
	    return KABADCMD;
	ka_StringToKey(as->parms[1].items->data, realm, &key);
    } else if (as->parms[2].items) {
	if (ka_ReadBytes(as->parms[2].items->data, (char *)&key, sizeof(key))
	    != 8) {
	    printf("Key must be 8 bytes: '%s' was too long\n",
		   as->parms[2].items->data);
	    return KABADCMD;
	}
    } else {
	printf("Must specify new password or key\n");
	return KABADCMD;
    }


    if (as->parms[3].items)
	sscanf(as->parms[3].items->data, "%d", &kvno);

#if defined(AFS_S390_LINUX20_ENV) && !defined(AFS_S390X_LINUX20_ENV)
    code = ubik_Call(KAM_SetPassword, conn, 0, name, instance, kvno, 0, key);
#else
    code = ubik_Call(KAM_SetPassword, conn, 0, name, instance, kvno, key);
#endif
    if (code)
	com_err(whoami, code, "so can't set password for %s.%s", name,
		instance);
    return code;
}

#define PrintPrincipal(p,n,l) \
    PrintName((p)->name, (p)->instance, (p)->cell, l, n)

static afs_int32
PrintName(char *name, char *inst, char *acell, int buflen, char *buf)
{
    int nlen, len;
    int left;			/* if ConvertBytes stops early */
    afs_int32 code;

    if (name == 0)
	name = "";
    if (inst == 0)
	inst = "";
    if (acell == 0)
	acell = "";
    left = ka_ConvertBytes(buf, buflen, name, strlen(name));
    if (left) {
      bad_name:
	code = KABADNAME;
	com_err(whoami, code, "PrintName: principal name was '%s'.'%s'@'%s'",
		name, inst, acell);
	return code;
    }
    nlen = strlen(buf);
    len = strlen(inst);
    if (len) {
	if (nlen + len + 1 >= buflen)
	    goto bad_name;
	buf[nlen++] = '.';
	left = ka_ConvertBytes(buf + nlen, buflen - nlen, inst, len);
	if (left)
	    goto bad_name;
	nlen += len;
    }

    len = strlen(acell);
    if (len) {
	char *lcell = ka_LocalCell();
	if (lcell == 0)
	    lcell = "";
	if (strcmp(acell, lcell) != 0) {
	    /* only append cell if not the local cell */
	    if (nlen + len + 1 >= buflen)
		goto bad_name;
	    buf[nlen++] = '@';
	    left = ka_ConvertBytes(buf + nlen, buflen - nlen, acell, len);
	    if (left)
		goto bad_name;
	    nlen += len;
	}
    }
    return 0;
}

#define PrintedPrincipal(p) PrintedName ((p)->name, (p)->instance, (p)->cell)

/* PrintedName - returned a pointer to a static string in which the formated
 * name has been stored. */

static char *
PrintedName(char *name, char *inst, char *cell)
{
    static char printedName[128];
    afs_int32 code;
    code = PrintName(name, inst, cell, sizeof(printedName), printedName);
    if (code) {
	if (name == 0)
	    name = "";
	strncpy(printedName, name, sizeof(printedName));
	printedName[sizeof(printedName) - 8] = 0;
	strcat(printedName, "<error>");
    }
    return printedName;
}

static afs_int32
ListTicket(struct ktc_principal *server, int verbose)
{
    afs_int32 code;
    struct ktc_token token;	/* the token we're printing */
    struct ktc_principal client;
    char UserName[sizeof(struct ktc_principal)];
    char ServerName[sizeof(struct ktc_principal)];
    afs_int32 now = time(0);
    char bob[KA_TIMESTR_LEN];

    /* get the ticket info itself */
    code = ktc_GetToken(server, &token, sizeof(token), &client);
    if (code) {
	com_err(whoami, code, "failed to get token info for server %s",
		PrintedPrincipal(server));
	return code;
    }
    code = PrintPrincipal(&client, UserName, sizeof(UserName));
    if (code)
	return code;
    /* spaces are printed as "\040" */
    if (UserName[0] == 0)
	printf("Tokens");
    else if (strncmp(UserName, "AFS\\040ID\\040", 13) == 0) {
	printf("User's (AFS ID %s) tokens", UserName + 13);
    } else if (strncmp(UserName, "Unix\\040UID\\040", 15) == 0) {
	printf("Tokens");
    } else
	printf("User %s's tokens", UserName);

    code = PrintPrincipal(server, ServerName, sizeof(ServerName));
    if (code)
	return code;
    printf(" for %s ", ServerName);

    if (token.startTime > now) {
	ka_timestr(token.startTime, bob, KA_TIMESTR_LEN);
	printf("[>> POSTDATED 'till %s <<]", bob);
    }

    if (token.endTime <= now)
	printf("[>> Expired <<]\n");
    else {
	ka_timestr(token.endTime, bob, KA_TIMESTR_LEN);
	printf("[Expires %s]\n", bob);
    }
    if (verbose) {
	printf("SessionKey: ");
	ka_PrintBytes((char *)&token.sessionKey, sizeof(token.sessionKey));
	printf("\nTicket (kvno = %d, len = %d): ", token.kvno,
	       token.ticketLen);
	ka_PrintBytes((char *)token.ticket, token.ticketLen);
	printf("\n");
    }
    return 0;
}

static
GetTicket(struct cmd_syndesc *as, char *arock)
{
    int code;
    struct ktc_principal server;
    struct ktc_token token;
    afs_int32 life = KA_SIXHOURS;

    if (as->parms[1].items) {
	code = read_time_interval(as->parms[1].items->data, &life);
	if (code)
	    return KABADCMD;
    }
    code =
	ka_ParseLoginName(as->parms[0].items->data, server.name,
			  server.instance, server.cell);
    if (code) {
	com_err(whoami, code, "parsing user's name '%s'",
		as->parms[0].items->data);
	return KABADCMD;
    }
    if (server.cell[0] == 0) {
	if (code = DefaultCell())
	    return code;
	strcpy(server.cell, cell);
    } else {
	code = ka_ExpandCell(server.cell, server.cell, 0 /*local */ );
	if (code) {
	    com_err(whoami, code, "Can't expand cell name");
	    return code;
	}
    }

    token.ticketLen = 0;	/* in case there are no tokens */
    code =
	ka_GetServerToken(server.name, server.instance, server.cell, life,
			  &token, /*new */ 1, /*dosetpag */ 0);
    if (code)
	com_err(whoami, code, "getting ticket for %s",
		PrintedPrincipal(&server));
    else {
	code = ListTicket(&server, /*verbose */ 1);
    }
    return code;
}

static
GetPassword(struct cmd_syndesc *as, char *arock)
{
    int code;
    char name[MAXKTCNAMELEN];
    struct ktc_encryptionKey key;
    static struct ubik_client *lpbkConn = 0;

    /* no instance allowed */
    code = ka_ParseLoginName(as->parms[0].items->data, name, 0, 0);
    if (code) {
      abort:
	com_err(whoami, code,
		"getting %s's password via loopback connection to GetPassword",
		name);
	/* if we got a timeout, print a clarification, too */
	if (code == -1) {
	    fprintf(stderr,
		    "%s: please note that this command must be run locally on a database server machine.\n",
		    whoami);
	}
	return code;
    }
    if (lpbkConn == 0) {
	struct rx_connection *conns[2];
	struct rx_securityClass *sc;
	int si;			/* security class index */

	code = rx_Init(0);
	if (code)
	    goto abort;
	sc = rxnull_NewClientSecurityObject();
	si = RX_SCINDEX_NULL;
	conns[0] =
	    rx_NewConnection(htonl(INADDR_LOOPBACK), htons(AFSCONF_KAUTHPORT),
			     KA_MAINTENANCE_SERVICE, sc, si);
	conns[1] = 0;
	code = ubik_ClientInit(conns, &lpbkConn);
	if (code)
	    goto abort;
    }
    code = ubik_Call(KAM_GetPassword, lpbkConn, 0, name, &key);
    /* Lets close down the ubik_Client connection now */
    ubik_ClientDestroy(lpbkConn);
    if (code)
	goto abort;
    printf("Key: ");
    ka_PrintBytes((char *)&key, sizeof(key));
    printf("\n");
    return code;
}

int
GetRandomKey(struct cmd_syndesc *as, char *arock)
{
    int code;
    struct ktc_encryptionKey key;

    code = ubik_Call(KAM_GetRandomKey, conn, 0, &key);
    if (code)
	com_err(whoami, code, "so can't get random key");
    else {
	int i;
	printf("Key: ");
	ka_PrintBytes((char *)&key, sizeof(key));
	printf(" (");
	for (i = 0; i < sizeof(key); i++) {
	    printf("%0.2x", ((char *)&key)[i] & 0xff);
	    if (i == 3)
		printf(" ");
	    else if (i != 7)
		printf(".");
	}
	printf(")\n");
    }
    return code;
}

int
Statistics(struct cmd_syndesc *as, char *arock)
{
    int code;
    kasstats statics;
    kadstats dynamics;
    afs_int32 admins;
    char bob[KA_TIMESTR_LEN];

    code =
	ubik_Call(KAM_GetStats, conn, 0, KAMAJORVERSION, &admins, &statics,
		  &dynamics);
    if (code) {
	printf("call to GetStats failed: %s\n", ka_ErrorString(code));
	return code;
    }
    if (statics.minor_version != KAMINORVERSION)
	printf("Minor version number mismatch: got %d, expected %d\n",
	       statics.minor_version, KAMINORVERSION);
    printf("%d allocs, %d frees, %d password changes\n", statics.allocs,
	   statics.frees, statics.cpws);
    printf("Hash table utilization = %f%%\n",
	   (double)dynamics.hashTableUtilization / 100.0);
    ka_timestr(dynamics.start_time, bob, KA_TIMESTR_LEN);
    printf("From host %lx started at %s:\n", dynamics.host, bob);

#define print_stat(name) if (dynamics.name.requests) printf ("  of %d requests for %s, %d were aborted.\n", dynamics.name.requests, # name, dynamics.name.aborts)
    print_stat(Authenticate);
    print_stat(ChangePassword);
    print_stat(GetTicket);
    print_stat(CreateUser);
    print_stat(SetPassword);
    print_stat(SetFields);
    print_stat(DeleteUser);
    print_stat(GetEntry);
    print_stat(ListEntry);
    print_stat(GetStats);
    print_stat(GetPassword);
    print_stat(GetRandomKey);
    print_stat(Debug);
    print_stat(UAuthenticate);
    print_stat(UGetTicket);

#if (KAMAJORVERSION>5)
    print cpu stats printf("%d string checks\n", dynamics.string_checks);
#else
    printf("Used %.3f seconds of CPU time.\n",
	   dynamics.string_checks / 1000.0);
#endif
    printf("%d admin accounts\n", admins);
    return 0;
}

int
DebugInfo(struct cmd_syndesc *as, char *arock)
{
    int code;
    struct ka_debugInfo info;
    int i;
    Date start, now;
    int timeOffset;
    char bob[KA_TIMESTR_LEN];

    start = time(0);
    if (as->parms[0].items) {
	struct ubik_client *iConn;
	code =
	    ka_SingleServerConn(cell, as->parms[0].items->data,
				KA_MAINTENANCE_SERVICE, 0, &iConn);
	if (code) {
	    struct afsconf_cell cellinfo;

	    com_err(whoami, code, "couldn't find host %s in cell %s",
		    as->parms[0].items->data, cell);
	    code = ka_GetServers(cell, &cellinfo);
	    if (code)
		com_err(whoami, code, "getting servers in cell %s", cell);
	    else {
		printf("Servers in cell %s, are:\n", cell);
		for (i = 0; i < cellinfo.numServers; i++)
		    printf("  %s\n", cellinfo.hostName[i]);
	    }
	    return code;
	}
	code = ubik_Call(KAM_Debug, iConn, 0, KAMAJORVERSION, 0, &info);
	ubik_ClientDestroy(iConn);
    } else
	code = ubik_Call(KAM_Debug, conn, 0, KAMAJORVERSION, 0, &info);

    if (code) {
	com_err(whoami, code, "call to Debug failed");
	return code;
    }
    now = time(0);

    if (info.minorVersion != KAMINORVERSION)
	printf("Minor version number mismatch: got %d, expected %d\n",
	       info.minorVersion, KAMINORVERSION);

    timeOffset = info.
#if (KAMAJORVERSION>5)
	now
#else
	reserved1
#endif
	- now;
    if (timeOffset < 0)
	timeOffset = -timeOffset;
    if (timeOffset > 60) {
	printf
	    ("WARNING: Large server client clock skew: %d seconds. Call itself took %d seconds.\n",
	     timeOffset, now - start);
    }
    ka_timestr(info.startTime, bob, KA_TIMESTR_LEN);
    printf("From host %lx started %sat %s:\n", info.host,
	   (info.noAuth ? "w/o authorization " : ""), bob);
    ka_timestr(info.lastTrans, bob, KA_TIMESTR_LEN);
    printf("Last trans was %s at %s\n", info.lastOperation, bob);
    ka_timestr(info.dbHeaderRead, bob, KA_TIMESTR_LEN);
    printf("Header last read %s.\n", bob);
    printf("db version=%d, keyVersion=%d, key cache version=%d\n",
	   info.dbVersion, info.dbSpecialKeysVersion, info.kcVersion);
    printf("db ptrs: free %d, eof %d, kvno %d.\n", info.dbFreePtr,
	   info.dbEofPtr, info.dbKvnoPtr);
    ka_timestr(info.nextAutoCPW, bob, KA_TIMESTR_LEN);
    printf("Next autoCPW at %s or in %d updates.\n", bob,
	   info.updatesRemaining);
    if (info.cheader_lock || info.keycache_lock)
	printf("locks: cheader %08lx, keycache %08lx\n", info.cheader_lock,
	       info.keycache_lock);
    printf("Last authentication for %s, last admin user was %s\n",
	   info.lastAuth, info.lastAdmin);
    printf("Last TGS op was a %s ticket was for %s\n", info.lastTGSServer,
	   info.lastTGS);
    printf("Last UDP TGS was a %s ticket for %s.  UDP Authenticate for %s\n",
	   info.lastUTGSServer, info.lastUTGS, info.lastUAuth);
    printf("key cache size %d, used %d.\n", info.kcSize, info.kcUsed);
    if (info.kcUsed > KADEBUGKCINFOSIZE) {
	printf("insufficient room to return all key cache entries!\n");
	info.kcUsed = KADEBUGKCINFOSIZE;
    }
    for (i = 0; i < info.kcUsed; i++)
	ka_timestr(info.kcInfo[i].used, bob, KA_TIMESTR_LEN);
    printf("%32s %c %2x(%2x) used %s\n", info.kcInfo[i].principal,
	   (info.kcInfo[i].primary ? '*' : ' '), info.kcInfo[i].kvno,
	   info.kcInfo[i].keycksum, bob);
    return 0;
}

int
Interactive(struct cmd_syndesc *as, char *arock)
{
    finished = 0;
    return 0;
}

int
Quit(struct cmd_syndesc *as, char *arock)
{
    finished = 1;
    return 0;
}

int
MyAfterProc(struct cmd_syndesc *as)
{
    if (!strcmp(as->name, "help"))
	return 0;

    /* Determine if we need to destory the ubik connection.
     * Closing it avoids resends of packets. 
     */
    if (conn) {
	ubik_ClientDestroy(conn);
	conn = 0;
    }

    return 0;
}

int init = 0, noauth;
char name[MAXKTCNAMELEN];
char instance[MAXKTCNAMELEN];
char newCell[MAXKTCREALMLEN];
afs_int32 serverList[MAXSERVERS];

int
NoAuth(struct cmd_syndesc *as, char *arock)
{
    noauth = 1;
    return 0;
}

static int
MyBeforeProc(struct cmd_syndesc *as, char *arock)
{
    extern struct passwd *getpwuid();
    struct ktc_encryptionKey key;
    struct ktc_principal auth_server, auth_token, client;
    char realm[MAXKTCREALMLEN];

    struct ktc_token token, *pToken;
    int i, acode, code = 0;

    {
	char *ws = strrchr(as->a0name, '/');
	if (ws)
	    ws++;		/* skip everything before the "/" */
	else
	    ws = as->a0name;
	if (strlen(ws) > 0) {
	    strncpy(whoami, ws, sizeof(whoami));
	    if (strlen(whoami) + 1 >= sizeof(whoami))
		strcpy(whoami, "kas:");
	    else
		strcat(whoami, ":");
	} else
	    whoami[0] = 0;
	/* append sub-command name */
	strncat(whoami, as->name, sizeof(whoami) - strlen(whoami) - 1);
    }

    if (as->parms[12].name == 0)
	return 0;

    assert(as->parms[13].name && as->parms[14].name && as->parms[15].name
	   && as->parms[16].name);

    /* MyAfterProc() destroys the conn, but just to be sure */
    if (conn) {
	code = ubik_ClientDestroy(conn);
	conn = 0;
    }

    if (!init || as->parms[12].items || as->parms[13].items
	|| as->parms[14].items || as->parms[15].items
	|| as->parms[16].items) {
	strcpy(instance, "");
	strcpy(newCell, "");

	if (as->parms[12].items) {	/* -admin_username */
	    code =
		ka_ParseLoginName(as->parms[12].items->data, name, instance,
				  newCell);
	    if (code) {
		com_err(whoami, code, "parsing user's name '%s'",
			as->parms[12].items->data);
		return code;
	    }
	} else {
#ifdef AFS_NT40_ENV
	    DWORD len = MAXKTCNAMELEN;
	    if (!GetUserName((LPTSTR) name, &len)) {
		printf("Can't get user name \n");
		return KABADCMD;
	    }
#else
	    /* No explicit name provided: use Unix uid. */
	    struct passwd *pw = getpwuid(getuid());
	    if (pw == NULL) {
		printf("Can't figure out your name from your user id.\n");
		return KABADCMD;
	    }
	    strncpy(name, pw->pw_name, sizeof(name));
#endif
	}

	if (as->parms[14].items) {	/* -cell */
	    if (strlen(newCell) > 0) {
		printf("Duplicate cell specification not allowed\n");
	    } else {
		strncpy(newCell, as->parms[14].items->data, sizeof(newCell));
	    }
	}
	code = ka_ExpandCell(newCell, newCell, 0 /*local */ );
	if (code) {
	    com_err(whoami, code, "Can't expand cell name");
	    return code;
	}
	strcpy(cell, newCell);

	if (as->parms[15].items) {	/* -servers */
	    struct cmd_item *ip;
	    char *ap[MAXSERVERS + 2];

	    ap[0] = "";
	    ap[1] = "-servers";
	    for (ip = as->parms[15].items, i = 2; ip; ip = ip->next, i++)
		ap[i] = ip->data;
	    code = ubik_ParseClientList(i, ap, serverList);
	    if (code) {
		com_err(whoami, code, "could not parse server list");
		return code;
	    }
	    ka_ExplicitCell(cell, serverList);
	}

	noauth = (as->parms[16].items ? 1 : 0);	/* -noauth */

	init = 1;
    }

    token.ticketLen = 0;	/* in case there are no tokens */
    if (!noauth) {		/* Will prompt for a password */
	/* first see if there's already an admin ticket */
	code =
	    ka_GetAdminToken(0, 0, cell, 0, KA_SIXHOURS, &token,
			     0 /* !new */ );
	if (code) {		/* if not then get key and try again */
	    if (as->parms[13].items) {	/* if password specified */
		strncpy(passwd, as->parms[13].items->data, sizeof(passwd));
		memset(as->parms[13].items->data, 0,
		       strlen(as->parms[13].items->data));
	    } else {
		char msg[MAXKTCNAMELEN + 50];
		if (as->parms[12].items)
		    sprintf(msg, "Administrator's (%s) Password: ", name);
		else
		    sprintf(msg, "Password for %s: ", name);
		code = read_pw_string(passwd, sizeof(passwd), msg, 0);
		if (code)
		    code = KAREADPW;
		else if (strlen(passwd) == 0)
		    code = KANULLPASSWORD;
		if (code) {
		    com_err(whoami, code, "reading password");
		    return code;
		}
	    }
	    ka_StringToKey(passwd, cell, &key);
	    code =
		ka_GetAdminToken(name, instance, cell, &key, KA_SIXHOURS,
				 &token, 0 /* !new */ );
	    if (code == KABADREQUEST) {
		des_string_to_key(passwd, &key);
		code =
		    ka_GetAdminToken(name, instance, cell, &key, KA_SIXHOURS,
				     &token, 0 /* !new */ );
	    }
	    if ((code == KABADREQUEST) && (strlen(passwd) > 8)) {
		/* try with only the first 8 characters incase they set
		 * their password with an old style passwd program. */
		passwd[8] = 0;
		ka_StringToKey(passwd, cell, &key);
		code =
		    ka_GetAdminToken(name, instance, cell, &key, KA_SIXHOURS,
				     &token, 0 /* !new */ );
		if (code == 0) {
		    fprintf(stderr,
			    "Warning: you have typed a password longer than 8 characters, but only the\n");
		    fprintf(stderr,
			    "first 8 characters were actually significant.  If you change your password\n");
		    fprintf(stderr,
			    "again this warning message will go away.\n");
		}
	    }
	    if (code) {
		char *reason;
		switch (code) {
		case KABADREQUEST:
		    reason = "password was incorrect";
		    break;
		case KAUBIKCALL:
		    reason = "Authentication Server was unavailable";
		    break;
		default:
		    reason = (char *)error_message(code);
		}
		fprintf(stderr,
			"%s: Auth. as %s to AuthServer failed: %s\nProceeding w/o authentication\n",
			whoami, PrintedName(name, instance, cell), reason);
	    }
	    /* get an Authentication token while were at it. */
	    if (ka_CellToRealm(cell, realm, 0) != 0)
		realm[0] = '\0';
	    strcpy(auth_server.name, KA_TGS_NAME);
	    strcpy(auth_server.instance, realm);
	    strcpy(auth_server.cell, cell);
	    if (ktc_GetToken
		(&auth_server, &auth_token, sizeof(struct ktc_token),
		 &client) != 0) {
		acode =
		    ka_GetAuthToken(name, instance, cell, &key,
				    MAXKTCTICKETLIFETIME, (afs_int32 *) 0
				    /*Don't need pwd expiration info here */
		    );
		if (acode && (acode != code))	/* codes are usually the same */
		    com_err(whoami, code,
			    "getting Authentication token for %s",
			    PrintedName(name, instance, cell));
	    }
	    memset(&key, 0, sizeof(key));
	}
    }

    pToken = ((token.ticketLen == 0) ? 0 : &token);
    code = ka_AuthServerConn(cell, KA_MAINTENANCE_SERVICE, pToken, &conn);
    if (code && pToken) {
	com_err(whoami, code,
		"connecting to AuthServer: now trying w/o authentication");
	code = ka_AuthServerConn(cell, KA_MAINTENANCE_SERVICE, 0, &conn);
	if (code)
	    com_err(whoami, code,
		    "making unauthenticated connection to AuthServer");
    }
    if (code) {
	com_err(whoami, code,
		"Couldn't establish connection to Authentication Server");
	return code;
    }

    /* now default unspecified password by prompting from terminal */
    if (as->nParms >= 12)
	for (i = 0; i < 12; i++)
	    if (as->parms[i].name && (as->parms[i].items == 0)) {
		char *p = as->parms[i].name;	/* parameter name */
		int l = strlen(p);	/* length of name */
		/* does parameter end in "password"  */
		if (strcmp(p + (l - 8), "password") == 0) {
		    char msg[32];
		    char password[BUFSIZ];
		    struct cmd_item *ip;

		    strcpy(msg, p + 1);
		    strcat(msg, ": ");
		    code = read_pw_string(password, sizeof(password), msg, 1);
		    if (code)
			code = KAREADPW;
		    else if (strlen(password) == 0)
			code = KANULLPASSWORD;
		    if (code) {
			com_err(whoami, code, "prompting for %s", p + 1);
			return code;
		    }
		    ip = (struct cmd_item *)malloc(sizeof(struct cmd_item));
		    ip->data = (char *)malloc(strlen(password) + 1);
		    ip->next = 0;
		    strcpy(ip->data, password);
		    as->parms[i].items = ip;
		}
	    }
    if (!conn) {		/* if all else fails... */
	code = NoAuth(0, 0);	/* get unauthenticated conn */
	if (code)
	    return code;
    }
    return 0;
}

/* These are some helpful command that deal with the cache managers tokens. */

static
ForgetTicket(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;

#ifdef notdef
    struct ktc_principal server;

    if (as->parms[0].items) {
	char *name = as->parms[0].items->data;
	code =
	    ka_ParseLoginName(name, server.name, server.instance,
			      server.cell);
	if (code) {
	    com_err(whoami, code, "couldn't interpret name '%s'", name);
	    return code;
	}
	if (server.cell[0] == 0) {
	    if (code = DefaultCell())
		return code;
	    strcpy(server.cell, cell);
	} else {
	    code = ka_ExpandCell(server.cell, server.cell, 0 /*local */ );
	    if (code) {
		com_err(whoami, code, "Can't expand cell name");
		return code;
	    }
	}
	code = ktc_ForgetToken(&server);
	if (code) {
	    com_err(whoami, code, "couldn't remove tokens for %s",
		    PrintedPrincipal(&server));
	    return code;
	}
    } else {
	if (!as->parms[1].items) {
	    fprintf(stderr, "Must specify server name or -all\n");
	    return KABADCMD;
	}
	code = ktc_ForgetAllTokens();
	if (code) {
	    com_err(whoami, code, "couldn't delete all tokens");
	    return code;
	}
    }
#endif
    code = ktc_ForgetAllTokens();
    if (code) {
	com_err(whoami, code, "couldn't delete all tokens");
	return code;
    }
    return 0;
}

static
ListTickets(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code = 0;
    int index, newIndex;
    struct ktc_principal server;
    int verbose = 0;

    if (as->parms[1].items)
	verbose = 1;
    if (as->parms[0].items) {
	char *name = as->parms[0].items->data;
	code =
	    ka_ParseLoginName(name, server.name, server.instance,
			      server.cell);
	if (code) {
	    com_err(whoami, code, "couldn't interpret name '%s'", name);
	    return code;
	}
	if (server.cell[0] == 0) {
	    if (code = DefaultCell())
		return code;
	    strcpy(server.cell, cell);
	} else {
	    code = ka_ExpandCell(server.cell, server.cell, 0 /*local */ );
	    if (code) {
		com_err(whoami, code, "Can't expand cell name");
		return code;
	    }
	}
	code = ListTicket(&server, verbose);
    } else
	for (index = 0;; index = newIndex) {
	    code = ktc_ListTokens(index, &newIndex, &server);
	    if (code) {
		if (code == KTC_NOENT)
		    code = 0;	/* end of list */
		break;
	    }
	    code = ListTicket(&server, verbose);
	}
    return code;
}

static void
add_std_args(register struct cmd_syndesc *ts)
{
    cmd_Seek(ts, 12);
    /* 12 */ cmd_AddParm(ts, "-admin_username", CMD_SINGLE, CMD_OPTIONAL,
			 "admin principal to use for authentication");
    /* 13 */ cmd_AddParm(ts, "-password_for_admin", CMD_SINGLE, CMD_OPTIONAL,
			 "admin password");
    /* 14 */ cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    /* 15 */ cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL,
			 "explicit list of authentication servers");
    /* 16 */ cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL,
			 "don't authenticate");
}

afs_int32
ka_AdminInteractive(int cmd_argc, char *cmd_argv[])
{
    register int code;
    register struct cmd_syndesc *ts;

    char line[BUFSIZ];
    afs_int32 argc;
    char *argv[32];

    strncpy(myName, *cmd_argv, 509);

    cmd_SetBeforeProc(MyBeforeProc, NULL);
    cmd_SetAfterProc(MyAfterProc, NULL);

    ts = cmd_CreateSyntax("interactive", Interactive, 0,
			  "enter interactive mode");
    add_std_args(ts);

    ts = cmd_CreateSyntax("noauthentication", NoAuth, 0,
			  "connect to AuthServer w/o using token");

    ts = cmd_CreateSyntax("list", ListUsers, 0, "list all users in database");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL,
		"show detailed info about each user");
    cmd_AddParm(ts, "-showadmin", CMD_FLAG, CMD_OPTIONAL,
		"show all cell administrators");
    cmd_AddParm(ts, "-showkey", CMD_FLAG, CMD_OPTIONAL,
		"show the user's actual key rather than the checksum");
    add_std_args(ts);
    cmd_CreateAlias(ts, "ls");

    ts = cmd_CreateSyntax("examine", ExamineUser, 0,
			  "examine the entry for a user");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of user");
    cmd_AddParm(ts, "-showkey", CMD_FLAG, CMD_OPTIONAL,
		"show the user's actual key rather than the checksum");
    add_std_args(ts);

    ts = cmd_CreateSyntax("create", CreateUser, 0,
			  "create an entry for a user");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of user");
    cmd_AddParm(ts, "-initial_password", CMD_SINGLE, CMD_OPTIONAL,
		"initial password");
    add_std_args(ts);

    ts = cmd_CreateSyntax("delete", DeleteUser, 0, "delete a user");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of user");
    add_std_args(ts);
    cmd_CreateAlias(ts, "rm");

    ts = cmd_CreateSyntax("setfields", SetFields, 0,
			  "set various fields in a user's entry");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of user");
    cmd_AddParm(ts, "-flags", CMD_SINGLE, CMD_OPTIONAL,
		"hex flag value or flag name expression");
    cmd_AddParm(ts, "-expiration", CMD_SINGLE, CMD_OPTIONAL,
		"date of account expiration");
    cmd_AddParm(ts, "-lifetime", CMD_SINGLE, CMD_OPTIONAL,
		"maximum ticket lifetime");
    cmd_AddParm(ts, "-pwexpires", CMD_SINGLE, CMD_OPTIONAL,
		"number days password is valid ([0..254])");
    cmd_AddParm(ts, "-reuse", CMD_SINGLE, CMD_OPTIONAL,
		"permit password reuse (yes/no)");
    cmd_AddParm(ts, "-attempts", CMD_SINGLE, CMD_OPTIONAL,
		"maximum successive failed login tries ([0..254])");
    cmd_AddParm(ts, "-locktime", CMD_SINGLE, CMD_OPTIONAL,
		"failure penalty [hh:mm or minutes]");
#if ASSOCIATES
    cmd_AddParm(ts, "-associates", CMD_SINGLE, CMD_OPTIONAL,
		"maximum associate instances");
#endif
    add_std_args(ts);
    cmd_CreateAlias(ts, "sf");


    ts = cmd_CreateSyntax("unlock", Unlock, 0,
			  "Enable authentication ID after max failed attempts exceeded");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "authentication ID");
    add_std_args(ts);


    ts = cmd_CreateSyntax("stringtokey", StringToKey, 0,
			  "convert a string to a key");
    cmd_AddParm(ts, "-string", CMD_SINGLE, 0, "password string");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");

    ts = cmd_CreateSyntax("setpassword", SetPassword, 0,
			  "set a user's password");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of user");
    cmd_AddParm(ts, "-new_password", CMD_SINGLE, CMD_OPTIONAL,
		"new password");
    cmd_Seek(ts, 3);
    cmd_AddParm(ts, "-kvno", CMD_SINGLE, CMD_OPTIONAL, "key version number");
    add_std_args(ts);
    cmd_CreateAlias(ts, "sp");
#ifdef CMD_PARSER_AMBIG_FIX
    cmd_CreateAlias(ts, "setpasswd");
#endif

    /* set a user's key */
    ts = cmd_CreateSyntax("setkey", SetPassword, 0, (char *)CMD_HIDDEN);
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of user");
    cmd_Seek(ts, 2);
    cmd_AddParm(ts, "-new_key", CMD_SINGLE, 0, "eight byte new key");
    cmd_Seek(ts, 3);
    cmd_AddParm(ts, "-kvno", CMD_SINGLE, CMD_OPTIONAL, "key version number");
    add_std_args(ts);

    /* get a user's password */
    ts = cmd_CreateSyntax("getpassword", GetPassword, 0, (char *)CMD_HIDDEN);
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of user");
    /* don't take standard args */
    /* add_std_args (ts); */
#ifdef CMD_PARSER_AMBIG_FIX
    cmd_CreateAlias(ts, "getpasswd");
#endif

    /* get a random key */
    ts = cmd_CreateSyntax("getrandomkey", GetRandomKey, 0,
			  (char *)CMD_HIDDEN);
    add_std_args(ts);

    /* get a ticket for a specific server */
    ts = cmd_CreateSyntax("getticket", GetTicket, 0, (char *)CMD_HIDDEN);
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of server");
    cmd_AddParm(ts, "-lifetime", CMD_SINGLE, CMD_OPTIONAL, "ticket lifetime");
    add_std_args(ts);

    ts = cmd_CreateSyntax("statistics", Statistics, 0,
			  "show statistics for AuthServer");
    add_std_args(ts);

    /* show debugging info from AuthServer */
    ts = cmd_CreateSyntax("debuginfo", DebugInfo, 0, (char *)CMD_HIDDEN);
    cmd_AddParm(ts, "-hostname", CMD_SINGLE, CMD_OPTIONAL,
		"authentication server host name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("forgetticket", ForgetTicket, 0,
			  "delete user's tickets");
#ifdef notdef
    cmd_AddParm(ts, "-name", CMD_SINGLE, (CMD_OPTIONAL | CMD_HIDE),
		"name of server");
#endif
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "delete all tickets");

    ts = cmd_CreateSyntax("listtickets", ListTickets, 0,
			  "show all cache manager tickets");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_OPTIONAL, "name of server");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL,
		"show session key and ticket");

    ts = cmd_CreateSyntax("quit", Quit, 0, "exit program");

    finished = 1;
    conn = 0;			/* no connection yet */
    zero_argc = cmd_argc;
    zero_argv = cmd_argv;

    strcpy(whoami, "kas");

    if (code = cmd_Dispatch(cmd_argc, cmd_argv)) {
	return code;
    }

    while (!finished) {
	char *s;
	int i;

	printf("ka> ");
	s = fgets(line, sizeof(line), stdin);
	if (s == NULL)
	    return 0;		/* EOF on input */
	for (i = strlen(line) - 1; i >= 0 && isspace(line[i]); i--)
	    line[i] = 0;
	if (i < 0)
	    continue;		/* blank line */

	code =
	    cmd_ParseLine(line, argv, &argc, sizeof(argv) / sizeof(argv[0]));
	if (code) {
	    com_err(whoami, code, "parsing line: '%s'", line);
	    return code;
	}
	code = cmd_Dispatch(argc, argv);
	cmd_FreeArgv(argv);
    }
    return code;
}
