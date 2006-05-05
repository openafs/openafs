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

#include <afs/stds.h>
#include <errno.h>
#include "kauth.h"
#include <sys/types.h>
#include <time.h>
#ifdef AFS_NT40_ENV
#include <afs/afsutil.h>
#else
#include <sys/resource.h>
#include <sys/file.h>
#endif
#include <stdio.h>
#include <lock.h>
#include <ubik.h>
#include <lwp.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <des.h>
#include <afs/cellconfig.h>
#include <afs/auth.h>
#include "kautils.h"
#include "kaserver.h"
#include "kalog.h"
#include "kaport.h"
#include "afs/audit.h"

extern struct ubik_dbase *KA_dbase;
struct kaheader cheader;
Date cheaderReadTime;		/* time cheader last read in */
extern struct afsconf_dir *KA_conf;	/* for getting cell info */

afs_int32 kamCreateUser(), ChangePassWord(), kamSetPassword(), kamSetFields(),
kamDeleteUser();
afs_int32 kamGetEntry(), kamListEntry(), kamGetStats(), kamGetPassword(),
kamGetRandomKey(), kamDebug();
char lrealm[MAXKTCREALMLEN];

#ifndef EXPIREPW		/* password expiration default yes */
#define EXPIREPW
#endif

#ifndef AUTOCPWINTERVAL
#define AUTOCPWINTERVAL (24*3600)
#endif
#ifndef AUTOCPWUPDATES
#define AUTOCPWUPDATES 128
#endif

extern int npwSums;

static afs_int32 autoCPWInterval;
static afs_int32 autoCPWUpdates;

static afs_int32 set_password();	/* forward */
extern afs_int32 InitAuthServ();	/* forward */
static afs_int32 impose_reuse_limits();	/* forward */
static int create_user();	/* forward */

/* This routine is called whenever an RPC interface needs the time.  It uses
   the current time to randomize a 128 bit value that is used to change the
   AuthServer Admin and TGS keys automatically. */

static Date nextAutoCPWTime = 0;
static afs_int32 totalUpdates = 0;

/* This routine is ostensibly to get the current time, but basically its job is
   to periodically update a random number.  It also periodically updates the
   keys for the builtin servers.  This is why it needs a transaction pointer
   and returns an error code.  If the caller is in a read transaction, the tt
   ptr should be zero and the return code need not be checked. */

static afs_int32
get_time(timeP, tt, admin)
     Date *timeP;
     struct ubik_trans *tt;	/* tt != 0: a write transaction */
     int admin;			/* the caller is an admin user */
{
    /* random value used to change Admin & TGS keys, this is at risk during
     * multi-threaded operation, but I think the consequences are fairly
     * harmless. */
    static afs_uint32 random_value[4];

    struct timeval time;
    unsigned int bit, nbit;
    int i;
    afs_int32 to;

    gettimeofday(&time, 0);
    bit = (random_value[3] >> 31) & 1;	/* get high bit of high word */
    for (i = 0; i < 4; i++) {
	nbit = random_value[i] >> 31;
	random_value[i] = (random_value[i] << 1) + bit;
	bit = nbit & 1;
    }
    /* get 60ths from usec.  This is all the real randomness there is. */
    random_value[0] += time.tv_usec / 16667;

    if (nextAutoCPWTime == 0) {	/* initialize things */
	nextAutoCPWTime = time.tv_sec + autoCPWInterval;
	memcpy(&random_value[0], &time, 8);
	memcpy(&random_value[2], &time, 8);
    }

    if ((++totalUpdates >= autoCPWUpdates) && tt &&	/* a write transaction */
	((admin && (time.tv_sec >= nextAutoCPWTime))
	 || (time.tv_sec >= nextAutoCPWTime + autoCPWInterval))) {
	struct ktc_encryptionKey key;
	char buf[4 * sizeof(key) + 1];
	struct kaentry tentry;
	afs_int32 code;
	char bob[KA_TIMESTR_LEN];

	ka_timestr(time.tv_sec, bob, KA_TIMESTR_LEN);
	es_Report("Auto CPW at %s\n", bob);
	if (!admin)
	    es_Report(" ... even though no ADMIN user\n");

	code = FindBlock(tt, KA_ADMIN_NAME, KA_ADMIN_INST, &to, &tentry);
	if (code)
	    return code;
	if (to) {		/* check if auto cpw is disabled */
	    if (!(ntohl(tentry.flags) & KAFNOCPW)) {
		memcpy(&key, &random_value[0], sizeof(key));
		des_fixup_key_parity(&key);
		code =
		    set_password(tt, KA_ADMIN_NAME, KA_ADMIN_INST, &key, 0,
				 0);
		if (code == 0) {
		    des_init_random_number_generator(&key);
		    ka_ConvertBytes(buf, sizeof(buf), (char *)&key,
				    sizeof(key));
		    es_Report("New Admin key is %s\n", buf);
		} else {
		    es_Report
			("in get_time: set_password failed because: %d\n",
			 code);
		    return code;
		}
	    }
	}

	code = FindBlock(tt, KA_TGS_NAME, lrealm, &to, &tentry);
	if (code)
	    return code;
	if (to) {		/* check if auto cpw is disabled */
	    if (!(ntohl(tentry.flags) & KAFNOCPW)) {
		memcpy(&key, &random_value[2], sizeof(key));
		des_fixup_key_parity(&key);
		code = set_password(tt, KA_TGS_NAME, lrealm, &key, 0, 0);
		if (code == 0) {
		    ka_ConvertBytes(buf, sizeof(buf), (char *)&key,
				    sizeof(key));
		    es_Report("New TGS key is %s\n", buf);
		} else {
		    es_Report
			("in get_time: set_password failed because: %s\n",
			 error_message(code));
		    return code;
		}
	    }
	}
	code = ka_FillKeyCache(tt);	/* ensure in-core copy is uptodate */
	if (code)
	    return code;

	nextAutoCPWTime = time.tv_sec + autoCPWInterval;
	totalUpdates = 0;
    }
    if (timeP)
	*timeP = time.tv_sec;
    return 0;
}

static int noAuthenticationRequired;	/* global state */
static int recheckNoAuth;	/* global state */

/* kaprocsInited is sort of a lock: during a transaction only one process runs
   while kaprocsInited is false. */

static int kaprocsInited = 0;

/* This variable is protected by the kaprocsInited flag. */

static int (*rebuildDatabase) ();

/* This is called to initialize the database */

static int
initialize_database(tt)
     struct ubik_trans *tt;
{
    struct ktc_encryptionKey key;
    int code;

    gettimeofday((struct timeval *)&key, 0);	/* this is just a cheap seed key */
    des_fixup_key_parity(&key);
    des_init_random_number_generator(&key);
    if ((code = des_random_key(&key))
	|| (code =
	    create_user(tt, KA_ADMIN_NAME, KA_ADMIN_INST, &key, 0,
			KAFNORMAL | KAFNOSEAL | KAFNOTGS)))
	return code;
    if ((code = des_random_key(&key))
	|| (code =
	    create_user(tt, KA_TGS_NAME, lrealm, &key, 0,
			KAFNORMAL | KAFNOSEAL | KAFNOTGS)))
	return code;
    return 0;
}

/* This routine handles initialization required by this module.  The initFlags
   parameter passes some information about the command line arguments. */

afs_int32
init_kaprocs(lclpath, initFlags)
     char *lclpath;
     int initFlags;
{
    int code;
    struct ubik_trans *tt;
    struct ktc_encryptionKey key;
    afs_int32 kvno;

    kaprocsInited = 0;
    if (myHost == 0)
	return KAINTERNALERROR;
    if (KA_conf == 0)
	return KAINTERNALERROR;
    code = afsconf_GetLocalCell(KA_conf, lrealm, sizeof(lrealm));
    if (code) {
	printf("** Can't determine local cell name!\n");
	return KANOCELLS;
    }
    ucstring(lrealm, lrealm, sizeof(lrealm));

    recheckNoAuth = 1;
    if (initFlags & 1)
	noAuthenticationRequired = 1;
    if (initFlags & 2)
	recheckNoAuth = 0;
    if (recheckNoAuth)
	noAuthenticationRequired = afsconf_GetNoAuthFlag(KA_conf);
    if (noAuthenticationRequired)
	printf("Running server with security disabled\n");

    if (initFlags & 4) {
	autoCPWInterval = 10;
	autoCPWUpdates = 10;
    } else {
	autoCPWInterval = AUTOCPWINTERVAL;
	autoCPWUpdates = AUTOCPWUPDATES;
    }

    init_kadatabase(initFlags);
    rebuildDatabase = initialize_database;

    if (code = InitAuthServ(&tt, LOCKREAD, 0)) {
	printf("init_kaprocs: InitAuthServ failed: code = %d\n", code);
	return code;
    }
    code = ka_LookupKey(tt, KA_ADMIN_NAME, KA_ADMIN_INST, &kvno, &key);
    if (code) {
	ubik_AbortTrans(tt);
	printf
	    ("init_kaprocs: ka_LookupKey (code = %d) DB not initialized properly?\n",
	     code);
	return code;
    }
    des_init_random_number_generator(&key);

    code = ubik_EndTrans(tt);
    if (code) {
	printf("init_kaprocs: ubik_EndTrans failed: code = %d\n", code);
	return code;
    }

    kaux_opendb(lclpath);	/* aux database stores failure counters */
    rebuildDatabase = 0;	/* only do this during init */
    kaprocsInited = 1;
    return 0;
}

/* These variable are for returning debugging info about the state of the
   server.  If they get trashed during multi-threaded operation it doesn't
   matter. */

/* this is global so COUNT_REQ in krb_udp.c can refer to it. */
char *lastOperation = 0;	/* name of last operation */
static Date lastTrans;		/* time of last transaction */

static char adminPrincipal[256];
static char authPrincipal[256];
static char tgsPrincipal[256];
static char tgsServerPrincipal[256];

void
save_principal(p, n, i, c)
     char *p, *n, *i, *c;
{
    int s = 255;
    int l;

    l = strlen(n);
    if (l > s)
	return;
    strcpy(p, n);
    s -= l;
    if (i && strlen(i)) {
	if (s-- <= 0)
	    return;
	strcat(p, ".");
	l = strlen(i);
	if (l > s)
	    return;
	strcat(p, i);
	s -= l;
    }
    if (c && strlen(c)) {
	if (s-- <= 0)
	    return;
	strcat(p, "@");
	l = strlen(c);
	if (l > s)
	    return;
	strcat(p, c);
    }
}

static afs_int32
check_auth(call, at, admin, acaller_id)
     struct rx_call *call;
     struct ubik_trans *at;
     int admin;			/* require caller to be ADMIN */
     afs_int32 *acaller_id;
{
    rxkad_level level;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
    afs_int32 kvno;
    Date expiration;		/* checked by Security Module */
    struct kaentry tentry;
    int code;
    int si;

    *acaller_id = 0;

    if (recheckNoAuth)
	noAuthenticationRequired = afsconf_GetNoAuthFlag(KA_conf);

    si = rx_SecurityClassOf(rx_ConnectionOf(call));
    if (si == RX_SCINDEX_VAB) {
	printf("No support for VAB security module yet.\n");
	return -1;
    } else if (si == RX_SCINDEX_NULL) {
	code = KANOAUTH;
	goto no_auth;
    } else if (si != RX_SCINDEX_KAD) {
	es_Report("Unknown security index %d\n", si);
	return -1;
    }

    code =
	rxkad_GetServerInfo(rx_ConnectionOf(call), &level, &expiration, name,
			    instance, cell, &kvno);
    if (code) {
	goto no_auth;
    }
    if (level != rxkad_crypt) {
	es_Report("Incorrect security level = %d\n", level);
	code = KANOAUTH;
	goto no_auth;
    }

    if (!name_instance_legal(name, instance))
	return KABADNAME;
    if (strlen(cell)) {
	ka_PrintUserID
	    ("Authorization rejected because we don't understand intercell stuff yet: ",
	     name, instance, "");
	printf("@%s\n", cell);
	return KANOAUTH;
    }

    code = FindBlock(at, name, instance, acaller_id, &tentry);
    if (code)
	return code;
    if (*acaller_id == 0) {
	ka_PrintUserID("User ", name, instance, " unknown.\n");
	return KANOENT;
    }
    save_principal(adminPrincipal, name, instance, 0);

    if (admin) {
	if (!(ntohl(tentry.flags) & KAFADMIN)) {
	    if (noAuthenticationRequired) {
		ka_PrintUserID("Authorization approved for ", name, instance,
			       " because there is no authentication required\n");
		osi_auditU(call, UnAuthEvent, code, AUD_STR, name, AUD_STR,
			   instance, AUD_STR, cell, AUD_END);
		return 0;
	    }
	    ka_PrintUserID("User ", name, instance, " is not ADMIN.\n");
	    return KANOAUTH;
	}
	osi_auditU(call, UseOfPrivilegeEvent, code, AUD_STR, name, AUD_STR,
		   instance, AUD_STR, cell, AUD_END);
    }
    return 0;

  no_auth:
    if (noAuthenticationRequired) {
	es_Report
	    ("Caller w/o authorization approved no authentication required\n");
	osi_auditU(call, UnAuthEvent, code, AUD_STR, name, AUD_STR, instance,
		   AUD_STR, cell, AUD_END);
	return 0;
    }
    return code;		/* no auth info */
}

afs_int32
AwaitInitialization()
{
    afs_int32 start = 0;
    while (!kaprocsInited) {
	if (!start)
	    start = time(0);
	else if (time(0) - start > 5)
	    return UNOQUORUM;
	IOMGR_Sleep(1);
    }
    return 0;
}

/* This is called by every RPC interface to create a Ubik transaction and read
   the database header into core */

afs_int32
InitAuthServ(tt, lock, this_op)
     struct ubik_trans **tt;
     int lock;			/* indicate read/write transaction */
     int *this_op;		/* opcode of RPC proc, for COUNT_ABO */
{
    int code;
    afs_int32 start = 0;	/* time started waiting for quorum */
    float wait = 0.91;		/* start waiting for 1 second */

    /* Wait for server initialization to finish if not during init_kaprocs */
    if (this_op)
	if (code = AwaitInitialization())
	    return code;

    for (code = UNOQUORUM; code == UNOQUORUM;) {
	if (lock == LOCKREAD)
	    code = ubik_BeginTransReadAny(KA_dbase, UBIK_READTRANS, tt);
	else
	    code = ubik_BeginTrans(KA_dbase, UBIK_WRITETRANS, tt);
	if (code == UNOQUORUM) {	/* no quorum elected */
	    if (!start)
		start = time(0);
	    else {
		int delay = time(0) - start;
		if (this_op) {	/* punt quickly, if RPC call */
		    if (delay > 5)
			return code;
		} else {	/* more patient during init. */
		    if (delay > 500)
			return code;
		}
	    }
	    printf("Waiting for quorum election.\n");
	    if (wait < 15.0)
		wait *= 1.1;
	    IOMGR_Sleep((int)wait);
	}
    }
    if (code)
	return code;
    if (code = ubik_SetLock(*tt, 1, 1, lock)) {
	if (this_op)
	    COUNT_ABO;
	ubik_AbortTrans(*tt);
	return code;
    }
    /* check that dbase is initialized and setup cheader */
    if (lock == LOCKREAD) {
	/* init but don't fix because this is read only */
	code = CheckInit(*tt, 0);
	if (code) {
	    ubik_AbortTrans(*tt);	/* abort, since probably I/O error */
	    /* we did the check under a ReadAny transaction, but now, after
	     * getting a write transaction (and thus some real guarantees
	     * about what databases are really out there), we will check again
	     * in CheckInit before nuking the database.  Since this may now get
	     * a UNOQUORUM we'll just do this from the top.
	     */
	    if (code = InitAuthServ(tt, LOCKWRITE, this_op))
		return code;
	    if (code = ubik_EndTrans(*tt))
		return code;

	    /* now open the read transaction that was originally requested. */
	    return InitAuthServ(tt, lock, this_op);
	}
    } else {
	if (code = CheckInit(*tt, rebuildDatabase)) {
	    if (this_op)
		COUNT_ABO;
	    ubik_AbortTrans(*tt);
	    return code;
	}
    }
    lastTrans = time(0);
    ka_FillKeyCache(*tt);	/* ensure in-core copy is uptodate */
    return 0;
}

/* returns true if name is specially known by AuthServer */

static int
special_name(name, instance)
     char *name;
     char *instance;
{
    return ((!strcmp(name, KA_TGS_NAME) && !strcmp(instance, lrealm))
	    || (strcmp(name, KA_ADMIN_NAME) == 0));
}

static int
create_user(tt, name, instance, key, caller, flags)
     struct ubik_trans *tt;
     char *name;
     char *instance;
     EncryptionKey *key;
     afs_int32 caller;
     afs_int32 flags;
{
    register int code;
    afs_int32 to;
    struct kaentry tentry;
    afs_int32 maxLifetime;

    code = FindBlock(tt, name, instance, &to, &tentry);
    if (code)
	return code;
    if (to)
	return KAEXIST;		/* name already exists, we fail */

    to = AllocBlock(tt, &tentry);
    if (to == 0)
	return KACREATEFAIL;

    /* otherwise we have a block */
    strncpy(tentry.userID.name, name, sizeof(tentry.userID.name));
    strncpy(tentry.userID.instance, instance, sizeof(tentry.userID.instance));
    tentry.flags = htonl(flags);
    if (special_name(name, instance)) {	/* this overrides key & version */
	tentry.flags = htonl(ntohl(tentry.flags) | KAFSPECIAL);
	tentry.key_version = htonl(-1);	/* don't save this key */
	if (code = ka_NewKey(tt, to, &tentry, key))
	    return code;
    } else {
	memcpy(&tentry.key, key, sizeof(tentry.key));
	tentry.key_version = htonl(0);
    }
    tentry.user_expiration = htonl(NEVERDATE);
    code = get_time(&tentry.modification_time, tt, 1);
    if (code)
	return code;

    /* time and addr of entry for guy changing this entry */
    tentry.modification_time = htonl(tentry.modification_time);
    tentry.modification_id = htonl(caller);
    tentry.change_password_time = tentry.modification_time;

    if (strcmp(name, KA_TGS_NAME) == 0)
	maxLifetime = MAXKTCTICKETLIFETIME;
    else if (strcmp(name, KA_ADMIN_NAME) == 0)
	maxLifetime = 10 * 3600;
    else if (strcmp(name, AUTH_SUPERUSER) == 0)
	maxLifetime = 100 * 3600;
    else
	maxLifetime = 25 * 3600;	/* regular users */
    tentry.max_ticket_lifetime = htonl(maxLifetime);

    code = ThreadBlock(tt, to, &tentry);
    return code;
}

/* Put actual stub routines here */

afs_int32
SKAM_CreateUser(call, aname, ainstance, ainitpw)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     EncryptionKey ainitpw;
{
    afs_int32 code;

    code = kamCreateUser(call, aname, ainstance, ainitpw);
    osi_auditU(call, AFS_KAM_CrUserEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);
    return code;
}


afs_int32
kamCreateUser(call, aname, ainstance, ainitpw)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     EncryptionKey ainitpw;
{
    register int code;
    struct ubik_trans *tt;
    afs_int32 caller;		/* Disk offset of caller's entry */

    COUNT_REQ(CreateUser);
    if (!des_check_key_parity(&ainitpw) || des_is_weak_key(&ainitpw))
	return KABADKEY;
    if (!name_instance_legal(aname, ainstance))
	return KABADNAME;
    if (code = InitAuthServ(&tt, LOCKWRITE, this_op))
	return code;
    code = check_auth(call, tt, 1, &caller);
    if (code) {
	COUNT_ABO;
	ubik_AbortTrans(tt);
	return code;
    }
    code = create_user(tt, aname, ainstance, &ainitpw, caller, KAFNORMAL);
    if (code) {
	COUNT_ABO;
	ubik_AbortTrans(tt);
	return code;
    }
    code = ubik_EndTrans(tt);
    KALOG(aname, ainstance, NULL, NULL, NULL,
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))),
	  LOG_CRUSER);
    return code;
}

afs_int32
SKAA_ChangePassword(call, aname, ainstance, arequest, oanswer)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     ka_CBS *arequest;
     ka_BBS *oanswer;
{
    afs_int32 code;

    code = ChangePassWord(call, aname, ainstance, arequest, oanswer);
    osi_auditU(call, AFS_KAA_ChPswdEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);
    return code;
}

afs_int32
ChangePassWord(call, aname, ainstance, arequest, oanswer)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     ka_CBS *arequest;
     ka_BBS *oanswer;
{
    register int code;
    struct ubik_trans *tt;
    afs_int32 to;		/* offset of block */
    struct kaentry tentry;
    struct ka_cpwRequest request;	/* request after decryption */
    char *answer;		/* where answer is to be put */
    int answer_len;		/* length of answer packet */
    afs_int32 kvno;		/* requested key version number */
    des_key_schedule user_schedule;	/* key schedule for user's key */
    Date request_time;		/* time request originated */

    COUNT_REQ(ChangePassword);
    if (!name_instance_legal(aname, ainstance))
	return KABADNAME;
    if (strcmp(ainstance, KA_ADMIN_NAME) == 0)
	return KABADNAME;
    if (code = InitAuthServ(&tt, LOCKWRITE, this_op))
	return code;

    code = FindBlock(tt, aname, ainstance, &to, &tentry);
    if (code) {
	goto abort;
    }
    if (to == 0) {		/* no such user */
	code = KANOENT;
	goto abort;
    }
    if (ntohl(tentry.flags) & KAFNOCPW) {
	code = KABADCPW;
	goto abort;
    }

    /* decrypt request w/ user password */
    if (code = des_key_sched(&tentry.key, user_schedule))
	es_Report("In KAChangePassword: key_sched returned %d\n", code);
    des_pcbc_encrypt(arequest->SeqBody, &request,
		     min(arequest->SeqLen, sizeof(request)), user_schedule,
		     &tentry.key, DECRYPT);

    /* validate the request */
    request_time = ntohl(request.time);	/* reorder date */
    kvno = ntohl(request.kvno);
    if ((abs(request_time - time(0)) > KTC_TIME_UNCERTAINTY) || strncmp(request.label, KA_CPW_REQ_LABEL, sizeof(request.label)) || (request.spare) || (kvno > MAXKAKVNO)) {	/* these are reseved */
	code = KABADREQUEST;
	goto abort;
    }

    /* check to see if the new password was used before, or if there has
     * not been sufficient time since the last password change
     */
    code = impose_reuse_limits(&request.newpw, &tentry);
    if (code) {
	goto abort;
    }

    /* Create the Answer Packet */
    answer_len = sizeof(Date) + KA_LABELSIZE;
    if (oanswer->MaxSeqLen < answer_len) {
	code = KAANSWERTOOLONG;
	goto abort;
    }
    oanswer->SeqLen = answer_len;
    answer = oanswer->SeqBody;
    request.time = htonl(request_time + 1);
    memcpy(answer, (char *)&request.time, sizeof(Date));
    answer += sizeof(Date);
    memcpy(answer, KA_CPW_ANS_LABEL, KA_LABELSIZE);

    des_pcbc_encrypt(oanswer->SeqBody, oanswer->SeqBody, answer_len,
		     user_schedule, &tentry.key, ENCRYPT);

    code = set_password(tt, aname, ainstance, &request.newpw, kvno, 0);
    if (code) {
	code = KAIO;
	goto abort;
    }

    cheader.stats.cpws = htonl(ntohl(cheader.stats.cpws) + 1);
    code =
	kawrite(tt, DOFFSET(0, &cheader, &cheader.stats.cpws),
		(char *)&cheader.stats.cpws, sizeof(afs_int32));
    if (code) {
	code = KAIO;
	goto abort;
    }

    code = ubik_EndTrans(tt);
    return code;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    return code;
}

static afs_int32
impose_reuse_limits(password, tentry)
     EncryptionKey *password;
     struct kaentry *tentry;
{
    int code;
    Date now;
    int i;
    extern int MinHours;
    afs_uint32 newsum;

    if (!tentry->pwsums[0] && npwSums > 1 && !tentry->pwsums[1])
	return 0;		/* password reuse limits not in effect */

    code = get_time(&now, 0, 0);
    if (code)
	return code;

    if ((now - ntohl(tentry->change_password_time)) < MinHours * 60 * 60)
	return KATOOSOON;

    if (!memcmp(password, &(tentry->key), sizeof(EncryptionKey)))
	return KAREUSED;

    code = ka_KeyCheckSum((char *)password, &newsum);
    if (code)
	return code;

    newsum = newsum & 0x000000ff;
    for (i = 0; i < npwSums; i++) {
	if (newsum == tentry->pwsums[i])
	    return KAREUSED;
    }

    return 0;
}


static afs_int32
set_password(tt, name, instance, password, kvno, caller)
     struct ubik_trans *tt;
     char *name;
     char *instance;
     EncryptionKey *password;
     afs_int32 kvno;
     afs_int32 caller;
{
    afs_int32 code;
    afs_int32 to;		/* offset of block */
    struct kaentry tentry;
    Date now;
    int i;
    extern int npwSums;
    afs_uint32 newsum;

    code = FindBlock(tt, name, instance, &to, &tentry);
    if (code)
	return code;
    if (to == 0)
	return KANOENT;		/* no such user */

    /* if password reuse limits in effect, set the checksums, the hard way */
    if (!tentry.pwsums[0] && npwSums > 1 && !tentry.pwsums[1]) {
	/* do nothing, no limits */ ;
    } else {
	code = ka_KeyCheckSum((char *)&(tentry.key), &newsum);
	if (code)
	    return code;
	for (i = npwSums - 1; i; i--)
	    tentry.pwsums[i] = tentry.pwsums[i - 1];
	tentry.pwsums[0] = newsum & 0x000000ff;
    }


    if (special_name(name, instance)) {	/* set key over rides key_version */
	tentry.flags = htonl(ntohl(tentry.flags) | KAFSPECIAL);
	if (code = ka_NewKey(tt, to, &tentry, password))
	    return (code);
    } else {
	memcpy(&tentry.key, password, sizeof(tentry.key));
	if (!kvno) {
	    kvno = ntohl(tentry.key_version);
	    if ((kvno < 1) || (kvno >= MAXKAKVNO))
		kvno = 1;
	    else
		kvno++;
	}
	tentry.key_version = htonl((afs_int32) kvno);	/* requested key version */
    }



    /* no-write prevents recursive call to set_password by AuthCPW code. */
    code = get_time(&now, 0, 0);
    if (code)
	return code;
    if (caller) {
	tentry.modification_time = htonl(now);
	tentry.modification_id = htonl(caller);
    }

    tentry.change_password_time = htonl(now);

    if (code = kawrite(tt, to, &tentry, sizeof(tentry)))
	return (KAIO);
    return (0);
}

afs_int32
SKAM_SetPassword(call, aname, ainstance, akvno, apassword)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 akvno;
     EncryptionKey apassword;
{
    afs_int32 code;

    code = kamSetPassword(call, aname, ainstance, akvno, apassword);
    osi_auditU(call, AFS_KAM_SetPswdEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);
    return code;
}

afs_int32
kamSetPassword(call, aname, ainstance, akvno, apassword)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 akvno;
     EncryptionKey apassword;
{
    register int code;
    struct ubik_trans *tt;
    afs_int32 caller;		/* Disk offset of caller's entry */
    struct kaentry tentry;

    COUNT_REQ(SetPassword);
    if (akvno > MAXKAKVNO)
	return KABADARGUMENT;
    if (!des_check_key_parity(&apassword) || des_is_weak_key(&apassword))
	return KABADKEY;

    if (!name_instance_legal(aname, ainstance))
	return KABADNAME;
    if (code = InitAuthServ(&tt, LOCKWRITE, this_op))
	return code;
    code = check_auth(call, tt, 0, &caller);
    if (code) {
	goto abort;
    }
    if (code = karead(tt, caller, &tentry, sizeof(tentry))) {
	code = KAIO;
	goto abort;
    }
    /* if the user is changing his own password or ADMIN then go ahead. */
    if ((strcmp(tentry.userID.name, aname) == 0)
	&& (strcmp(tentry.userID.instance, ainstance) == 0)) {
	if (ntohl(tentry.flags) & KAFNOCPW)
	    code = KABADCPW;
	else {
	    code = impose_reuse_limits(&apassword, &tentry);
	    if (!code)
		code =
		    set_password(tt, aname, ainstance, &apassword, akvno, 0);
	}
    } else if (ntohl(tentry.flags) & KAFADMIN) {
	code = set_password(tt, aname, ainstance, &apassword, akvno, caller);
    } else
	code = KANOAUTH;
    if (code)
	goto abort;

    code = ubik_EndTrans(tt);
    KALOG(aname, ainstance, NULL, NULL, NULL,
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), LOG_CHPASSWD);
    return code;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    return code;
}

static Date
CoerseLifetime(start, end)
     Date start, end;
{
    unsigned char kerberosV4Life;
    kerberosV4Life = time_to_life(start, end);
    end = life_to_time(start, kerberosV4Life);
    return end;
}

static afs_int32
GetEndTime(start, reqEnd, expiration, caller, server, endP)
     IN Date start;		/* start time of ticket */
     IN Date reqEnd;		/* requested end time */
     IN Date expiration;	/* authorizing ticket's expiration */
     IN struct kaentry *caller;
     IN struct kaentry *server;
     OUT Date *endP;		/* actual end time */
{
    Date cExp, sExp;
    Date cLife, sLife;
    Date end;

    if (ntohl(caller->flags) & KAFNOTGS)
	return KABADUSER;	/* no new tickets for this user */
    if (expiration && (ntohl(server->flags) & KAFNOSEAL))
	return KABADSERVER;	/* can't be target of GetTicket req */
    if (!expiration)
	expiration = NEVERDATE;

    cExp = ntohl(caller->user_expiration);
    sExp = ntohl(server->user_expiration);
    if (cExp < start)
	return KAPWEXPIRED;
    if (sExp < start)
	return KABADSERVER;
    cLife = start + ntohl(caller->max_ticket_lifetime);
    sLife = start + ntohl(server->max_ticket_lifetime);
    end =
	umin(umin(reqEnd, expiration),
	     umin(umin(cLife, sLife), umin(cExp, sExp)));
    end = CoerseLifetime(start, end);
    *endP = end;
    return 0;
}

static afs_int32
PrepareTicketAnswer(oanswer, challenge, ticket, ticketLen, sessionKey, start,
		    end, caller, server, cell, label)
     ka_BBS *oanswer;
     afs_int32 challenge;
     char *ticket;
     afs_int32 ticketLen;
     struct ktc_encryptionKey *sessionKey;
     Date start, end;
     struct kaentry *caller, *server;
     char *cell;
     char *label;
{
    afs_int32 code;
    struct ka_ticketAnswer *answer;
    afs_int32 cksum;

    code = KAANSWERTOOLONG;
    if (oanswer->MaxSeqLen <
	sizeof(struct ka_ticketAnswer) - 5 * MAXKTCNAMELEN - MAXKTCTICKETLEN +
	ticketLen)
	return code;

    answer = (struct ka_ticketAnswer *)oanswer->SeqBody;
    answer->challenge = htonl(challenge);
    memcpy(&answer->sessionKey, sessionKey, sizeof(struct ktc_encryptionKey));
    answer->startTime = htonl(start);
    answer->endTime = htonl(end);
    answer->kvno = server->key_version;
    answer->ticketLen = htonl(ticketLen);

    {
	char *ans = answer->name;	/* pointer to variable part */
	int rem;		/* space remaining */
	int len;		/* macro temp. */

	rem = oanswer->MaxSeqLen - (ans - oanswer->SeqBody);
#undef putstr
#define putstr(str) len = strlen (str)+1;\
		    if (rem < len) return code;\
		    strcpy (ans, str);\
		    ans += len; rem -= len
	putstr(caller->userID.name);
	putstr(caller->userID.instance);
	putstr(cell);
	putstr(server->userID.name);
	putstr(server->userID.instance);
	if (rem < ticketLen + KA_LABELSIZE)
	    return code;
	memcpy(ans, ticket, ticketLen);
	ans += ticketLen;
	if (label)
	    memcpy(ans, label, KA_LABELSIZE);
	else
	    memset(ans, 0, KA_LABELSIZE);
	ans += KA_LABELSIZE;
	oanswer->SeqLen = (ans - oanswer->SeqBody);
    }
    cksum = 0;
    answer->cksum = htonl(cksum);
    oanswer->SeqLen = round_up_to_ebs(oanswer->SeqLen);
    if (oanswer->SeqLen > oanswer->MaxSeqLen)
	return code;
    return 0;
}

/* This is used to get a ticket granting ticket or an admininstration ticket.
   These two specific, built-in servers are special cases, which require the
   client's key as an additional security precaution.  The GetTicket operation
   is normally disabled for these two principals. */

static afs_int32
Authenticate(version, call, aname, ainstance, start, end, arequest, oanswer)
     int version;
     struct rx_call *call;
     char *aname;
     char *ainstance;
     Date start, end;
     ka_CBS *arequest;
     ka_BBS *oanswer;
{
    int code;
    struct ubik_trans *tt;
    afs_int32 to;		/* offset of block */
    kaentry tentry;
    struct kaentry server;	/* entry for desired server */
    struct ka_gettgtRequest request;	/* request after decryption */
    int tgt, adm;		/* type of request */
    char *sname;		/* principal of server */
    char *sinst;
    char ticket[MAXKTCTICKETLEN];	/* our copy of the ticket */
    int ticketLen;
    struct ktc_encryptionKey sessionKey;	/* we have to invent a session key */
    char *answer;		/* where answer is to be put */
    int answer_len;		/* length of answer packet */
    Date answer_time;		/* 1+ request time in network order */
    afs_int32 temp;		/* for htonl conversions */
    des_key_schedule user_schedule;	/* key schedule for user's key */
    afs_int32 tgskvno;		/* key version of service key */
    struct ktc_encryptionKey tgskey;	/* service key for encrypting ticket */
    Date now;
    afs_uint32 pwexpires;

    COUNT_REQ(Authenticate);
    if (!name_instance_legal(aname, ainstance))
	return KABADNAME;
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	return code;
    get_time(&now, 0, 0);

    sname = sinst = NULL;

    code = FindBlock(tt, aname, ainstance, &to, &tentry);
    if (code) {
	goto abort;
    }
    if (to == 0) {		/* no such user */
	code = KANOENT;
	goto abort;
    }
#ifdef LOCKPW
    /* have to check for locked before verifying the password, otherwise all
     * KALOCKED means is "yup, you guessed the password all right, now wait a 
     * few minutes and we'll let you in"
     */
    if (kaux_islocked
	(to, (u_int) tentry.misc_auth_bytes[ATTEMPTS],
	 (afs_uint32) tentry.misc_auth_bytes[LOCKTIME] << 9)) {
	code = KALOCKED;
	goto abort;
    }
#endif /* LOCKPW */

    save_principal(authPrincipal, aname, ainstance, 0);

    /* decrypt request w/ user password */
    if (code = des_key_sched(&tentry.key, user_schedule))
	es_Report("In KAAuthenticate: key_sched returned %d\n", code);
    des_pcbc_encrypt(arequest->SeqBody, &request,
		     min(arequest->SeqLen, sizeof(request)), user_schedule,
		     &tentry.key, DECRYPT);

    request.time = ntohl(request.time);	/* reorder date */
    tgt = !strncmp(request.label, KA_GETTGT_REQ_LABEL, sizeof(request.label));
    adm = !strncmp(request.label, KA_GETADM_REQ_LABEL, sizeof(request.label));
    if (!(tgt || adm)) {
	kaux_inc(to, ((unsigned char)tentry.misc_auth_bytes[LOCKTIME]) << 9);
	code = KABADREQUEST;
	goto abort;
    } else
	kaux_write(to, 0, 0);	/* reset counters */

#ifdef EXPIREPW
    if (!tentry.misc_auth_bytes[EXPIRES]) {
	/* 0 in the database means never, but 0 on the network means today */
	/* 255 on the network means "long time, maybe never" */
	pwexpires = 255;
    } else {
	pwexpires = tentry.misc_auth_bytes[EXPIRES];

	pwexpires =
	    ntohl(tentry.change_password_time) + 24 * 60 * 60 * pwexpires;
	if (adm) {		/* provide a little slack for admin ticket */
	    pwexpires += 30 * 24 * 60 * 60;	/*  30 days */
	}
	if (pwexpires < now) {
	    code = KAPWEXPIRED;
	    goto abort;
	} else {
	    pwexpires = (pwexpires - now) / (24 * 60 * 60);
	    if (pwexpires > 255)
		pwexpires = 255;
	}
    }
#endif /* EXPIREPW */

    if (abs(request.time - now) > KTC_TIME_UNCERTAINTY) {
#if 0
	if (oanswer->MaxSeqLen < sizeof(afs_int32))
	    code = KAANSWERTOOLONG;
	else {			/* return our time if possible */
	    oanswer->SeqLen = sizeof(afs_int32);
	    request.time = htonl(now);
	    memcpy(oanswer->SeqBody, &request.time, sizeof(afs_int32));
	}
#endif
	code = KACLOCKSKEW;
	goto abort;
    }
    sname = (tgt ? KA_TGS_NAME : KA_ADMIN_NAME);
    sinst = (tgt ? lrealm : KA_ADMIN_INST);
    code = FindBlock(tt, sname, sinst, &to, &server);
    if (code)
	goto abort;
    if (to == 0) {
	code = KANOENT;
	goto abort;
    }

    tgskvno = ntohl(server.key_version);
    memcpy(&tgskey, &server.key, sizeof(tgskey));

    code = des_random_key(&sessionKey);
    if (code) {
	code = KANOKEYS;
	goto abort;
    }

    code = GetEndTime(start, end, 0 /*!GetTicket */ , &tentry, &server, &end);
    if (code)
	goto abort;

    code =
	tkt_MakeTicket(ticket, &ticketLen, &tgskey, aname, ainstance, "",
		       start, end, &sessionKey,
		       rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), sname,
		       sinst);
    if (code)
	goto abort;

    switch (version) {
    case 0:
	answer_len =
	    ticketLen + sizeof(Date) + sizeof(struct ktc_encryptionKey) +
	    2 * sizeof(afs_int32) + KA_LABELSIZE;
	answer_len = round_up_to_ebs(answer_len);
	if (answer_len > oanswer->MaxSeqLen) {
	    code = KAANSWERTOOLONG;
	    goto abort;
	}
	oanswer->SeqLen = answer_len;
	answer = oanswer->SeqBody;
	answer_time = htonl(request.time + 1);
	memcpy(answer, (char *)&answer_time, sizeof(Date));
	answer += sizeof(Date);
	memcpy(answer, (char *)&sessionKey, sizeof(struct ktc_encryptionKey));
	answer += sizeof(struct ktc_encryptionKey);
	temp = htonl(tgskvno);
	memcpy(answer, (char *)&temp, sizeof(afs_int32));
	answer += sizeof(afs_int32);
	temp = htonl(ticketLen);
	memcpy(answer, (char *)&temp, sizeof(afs_int32));
	answer += sizeof(afs_int32);
	memcpy(answer, ticket, ticketLen);
	answer += ticketLen;
	memcpy(answer, (tgt ? KA_GETTGT_ANS_LABEL : KA_GETADM_ANS_LABEL),
	       KA_LABELSIZE);
	break;
    case 1:
    case 2:
	code =
	    PrepareTicketAnswer(oanswer, request.time + 1, ticket, ticketLen,
				&sessionKey, start, end, &tentry, &server, "",
				(tgt ? KA_GETTGT_ANS_LABEL :
				 KA_GETADM_ANS_LABEL));
	if (code)
	    goto abort;
#ifdef EXPIREPW
	if ((version == 2)
	    && oanswer->SeqLen < oanswer->MaxSeqLen + sizeof(afs_int32)) {
	    temp = pwexpires << 24;	/* move it into the high byte */
	    pwexpires = htonl(temp);

	    memcpy((char *)oanswer->SeqBody + oanswer->SeqLen, &pwexpires,
		   sizeof(afs_int32));
	    oanswer->SeqLen += sizeof(afs_int32);
	    oanswer->SeqLen = round_up_to_ebs(oanswer->SeqLen);
	    if (oanswer->SeqLen > oanswer->MaxSeqLen) {
		code = KAANSWERTOOLONG;
		goto abort;
	    }
	}
#endif /* EXPIREPW */
	break;

    default:
	code = KAINTERNALERROR;
	goto abort;
    }
    des_pcbc_encrypt(oanswer->SeqBody, oanswer->SeqBody, oanswer->SeqLen,
		     user_schedule, &tentry.key, ENCRYPT);
    code = ubik_EndTrans(tt);
    KALOG(aname, ainstance, sname, sinst, NULL,
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), LOG_AUTHENTICATE);
    return code;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    KALOG(aname, ainstance, sname, sinst, NULL,
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), LOG_AUTHFAILED);
    return code;
}

afs_int32
SKAA_Authenticate_old(call, aname, ainstance, start, end, arequest, oanswer)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     Date start, end;
     ka_CBS *arequest;
     ka_BBS *oanswer;
{
    int code;

    IOMGR_Sleep(1);		/* discourage use of this mechanism */
    code =
	Authenticate(0, call, aname, ainstance, start, end, arequest,
		     oanswer);
    osi_auditU(call, AFS_KAA_AuthOEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);

    return code;
}

afs_int32
SKAA_Authenticate(call, aname, ainstance, start, end, arequest, oanswer)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     Date start, end;
     ka_CBS *arequest;
     ka_BBS *oanswer;
{
    int code;

    code =
	Authenticate(1, call, aname, ainstance, start, end, arequest,
		     oanswer);
    osi_auditU(call, AFS_KAA_AuthEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);

    return code;
}

afs_int32
SKAA_AuthenticateV2(call, aname, ainstance, start, end, arequest, oanswer)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     Date start, end;
     ka_CBS *arequest;
     ka_BBS *oanswer;
{
    int code;

    code =
	Authenticate(2, call, aname, ainstance, start, end, arequest,
		     oanswer);
    osi_auditU(call, AFS_KAA_AuthEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);

    return code;
}

afs_int32
SKAM_SetFields(call, aname, ainstance, aflags, aexpiration, alifetime,
	       amaxAssociates, misc_auth_bytes, spare2)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 aflags;
     Date aexpiration;
     afs_int32 alifetime;
     afs_int32 amaxAssociates;
     afs_uint32 misc_auth_bytes;	/* 4 bytes, each 0 means unspecified */
     afs_int32 spare2;
{
    afs_int32 code;

    code =
	kamSetFields(call, aname, ainstance, aflags, aexpiration, alifetime,
		     amaxAssociates, misc_auth_bytes, spare2);
    osi_auditU(call, AFS_KAM_SetFldEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_LONG, aflags, AUD_DATE, aexpiration, AUD_LONG,
	       alifetime, AUD_LONG, amaxAssociates, AUD_END);
    return code;
}

afs_int32
kamSetFields(call, aname, ainstance, aflags, aexpiration, alifetime,
	     amaxAssociates, misc_auth_bytes, spare2)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 aflags;
     Date aexpiration;
     afs_int32 alifetime;
     afs_int32 amaxAssociates;
     afs_uint32 misc_auth_bytes;	/* 4 bytes, each 0 means unspecified */
     afs_int32 spare2;
{
    afs_int32 code;
    Date now;
    struct ubik_trans *tt;
    afs_int32 caller;
    afs_int32 tentry_offset;	/* offset of entry */
    struct kaentry tentry;
    unsigned char newvals[4];

    COUNT_REQ(SetFields);

    if (spare2)
	return KABADARGUMENT;	/* not supported yet... */

    /* make sure we're supposed to do something */
    if (!(aflags || aexpiration || alifetime || (amaxAssociates >= 0)
	  || misc_auth_bytes)
	|| ((aflags & ~KAFNORMAL) & ~KAF_SETTABLE_FLAGS))
	return KABADARGUMENT;	/* arguments no good */
    if (!name_instance_legal(aname, ainstance))
	return KABADNAME;
    if (code = InitAuthServ(&tt, LOCKWRITE, this_op))
	return code;
    code = check_auth(call, tt, 1, &caller);
    if (code) {
	goto abort;
    }

    code = FindBlock(tt, aname, ainstance, &tentry_offset, &tentry);
    if (code)
	goto abort;
    if (tentry_offset == 0) {	/* no such user */
	code = KANOENT;
	goto abort;
    }
    if ((ntohl(tentry.flags) & KAFNORMAL) == 0)
	return KAINTERNALERROR;
    if (aflags) {
	/* Keep track of the total number of admin accounts.  This way we can
	 * update database without any admin privilege initially */
	if ((aflags & KAFADMIN) != (ntohl(tentry.flags) & KAFADMIN)) {
	    /* if admin state is changing */
	    int delta;
	    if (ntohl(tentry.flags) & KAFADMIN)
		delta = -1;
	    else
		delta = 1;
	    if (code = update_admin_count(tt, delta))
		goto abort;
	}
	tentry.flags =
	    htonl((ntohl(tentry.flags) & ~KAF_SETTABLE_FLAGS) | aflags);
    }
    if (code = get_time(&now, tt, 1))
	goto abort;
    if (aexpiration) {
	tentry.user_expiration = htonl(aexpiration);
	if (!ntohl(tentry.change_password_time)) {
	    tentry.change_password_time = htonl(now);
	}
    }
    if (alifetime)
	tentry.max_ticket_lifetime = htonl(alifetime);

#ifndef NOPWCONTROLS
    /* 
     * We've packed a bunch of bytes into a long for backward compatibility.
     * These include password expiration time, and some failed login limits
     * counters.  Now let's unpack them and stick them into the
     * kaentry struct.  All the bytes have values in the range
     * 1..255, else they were not specified in the interface, and are
     * set to zero. 
     * In the case of password expiration times, 1 means password never
     * expires (==>0), 2 means password only lives for one day (==>1),
     * and so on.
     */
    if (misc_auth_bytes) {
	unpack_long(misc_auth_bytes, newvals);
	if (newvals[EXPIRES]) {
	    tentry.misc_auth_bytes[EXPIRES] = newvals[EXPIRES] - 1;
	}

	if (newvals[REUSEFLAGS]) {
	    if (newvals[REUSEFLAGS] & KA_REUSEPW)
		memset(tentry.pwsums, 0, KA_NPWSUMS);
	    else if ((newvals[REUSEFLAGS] & KA_NOREUSEPW)
		     && !tentry.pwsums[0])
		tentry.pwsums[0] = 0xff;
	}

	if (newvals[ATTEMPTS]) {
	    tentry.misc_auth_bytes[ATTEMPTS] = newvals[ATTEMPTS] - 1;
	}
	if (newvals[LOCKTIME]) {
	    tentry.misc_auth_bytes[LOCKTIME] = newvals[LOCKTIME] - 1;
	}
/*
       tentry.misc_auth_bytes = htonl(tentry.misc_auth_bytes);
*/
    }
#endif /* NOPWCONTROLS */

    if (amaxAssociates >= 0) {
	if ((ntohl(tentry.flags) & KAFASSOC)
	    || (ntohl(tentry.flags) & KAFSPECIAL))
	    return KAASSOCUSER;
	if (((ntohl(tentry.flags) & KAFASSOCROOT) == 0) && (amaxAssociates > 0))	/* convert normal user to assoc root */
	    tentry.flags = htonl(ntohl(tentry.flags) | KAFASSOCROOT);
	tentry.misc.assocRoot.maxAssociates = htonl(amaxAssociates);
    }

    tentry.modification_time = htonl(now);
    tentry.modification_id = htonl(caller);
    code = kawrite(tt, tentry_offset, &tentry, sizeof(tentry));
    if (code)
	goto abort;

    code = ubik_EndTrans(tt);
    KALOG(aname, ainstance, NULL, NULL, NULL,
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), LOG_SETFIELDS);
    return code;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    return code;
}

/* delete a user */

afs_int32
SKAM_DeleteUser(call, aname, ainstance)
     struct rx_call *call;
     char *aname;
     char *ainstance;
{
    afs_int32 code;

    code = kamDeleteUser(call, aname, ainstance);
    osi_auditU(call, AFS_KAM_DelUserEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);
    return code;
}

afs_int32
kamDeleteUser(call, aname, ainstance)
     struct rx_call *call;
     char *aname;
     char *ainstance;
{
    register int code;
    struct ubik_trans *tt;
    afs_int32 caller;
    afs_int32 to;
    struct kaentry tentry;
    int nfailures;
    afs_uint32 locktime;

    COUNT_REQ(DeleteUser);
    if (!name_instance_legal(aname, ainstance))
	return KABADNAME;
    if (code = InitAuthServ(&tt, LOCKWRITE, this_op))
	return code;
    code = check_auth(call, tt, 1, &caller);
    if (code) {
      abort:
	COUNT_ABO;
	ubik_AbortTrans(tt);
	return code;
    }

    code = FindBlock(tt, aname, ainstance, &to, &tentry);
    if (code)
	goto abort;
    if (to == 0) {		/* name not found */
	code = KANOENT;
	goto abort;
    }

    kaux_read(to, &nfailures, &locktime);
    if (nfailures || locktime)
	kaux_write(to, 0, 0);	/* zero failure counters at this offset */

    /* track all AuthServer identities */
    if (special_name(aname, ainstance))
	if (code = ka_DelKey(tt, to, &tentry))
	    goto abort;

    if (ntohl(tentry.flags) & KAFADMIN)	/* keep admin count up-to-date */
	if (code = update_admin_count(tt, -1))
	    goto abort;

    if ((code = UnthreadBlock(tt, &tentry)) || (code = FreeBlock(tt, to)) || (code = get_time(0, tt, 1))	/* update randomness */
	)
	goto abort;

    code = ubik_EndTrans(tt);
    KALOG(aname, ainstance, NULL, NULL, NULL,
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), LOG_DELUSER);
    return code;
}

/* we set a bit in here which indicates that the user's limit of
 * authentication failures has been exceeded.  If that bit is not set,
 * kas can take it on faith that the user ID is not locked.  If that
 * bit is set, kas has to check all the servers to find one who will
 * report that the ID is not locked, or else to find out when the ID
 * will be unlocked.
 */
afs_int32
SKAM_GetEntry(call, aname, ainstance, aversion, aentry)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 aversion;	/* major version assumed by caller */
     kaentryinfo *aentry;	/* entry data copied here */
{
    afs_int32 code;

    code = kamGetEntry(call, aname, ainstance, aversion, aentry);
    osi_auditU(call, AFS_KAM_GetEntEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);
    return code;
}

afs_int32
kamGetEntry(call, aname, ainstance, aversion, aentry)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 aversion;	/* major version assumed by caller */
     kaentryinfo *aentry;	/* entry data copied here */
{
    register afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 callerIndex;
    struct kaentry caller;
    afs_int32 to;
    afs_uint32 temp;
    struct kaentry tentry;
    rxkad_level enc_level = rxkad_clear;
    int callerIsAdmin = 0;

    COUNT_REQ(GetEntry);
    if (aversion != KAMAJORVERSION)
	return KAOLDINTERFACE;
    if (!name_instance_legal(aname, ainstance))
	return KABADNAME;
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	return code;
    code = check_auth(call, tt, 0, &callerIndex);
    if (code) {
	goto abort;
    }
    if (noAuthenticationRequired) {
    } else if (!callerIndex) {
	code = KANOENT;
	goto abort;
    } else {
	if (code = karead(tt, callerIndex, &caller, sizeof(caller))) {
	    code = KAIO;
	    goto abort;
	}
	/* if the user is checking his own entry or ADMIN then go ahead. */
	callerIsAdmin = (ntohl(caller.flags) & KAFADMIN);

	if (strcmp(caller.userID.name, aname) != 0 && !callerIsAdmin) {
	    code = KANOAUTH;
	    goto abort;
	}
    }

    code = FindBlock(tt, aname, ainstance, &to, &tentry);
    if (code)
	goto abort;
    if (to == 0) {		/* entry not found */
	code = KANOENT;
	goto abort;
    }

    get_time(0, 0, 0);		/* generate random update */

    memset(aentry, 0, sizeof(*aentry));
    aentry->minor_version = KAMINORVERSION;
    aentry->flags = ntohl(tentry.flags);
    aentry->user_expiration = ntohl(tentry.user_expiration);
    aentry->modification_time = ntohl(tentry.modification_time);
    aentry->change_password_time = ntohl(tentry.change_password_time);
    aentry->max_ticket_lifetime = ntohl(tentry.max_ticket_lifetime);
    aentry->key_version = ntohl(tentry.key_version);

    temp = (unsigned char)tentry.misc_auth_bytes[LOCKTIME];
    temp = temp << 9;
    if (kaux_islocked(to, (u_int) tentry.misc_auth_bytes[ATTEMPTS], temp))
	tentry.misc_auth_bytes[REUSEFLAGS] |= KA_ISLOCKED;	/* saves an RPC */

    temp = pack_long(tentry.misc_auth_bytes);
    aentry->misc_auth_bytes = temp;
    /* 
     * only return user's key if security disabled or if admin and
     * we have an encrypted connection to the user
     */
    rxkad_GetServerInfo(call->conn, &enc_level, 0, 0, 0, 0, 0);
    if ((noAuthenticationRequired)
	|| (callerIsAdmin && enc_level == rxkad_crypt))
	memcpy(&aentry->key, &tentry.key, sizeof(struct ktc_encryptionKey));
    else
	memset(&aentry->key, 0, sizeof(aentry->key));
    code = ka_KeyCheckSum((char *)&tentry.key, &aentry->keyCheckSum);
    if (!tentry.pwsums[0] && npwSums > 1 && !tentry.pwsums[1]) {
	aentry->reserved3 = 0x12340000;
    } else {
	aentry->reserved3 = 0x12340001;
    }

    /* Now get entry of user who last modified this entry */
    if (ntohl(tentry.modification_id)) {
	temp = ntohl(tentry.modification_id);
	code = karead(tt, temp, &tentry, sizeof(tentry));
	if (code) {
	    code = KAIO;
	    goto abort;
	}
	aentry->modification_user = tentry.userID;
    } else {
	strcpy(aentry->modification_user.name, "<none>");
	strcpy(aentry->modification_user.instance, "\0");
    }
    code = ubik_EndTrans(tt);
    return code;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    return code;
}

afs_int32
SKAM_ListEntry(call, previous_index, index, count, name)
     struct rx_call *call;
     afs_int32 previous_index;	/* last entry ret'd or 0 for first */
     afs_int32 *index;		/* index of this entry */
     afs_int32 *count;		/* total entries in database */
     kaident *name;		/* name & instance of this entry */
{
    afs_int32 code;

    code = kamListEntry(call, previous_index, index, count, name);
    osi_auditU(call, AFS_KAM_LstEntEvent, code, AUD_LONG, *index, AUD_END);
    return code;
}


afs_int32
kamListEntry(call, previous_index, index, count, name)
     struct rx_call *call;
     afs_int32 previous_index;	/* last entry ret'd or 0 for first */
     afs_int32 *index;		/* index of this entry */
     afs_int32 *count;		/* total entries in database */
     kaident *name;		/* name & instance of this entry */
{
    register int code;
    struct ubik_trans *tt;
    afs_int32 caller;
    struct kaentry tentry;

    COUNT_REQ(ListEntry);
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	return code;
    code = check_auth(call, tt, 1, &caller);
    if (code) {
	goto abort;
    }

    *index = NextBlock(tt, previous_index, &tentry, count);
    if (*count < 0) {
	code = KAIO;
	goto abort;
    }

    if (*index) {		/* return name & inst of this entry */
	strncpy(name->name, tentry.userID.name, sizeof(name->name));
	strncpy(name->instance, tentry.userID.instance,
		sizeof(name->instance));
    } else {
	strcpy(name->name, "\0");
	strcpy(name->instance, "\0");
    }
    code = ubik_EndTrans(tt);
    return code;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    return code;
}

static afs_int32
GetTicket(version, call, kvno, authDomain, aticket, sname, sinstance, atimes,
	  oanswer)
     int version;
     struct rx_call *call;
     afs_int32 kvno;
     char *authDomain;
     ka_CBS *aticket;
     char *sname;
     char *sinstance;
     ka_CBS *atimes;		/* encrypted start & end time */
     ka_BBS *oanswer;
{
    afs_int32 code;
    int import, export;
    struct ubik_trans *tt;
    struct ktc_encryptionKey tgskey;
    des_key_schedule schedule;
    afs_int32 to;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char cell[MAXKTCNAMELEN];
    int celllen;
    struct kaentry caller;
    struct kaentry server;
    struct ktc_encryptionKey authSessionKey;
    struct ktc_encryptionKey sessionKey;
    int ticketLen;
    char ticket[MAXKTCTICKETLEN];
    afs_int32 host;
    Date start;
    Date expiration;
    Date now;
    Date end;
    struct ka_getTicketTimes times;
    struct ka_getTicketAnswer *answer;

    COUNT_REQ(GetTicket);
    if (!name_instance_legal(sname, sinstance))
	return KABADNAME;
    if (atimes->SeqLen != sizeof(times))
	return KABADARGUMENT;
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	return code;

    export = import = 0;
    if ((strcmp(sname, KA_TGS_NAME) == 0) && (strcmp(sinstance, lrealm) != 0))
	export = 1;
    if ((strlen(authDomain) > 0) && (strcmp(authDomain, lrealm) != 0))
	import = 1;

    if (strlen(authDomain) == 0)
	authDomain = lrealm;
    code = ka_LookupKvno(tt, KA_TGS_NAME, authDomain, kvno, &tgskey);
    if (code) {
	goto abort;
    }
    code =
	tkt_DecodeTicket(aticket->SeqBody, aticket->SeqLen, &tgskey, name,
			 instance, cell, &authSessionKey, &host, &start,
			 &expiration);
    if (code) {
	code = KANOAUTH;
	goto abort;
    }
    save_principal(tgsPrincipal, name, instance, cell);

    if (code = get_time(&now, 0, 0))
	goto abort;

    code = tkt_CheckTimes(start, expiration, now);
    if (code <= 0) {
	if (code == -1)
	    code = RXKADEXPIRED;
	else
	    code = KANOAUTH;
	goto abort;
    }
    code = des_key_sched(&authSessionKey, schedule);
    if (code) {
	code = KANOAUTH;
	goto abort;
    }
    celllen = strlen(cell);
    if (import && (celllen == 0)) {
	code = KABADTICKET;
	goto abort;
    }
    if (export && (celllen == 0))
	strcpy(cell, lrealm);

    if (!krb4_cross && celllen && strcmp(lrealm, cell) != 0) {
	code = KABADUSER;
	goto abort;
    }

    des_ecb_encrypt(atimes->SeqBody, &times, schedule, DECRYPT);
    times.start = ntohl(times.start);
    times.end = ntohl(times.end);
    code = tkt_CheckTimes(times.start, times.end, now);
    if (code < 0) {
	code = KABADREQUEST;
	goto abort;
    }

    if (import) {
	strcpy(caller.userID.name, name);
	strcpy(caller.userID.instance, instance);
	caller.max_ticket_lifetime = htonl(MAXKTCTICKETLIFETIME);
	caller.flags = htonl(KAFNORMAL);
	caller.user_expiration = htonl(NEVERDATE);
    } else {
	code = FindBlock(tt, name, instance, &to, &caller);
	if (code)
	    goto abort;
	if (to == 0) {
	    ka_PrintUserID("GetTicket: User ", name, instance, " unknown.\n");
	    code = KANOENT;
	    goto abort;
	}
    }

    /* get server's entry */
    code = FindBlock(tt, sname, sinstance, &to, &server);
    if (code)
	goto abort;
    if (to == 0) {		/* entry not found */
	ka_PrintUserID("GetTicket: Server ", sname, sinstance, " unknown.\n");
	code = KANOENT;
	goto abort;
    }
    save_principal(tgsServerPrincipal, sname, sinstance, 0);

    code = des_random_key(&sessionKey);
    if (code) {
	code = KANOKEYS;
	goto abort;
    }

    code =
	GetEndTime(times.start, times.end, expiration, &caller, &server,
		   &end);
    if (code)
	goto abort;

    code =
	tkt_MakeTicket(ticket, &ticketLen, &server.key, caller.userID.name,
		       caller.userID.instance, cell, times.start, end,
		       &sessionKey,
		       rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))),
		       server.userID.name, server.userID.instance);
    if (code)
	goto abort;

    switch (version) {
    case 0:
	code = KAANSWERTOOLONG;
	if (oanswer->MaxSeqLen <
	    sizeof(struct ka_getTicketAnswer) - 5 * MAXKTCNAMELEN -
	    MAXKTCTICKETLEN + ticketLen)
	    goto abort;

	answer = (struct ka_getTicketAnswer *)oanswer->SeqBody;
	memcpy(&answer->sessionKey, &sessionKey,
	       sizeof(struct ktc_encryptionKey));
	answer->startTime = htonl(times.start);
	answer->endTime = htonl(end);
	answer->kvno = server.key_version;
	answer->ticketLen = htonl(ticketLen);

	{
	    char *ans = answer->name;	/* ptr to variable part of answer */
	    int rem, len;

	    /* space remaining */
	    rem = oanswer->MaxSeqLen - (ans - oanswer->SeqBody);
#undef putstr
#define putstr(str) len = strlen (str)+1;\
	    if (rem < len) goto abort;\
	    strcpy (ans, str);\
	    ans += len; rem -= len

	    putstr(name);
	    putstr(instance);
	    putstr(cell);
	    putstr(sname);
	    putstr(sinstance);
	    if (rem < ticketLen)
		goto abort;
	    memcpy(ans, ticket, ticketLen);
	    oanswer->SeqLen = (ans - oanswer->SeqBody) + ticketLen;
	}
	oanswer->SeqLen = round_up_to_ebs(oanswer->SeqLen);
	break;
    case 1:
	code =
	    PrepareTicketAnswer(oanswer, /*challenge */ 0, ticket, ticketLen,
				&sessionKey, times.start, end, &caller,
				&server, cell, KA_GETTICKET_ANS_LABEL);
	if (code)
	    goto abort;
	break;
    default:
	code = KAINTERNALERROR;
	goto abort;
    }
    des_pcbc_encrypt(oanswer->SeqBody, oanswer->SeqBody, oanswer->SeqLen,
		     schedule, &authSessionKey, ENCRYPT);
    code = ubik_EndTrans(tt);
    KALOG(name, instance, sname, sinstance, (import ? authDomain : NULL),
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), LOG_GETTICKET);
    return code;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    return code;
}

afs_int32
SKAT_GetTicket_old(call, kvno, authDomain, aticket, sname, sinstance, atimes,
		   oanswer)
     struct rx_call *call;
     afs_int32 kvno;
     char *authDomain;
     ka_CBS *aticket;
     char *sname;
     char *sinstance;
     ka_CBS *atimes;		/* encrypted start & end time */
     ka_BBS *oanswer;
{
    int code;

    sleep(1);			/* strongly discourage this */
    code =
	GetTicket(0, call, kvno, authDomain, aticket, sname, sinstance,
		  atimes, oanswer);

    osi_auditU(call, AFS_KAT_GetTicketOEvent, code, AUD_STR, sname, AUD_STR,
	       sinstance, AUD_END);
    return code;
}

afs_int32
SKAT_GetTicket(call, kvno, authDomain, aticket, sname, sinstance, atimes,
	       oanswer)
     struct rx_call *call;
     afs_int32 kvno;
     char *authDomain;
     ka_CBS *aticket;
     char *sname;
     char *sinstance;
     ka_CBS *atimes;		/* encrypted start & end time */
     ka_BBS *oanswer;
{
    int code;

    code =
	GetTicket(1, call, kvno, authDomain, aticket, sname, sinstance,
		  atimes, oanswer);
    osi_auditU(call, AFS_KAT_GetTicketEvent, code, AUD_STR, sname, AUD_STR,
	       sinstance, AUD_END);
    return code;
}

afs_int32
SKAM_GetStats(call, version, admin_accounts, statics, dynamics)
     struct rx_call *call;
     afs_int32 version;
     afs_int32 *admin_accounts;
     kasstats *statics;
     kadstats *dynamics;
{
    afs_int32 code;

    code = kamGetStats(call, version, admin_accounts, statics, dynamics);
    osi_auditU(call, AFS_KAM_GetStatEvent, code, AUD_END);
    return code;
}

afs_int32
kamGetStats(call, version, admin_accounts, statics, dynamics)
     struct rx_call *call;
     afs_int32 version;
     afs_int32 *admin_accounts;
     kasstats *statics;
     kadstats *dynamics;
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 caller;

    COUNT_REQ(GetStats);
    if (version != KAMAJORVERSION)
	return KAOLDINTERFACE;
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	return code;
    code = check_auth(call, tt, 1, &caller);
    if (code) {
	COUNT_ABO;
	ubik_AbortTrans(tt);
	return code;
    }

    *admin_accounts = ntohl(cheader.admin_accounts);
    /* memcpy((char *)statics, (char *)&cheader.stats, sizeof(kasstats)); */
    /* these are stored in network byte order and must be copied */
    statics->allocs = ntohl(cheader.stats.allocs);
    statics->frees = ntohl(cheader.stats.frees);
    statics->cpws = ntohl(cheader.stats.cpws);
#if KADBVERSION != 5
    check that the statistics command copies all the fields
#endif
      memcpy((char *)dynamics, (char *)&dynamic_statistics, sizeof(kadstats));
    statics->minor_version = KAMINORVERSION;
    dynamics->minor_version = KAMINORVERSION;

    {
	int used = 0;
	int i;

	for (i = 0; i < HASHSIZE; i++)
	    if (cheader.nameHash[i])
		used++;
	dynamics->hashTableUtilization =
	    (used * 10000 + HASHSIZE / 2) / HASHSIZE;
    }
    {
#if !defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_NT40_ENV)
	struct rusage ru;
	/* Unfortunately, although aix_22 has a minimal compatibility
	 * method of getting to some rusage fields (i.e. stime &
	 * utime), the version that we have doesn't even have the
	 * related include files needed for the aix vtimes() call; so
	 * ignore this for aix till v3.1... */
	getrusage(RUSAGE_SELF, &ru);
#if (KAMAJORVERSION>5)
	memcpy(&dynamics->utime, &ru.ru_utime, sizeof(struct katimeval));
	memcpy(&dynamics->stime, &ru.ru_stime, sizeof(struct katimeval));
	dynamics->dataSize = ru.ru_idrss;
	dynamics->stackSize = ru.ru_isrss;
	dynamics->pageFailts = ru.ru_majflt;
#else
	dynamics->string_checks =
	    (afs_int32) (1000.0 *
			 ((ru.ru_utime.tv_sec +
			   ru.ru_utime.tv_usec / 1000000.0) +
			  (ru.ru_stime.tv_sec +
			   ru.ru_stime.tv_usec / 1000000.0)));
#endif
#endif /* AFS_AIX_ENV && AFS_HPUX_ENV && AFS_NT40_ENV */
    }

    code = ubik_EndTrans(tt);
    return code;
}

afs_int32
SKAM_GetPassword(call, name, password)
     struct rx_call *call;
     char *name;
     EncryptionKey *password;
{
    afs_int32 code;

    code = kamGetPassword(call, name, password);
    osi_auditU(call, AFS_KAM_GetPswdEvent, code, AUD_STR, name, AUD_END);
    return code;
}

afs_int32
kamGetPassword(call, name, password)
     struct rx_call *call;
     char *name;
     EncryptionKey *password;
{
    int code = KANOAUTH;
    COUNT_REQ(GetPassword);

#ifdef GETPASSWORD
    {
	afs_int32 to;
	struct ubik_trans *tt;
	struct kaentry tentry;

	if (!name_instance_legal(name, ""))
	    return KABADNAME;
	/* only requests from this host work */
	if (rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))) !=
	    htonl(INADDR_LOOPBACK))
	    return KANOAUTH;
	if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	    return code;

	/* this isn't likely to be used because of string to key problems, so since
	 * this is a temporary thing anyway, we'll use it here. */
	{
	    extern char udpAuthPrincipal[256];

	    save_principal(udpAuthPrincipal, name, 0, 0);
	}

	get_time(0, 0, 0);	/* update random value */
	code = FindBlock(tt, name, "", &to, &tentry);
	if (code)
	    goto abort;
	if (to == 0) {
	    code = KANOENT;
	  abort:
	    COUNT_ABO;
	    ubik_AbortTrans(tt);
	    return code;
	}

	memcpy(password, &tentry.key, sizeof(*password));
	code = ubik_EndTrans(tt);
    }
#endif
    return code;
}

afs_int32
SKAM_GetRandomKey(call, key)
     struct rx_call *call;
     EncryptionKey *key;
{
    afs_int32 code;

    code = kamGetRandomKey(call, key);
    osi_auditU(call, AFS_KAM_GetRndKeyEvent, code, AUD_END);
    return code;
}

afs_int32
kamGetRandomKey(call, key)
     struct rx_call *call;
     EncryptionKey *key;
{
    int code;

    COUNT_REQ(GetRandomKey);
    if (code = AwaitInitialization())
	return code;
    code = des_random_key(key);
    if (code)
	return KANOKEYS;
    return 0;
}

afs_int32
SKAM_Debug(call, version, checkDB, info)
     struct rx_call *call;
     afs_int32 version;
     int checkDB;		/* start a transaction to examine DB */
     struct ka_debugInfo *info;
{
    afs_int32 code;

    code = kamDebug(call, version, checkDB, info);
    osi_auditU(call, AFS_KAM_DbgEvent, code, AUD_END);
    return code;
}

afs_int32
kamDebug(call, version, checkDB, info)
     struct rx_call *call;
     afs_int32 version;
     int checkDB;		/* start a transaction to examine DB */
     struct ka_debugInfo *info;
{
/*  COUNT_REQ (Debug); */
    if (sizeof(struct kaentry) != sizeof(struct kaOldKeys))
	return KAINTERNALERROR;
    if (sizeof(struct ka_cpwRequest) % 8)
	return KAINTERNALERROR;
    if (version != KAMAJORVERSION)
	return KAOLDINTERFACE;

    memset(info, 0, sizeof(*info));

    info->minorVersion = KAMINORVERSION;
    info->host = dynamic_statistics.host;
    info->startTime = dynamic_statistics.start_time;
    info->
#if (KAMAJORVERSION>5)
	now
#else
	reserved1
#endif
	= time(0);
    info->noAuth = noAuthenticationRequired;

    info->dbVersion = ntohl(cheader.version);
    info->dbFreePtr = ntohl(cheader.freePtr);
    info->dbEofPtr = ntohl(cheader.eofPtr);
    info->dbKvnoPtr = ntohl(cheader.kvnoPtr);
    info->dbSpecialKeysVersion = ntohl(cheader.specialKeysVersion);

    info->dbHeaderRead = cheaderReadTime;
    info->lastTrans = lastTrans;
    if (!lastOperation)
	lastOperation = "(Not Available)";
    strncpy(info->lastOperation, lastOperation, sizeof(info->lastOperation));
    strncpy(info->lastAuth, authPrincipal, sizeof(info->lastAuth));
    strncpy(info->lastTGS, tgsPrincipal, sizeof(info->lastTGS));
    strncpy(info->lastAdmin, adminPrincipal, sizeof(info->lastAdmin));
    strncpy(info->lastTGSServer, tgsServerPrincipal,
	    sizeof(info->lastTGSServer));
    {
	extern char udpAuthPrincipal[256];
	extern char udptgsPrincipal[256];
	extern char udptgsServerPrincipal[256];

	strncpy(info->lastUAuth, udpAuthPrincipal, sizeof(info->lastUAuth));
	strncpy(info->lastUTGS, udptgsPrincipal, sizeof(info->lastUTGS));
	strncpy(info->lastUTGSServer, udptgsServerPrincipal,
		sizeof(info->lastUTGSServer));
    }
    info->nextAutoCPW = nextAutoCPWTime;
    info->updatesRemaining = autoCPWUpdates - totalUpdates;
    ka_debugKeyCache(info);
    return 0;
}

/* these are auxiliary routines. They don't do any Ubik stuff.  They use 
 * a tacked-on-the-side data file.
 * prob'ly ought to check the noauth flag.
 */
#define ABORTIF(A) {if(code= A){goto abort;}}
afs_int32
SKAM_Unlock(call, aname, ainstance, spare1, spare2, spare3, spare4)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 spare1, spare2, spare3, spare4;
{
    register int code;
    struct ubik_trans *tt;
    afs_int32 caller;
    afs_int32 to;
    struct kaentry tentry;

    COUNT_REQ(Unlock);
    if (!name_instance_legal(aname, ainstance)) {
	code = KABADNAME;
	goto exit;
    }
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	goto exit;

    ABORTIF(check_auth(call, tt, 1, &caller));
    ABORTIF(FindBlock(tt, aname, ainstance, &to, &tentry));
    ABORTIF((to == 0 ? KANOENT : 0));

    kaux_write(to, 0, 0);	/* zero failure counters at this offset */

    code = ubik_EndTrans(tt);
    KALOG(aname, ainstance, NULL, NULL, NULL,
	  rx_HostOf(rx_PeerOf(rx_ConnectionOf(call))), LOG_UNLOCK);
    goto exit;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);

  exit:
    osi_auditU(call, UnlockEvent, code, AUD_STR, aname, AUD_STR, ainstance,
	       AUD_END);
    return code;
}

afs_int32
SKAM_LockStatus(call, aname, ainstance, lockeduntil, spare1, spare2, spare3,
		spare4)
     struct rx_call *call;
     char *aname;
     char *ainstance;
     afs_int32 *lockeduntil;
     afs_int32 spare1, spare2, spare3, spare4;
{
    register int code;
    struct ubik_trans *tt;
    afs_int32 callerIndex;
    afs_int32 to;
    struct kaentry caller;
    struct kaentry tentry;
    afs_uint32 temp;

    COUNT_REQ(LockStatus);

    if (!name_instance_legal(aname, ainstance)) {
	code = KABADNAME;
	goto exit;
    }
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	goto exit;

    if (code = check_auth(call, tt, 0, &callerIndex))
	goto abort;

    if (!noAuthenticationRequired && callerIndex) {
	if (karead(tt, callerIndex, &caller, sizeof(caller))) {
	    code = KAIO;
	    goto abort;
	}
	/* if the user is checking his own entry or ADMIN then go ahead. */
	if ((strcmp(caller.userID.name, aname) != 0)
	    && !(ntohl(caller.flags) & KAFADMIN)) {
	    code = KANOAUTH;
	    goto abort;
	}
    }

    if (code = FindBlock(tt, aname, ainstance, &to, &tentry))
	goto abort;

    if (to == 0) {
	code = KANOENT;
	goto abort;
    }

    temp = (unsigned char)tentry.misc_auth_bytes[LOCKTIME];
    temp = temp << 9;
    *lockeduntil =
	kaux_islocked(to, (u_int) tentry.misc_auth_bytes[ATTEMPTS], temp);

    code = ubik_EndTrans(tt);
    goto exit;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);
    osi_auditU(call, LockStatusEvent, code, AUD_STR, aname, AUD_STR,
	       ainstance, AUD_END);

  exit:
    return code;
}
