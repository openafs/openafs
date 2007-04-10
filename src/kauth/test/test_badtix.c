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
#include <des.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <lock.h>
#include <ubik.h>
#include <lwp.h>
#include "kauth.h"
#include "kautils.h"

char *whoami;
char *localCell;
char name[MAXKTCNAMELEN] = "guest";
char inst[MAXKTCNAMELEN] = "";

char aname[] = "AuthServer";
char ainst[] = "Admin";

static
print_entry(tentry, name, instance)
     struct kaentryinfo *tentry;
     char *name;
     char *instance;
{
    Date now = time(0);
    char bob[KA_TIMESTR_LEN];

    if (tentry->minor_version != KAMINORVERSION)
	printf("Minor version number mismatch: got %d, expected %d\n",
	       tentry->minor_version, KAMINORVERSION);
    ka_PrintUserID("User data for ", name, instance, "");
    {
	char *prefix = " (";
#define NEWPREFIX "+"
	if (tentry->flags & KAFADMIN) {
	    printf("%sADMIN", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry->flags & KAFNOTGS) {
	    printf("%sNOTGS", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry->flags & KAFNOCPW) {
	    printf("%sNOCPW", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry->flags & KAFNOSEAL) {
	    printf("%sNOSEAL", prefix);
	    prefix = NEWPREFIX;
	}
	if (tentry->user_expiration <= now) {
	    printf("%sexpired", prefix);
	    prefix = NEWPREFIX;
	}
	if (strcmp(prefix, NEWPREFIX) == 0)
	    printf(")\n");
	else
	    printf("\n");
    }
    printf("  key (%d):", tentry->key_version);
    ka_PrintBytes(&tentry->key, sizeof(tentry->key));
    ka_timestr(tentry->change_password_time, bob, KA_TIMESTR_LEN);
    printf(", last cpw: %s\n", bob);
    ka_timestr(tentry->user_expiration, bob, KA_TIMESTR_LEN);
    printf("  entry expires on %s.  Max ticket lifetime %.2f hours.\n", bob,
	   tentry->max_ticket_lifetime / 3600.0);
    ka_timestr(tentry->modification_time, bob, KA_TIMESTR_LEN);
    printf("  last mod on %s by ", bob);
    ka_PrintUserID("", tentry->modification_user.name,
		   tentry->modification_user.instance, "\n");
}

int
sign(x)
     int x;
{
    if (x < 0)
	return -1;
    if (x > 0)
	return 1;
    return 0;
}

void
TestOldKeys(userkey)
     struct ktc_encryptionKey *userkey;
{

#define MAXADMINTOKENS 30
#define MAXTGSTOKENS 10
    int nAdminTokens, nTGSTokens;
    struct ktc_token adminTokens[MAXADMINTOKENS];
    struct ktc_token tgsTokens[MAXTGSTOKENS];

    /* depends on 10 updates in 10 seconds to get AutoCPW to trigger,
     * also 90 seconds is the maximum password age,
     * also 11 old keys per block. */

    static int OKTestVector[] = {
	0, 1000, 3000, 1005, 1005, 1005, 1006, 1006, 1007, 1007, 1007, 1007,
	1008,
	1011, 3012, 3012, 1015, 3017, 3017, 3017,
	1021, 2024, 4024, 13026, 1028, 1028, 1028, 1028, 1028, 1028, 1028,
	1028,
	1031, 1031, 13032, 13032, 13032, 13032, 13032, 13032, 2037, 4037,
	13040, 13040, 13041, 13041, 13042, 13042, 2049, 4049,
	13050, 13051, 13052, 54, 55, 56,
	2061, 4061, 65, 65, 65, 65, 65, 65, 65, 65, 65,
	2073, 4073, 13073, 13073, 13074, 13075, 77,
	2085, 4085, 85, 85, 85, 85, 85, 85, 85,
	13093,
	2100, 1103 /* should change primary on aa */ , 4105,
	13111
    };
    int nOKTestVector = sizeof(OKTestVector) / sizeof(int);

    long code;
    struct ktc_token tgt;
    struct ktc_token atoken;
    struct ubik_client *aconn;	/* connection to authentication service */
    struct ubik_client *tgsConn;	/* connection to TGS */
    struct ubik_client *adminConn;	/* connection to maintenance service */
    struct ktc_token ttoken;
    struct kaentryinfo aentry;
    struct ka_debugInfo info;
    Date now = time(0);
    Date start;
    int notify;
    int i;
    struct ktc_encryptionKey key;
    char *aaname = "krbtgt";
    char *aainst = "FAKECELL.EDU";

    printf("checking old keys\n");

    /* get tickets */
    if ((code =
	 ka_AuthServerConn(localCell, KA_AUTHENTICATION_SERVICE, 0, &aconn))
	|| (code =
	    ka_Authenticate(name, inst, localCell, aconn,
			    KA_TICKET_GRANTING_SERVICE, userkey, now,
			    now + 3600, &tgt, 0))
	|| (code =
	    ka_Authenticate(name, inst, localCell, aconn,
			    KA_MAINTENANCE_SERVICE, userkey, now, now + 3600,
			    &atoken, 0))
	|| (code =
	    ka_AuthServerConn(localCell, KA_TICKET_GRANTING_SERVICE, 0,
			      &tgsConn))
	|| (code =
	    ka_AuthServerConn(localCell, KA_MAINTENANCE_SERVICE, &atoken,
			      &adminConn))
	) {
      abort:
	afs_com_err(whoami, code, "testing old keys");
	exit(1);
    }

    code = ubik_Call(KAM_GetRandomKey, adminConn, 0, &key);
    if (code)
	goto abort;
    code = ubik_Call(KAM_CreateUser, adminConn, 0, aaname, aainst, key);
    if (code == KAEXIST)
	printf("Alternate Admin User already exists\n");
    else if (code)
	goto abort;

    nAdminTokens = nTGSTokens = 0;
    start = time(0);
    notify = 10;
    for (i = 0; i < nOKTestVector; i++) {
	int v = OKTestVector[i];	/* get test vector */
	int sleep;

	sleep = v % 1000;
	v = v / 1000;

	if ((now = time(0)) < start + sleep)
	    IOMGR_Sleep(start + sleep - now);
	else
	    IOMGR_Poll();
	now = time(0);
	if (sleep >= notify) {
	    printf("Now at %d seconds\n", sleep);
	    notify = sleep + 10;
	}

	switch (v) {
	case 1:		/* set krbtgt.FAKECELL.EDU password */
	    code = ubik_Call(KAM_GetRandomKey, adminConn, 0, &key);
	    if (code)
		goto abort_1;
	    code =
		ubik_Call(KAM_SetPassword, adminConn, 0, aaname, aainst, 0,
			  key);
	    break;
	case 3:		/* set AuthServer.Admin password */
	case 13:		/* and remember admin ticket */
	    code = ubik_Call(KAM_GetRandomKey, adminConn, 0, &key);
	    if (code)
		goto abort_1;
	    code =
		ubik_Call(KAM_SetPassword, adminConn, 0, aname, ainst, 0,
			  key);
	    if (v == 3)
		break;
	case 4:		/* remeber Admin ticket and TGS ticket */
	    if (nAdminTokens >= MAXADMINTOKENS)
		printf("Too many admin tokens\n");
	    else
		code =
		    ka_Authenticate(name, inst, localCell, aconn,
				    KA_MAINTENANCE_SERVICE, userkey, now,
				    now + 3600, &adminTokens[nAdminTokens++],
				    0);
	    if (code)
		goto abort_1;
	    if (v != 4)
		break;
	    if (nTGSTokens >= MAXTGSTOKENS)
		printf("Too many tgs tokens\n");
	    else
		code =
		    ka_Authenticate(name, inst, localCell, aconn,
				    KA_TICKET_GRANTING_SERVICE, userkey, now,
				    now + 3600, &tgsTokens[nTGSTokens++], 0);
	    break;
	case 2:
	    code =
		ubik_Call(KAM_Debug, adminConn, 0, KAMAJORVERSION, 0, &info);
	    if (code)
		goto abort_1;
	    now = time(0);
	    printf
		("Now at %d seconds (really %d): %d updates and %d seconds remaining\n",
		 sleep, (now - start), info.updatesRemaining,
		 info.nextAutoCPW - now);
	    if (info.updatesRemaining > 1)
		printf("Too many updates needed at time %d\n", sleep);
	    while ((now = time(0)) < info.nextAutoCPW) {
		printf("...waiting for next auto CPW\n");
		if (info.nextAutoCPW - now > 1)
		    IOMGR_Sleep(1);
		else
		    IOMGR_Poll();
	    }
	    code =
		ubik_Call(KAM_SetFields, adminConn, 0, name, inst, 0, 0,
			  100 * 3600, 0, /* spares */ 0,
			  0);
	    break;
	case 0:
	    code =
		ubik_Call(KAM_GetEntry, adminConn, 0, aname, ainst,
			  KAMAJORVERSION, &aentry);
	    break;
	}
	if (code) {
	  abort_1:
	    afs_com_err(whoami, code, "at %d seconds: calling server with v=%x",
		    sleep, v);
	    exit(2);
	}
    }

    printf("Trying %d Admin tokens\n", nAdminTokens);
    for (i = 0; i < nAdminTokens; i++) {
	int j;
	struct ubik_client *conn;

	for (j = i + 1; j < nAdminTokens; j++)
	    if (adminTokens[i].kvno == adminTokens[j].kvno)
		printf("Two admin tokens with kvno %d: %d and %d\n",
		       (int)adminTokens[i].kvno, i, j);

	code =
	    ka_AuthServerConn(localCell, KA_MAINTENANCE_SERVICE,
			      &adminTokens[i], &conn);
	if (code) {
	  abort_ta:
	    afs_com_err(whoami, code, "Checking admin token #%d with kvno %d\n",
		    i, (int)adminTokens[i].kvno);
	    exit(5);
	}
	code =
	    ubik_Call(KAM_GetEntry, conn, 0, aname, ainst, KAMAJORVERSION,
		      &aentry);
	if (code)
	    goto abort_ta;
    }

    printf("Trying %d TGS tokens\n", nTGSTokens);
    for (i = 0; i < nTGSTokens; i++) {
	int j;
	struct ktc_token token;

	for (j = i + 1; j < nTGSTokens; j++)
	    if (tgsTokens[i].kvno == tgsTokens[j].kvno)
		printf("Two tgs tokens with kvno %d: %d and %d\n",
		       (int)tgsTokens[i].kvno, i, j);

	code =
	    ka_GetToken(name, inst, localCell, name, inst, tgsConn, now,
			now + 3600, &tgsTokens[i], "", &token);
	if (code) {
	    afs_com_err(whoami, code, "Checking tgs token #%d with kvno %d\n", i,
		    (int)tgsTokens[i].kvno);
	    exit(6);
	}
    }

    code = ubik_Call(KAM_DeleteUser, adminConn, 0, aaname, aainst);
    if (code) {
	afs_com_err(whoami, code, "Deleting alternate admin user");
	exit(3);
    }
    return;
}

int
main(argc, argv)
     int argc;
     char *argv[];
{
    int i, j;
    long serverList[MAXSERVERS];
    int code;
    char *args[3];
    struct ktc_encryptionKey key;
    struct ktc_token tgt;
    struct ktc_token token;
    struct ktc_token atoken;
    struct ubik_client *aconn;	/* connection to authentication service */
    struct ubik_client *conn;
    struct ubik_client *lpbkConn = 0;
    struct kaentryinfo tentry;
    Date now = time(0);
    Date end;
    char password[BUFSIZ];
#if (BUFSIZ<=1000)
    password needs to be at least 1000 chars long;
#endif
    static char source[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0987654321";
    static int truncate[] =
	{ 1000, 750, 562, 432, 316, 237, 178, 133, 100, 75, 56, 42, 32, 24,
	18, 13, 10, 8, 6, 4, 3, 2, 1, 0
    };
    static char answer[sizeof(truncate) / sizeof(int)][32] =
	{ "\250y\3161\315\250\361\274", "\212\133\133\345\236\206\362\200",
	"\230b\354\334\302F\352\040", "\001\221T\016\265T\376\100",
	"X\002\224g\250\221\214\340", "\241p\304\032\135\0328\361",
	"\010\352\037\236sp\046\212", "\3522\352\325\247\313\023W",
	"\023\310\034\315\265\227L\203", "z\373RJ\233\304\046m",
	"\310\244\032\375\3704\045\323", "n\224\3022\034\376\013\247",
	"\217\343\302\364\177\277\052\214",
	"\357\337\010\054sv\332\057", "\373\373\316u\247\302\362g",
	"\214O\007m\320\221\301\174", "\002\057OQ\031p\370u",
	"k\224s\235\362\247\230\206", "\364\233\334\211\235\217k\205",
	"\352\203m\212s\337\211d",
	"k\354\256\364\222\352\265\323", "\205\135d\351\326\334\260\313",
	"\345\250\331\340\265hd\222",
	"\346\265\352\136\316\205\217\313"
    };
    struct ktc_encryptionKey correct_key;
    char hostname[64];

    whoami = argv[0];
    args[0] = "";
    args[1] = "-servers";

    /* get this host */
    gethostname(hostname, sizeof(hostname));
    args[2] = hostname;

    /* test string hacking */
    if ((strcmp(lcstring(name, "AbcDe", 5), "abcd") != 0)
	|| (strcmp(lcstring(name, "AbcDe", 6), "abcde") != 0)
	|| (strcmp(ucstring(inst, "AbcDe", 4), "ABC") != 0)
	|| (strcmp(ucstring(inst, "AbcDe", sizeof(inst)), "ABCDE") != 0)) {
	printf("uc/lc string problem\n");
	exit(1);
    }
#define tryscc(A,B,a,b) (sign(strcasecmp (A,B)) != sign (strcmp (a,b)))
    if (tryscc("Abc", "abc", "abc", "abc") || tryscc("aB", "ABC", "ab", "abc")
	|| tryscc("ABC", "ab", "abc", "ab")
	|| tryscc("Abcd", "aBg", "abcd", "abg")) {
	printf("strcasecmp problem\n");
	exit(1);
    }

    strcpy(password, "");
    for (i = 0; i < 100; i++)
	for (j = 0; j < 10; j++)
	    password[i * 10 + j] = source[i % (sizeof(source) - 1)];

    code = ka_CellConfig(AFSCONF_CLIENTNAME);
    if (code)
	afs_com_err(whoami, code, "calling cell config");
    localCell = ka_LocalCell();

    for (i = 0; i < (sizeof(truncate) / sizeof(int)); i++) {
	password[truncate[i]] = 0;
	ka_StringToKey(password, "andy.edu", &key);
	ka_ReadBytes(answer[i], &correct_key, sizeof(key));
	if (memcmp(&key, &correct_key, sizeof(key)) != 0) {
	    printf
		("String to key error converting '%s'; should be '%s' instead got '",
		 password, answer[i]);
	    ka_PrintBytes(&key, sizeof(key));
	    printf("'\n");
	    exit(1);
	}
    }
    memset(password, 0, sizeof(password));
    j = 0;			/* current password length */
    for (i = (sizeof(truncate) / sizeof(int)) - 1; i >= 0; i--) {
	while (j < truncate[i]) {
	    password[j] = source[j / 10 % (sizeof(source) - 1)];
	    j++;
	}
	ka_StringToKey(password, "andy.edu", &key);
	ka_ReadBytes(answer[i], &correct_key, sizeof(key));
	if (memcmp(&key, &correct_key, sizeof(key)) != 0) {
	    printf
		("String to key error converting '%s'; should be '%s' instead got '",
		 password, answer[i]);
	    ka_PrintBytes(&key, sizeof(key));
	    printf("'\n");
	}
    }

    strcpy(name, "guest");
    strcpy(inst, "");

    code = rx_Init(0);
    if (code) {
	afs_com_err(whoami, code, "rx_Init'ing");
	exit(1);
    }
    if (code = ka_Init(0)) {
	afs_com_err(whoami, code, "ka_Init'ing");
	exit(1);
    }
    if (code = ubik_ParseClientList(3, args, serverList)) {
	afs_com_err(whoami, code, "parsing Ubik server list");
	exit(1);
    }
    ka_ExplicitCell(localCell, serverList);

    {
	struct rx_connection *conns[2];
	struct rx_securityClass *sc;
	int si;			/* security class index */

	sc = rxnull_NewClientSecurityObject();
	si = RX_SCINDEX_NULL;
	conns[0] =
	    rx_NewConnection(htonl(INADDR_LOOPBACK), htons(AFSCONF_KAUTHPORT),
			     KA_MAINTENANCE_SERVICE, sc, si);
	conns[1] = 0;
	code = ubik_ClientInit(conns, &lpbkConn);
	if (code) {
	  abort_4:
	    afs_com_err(whoami, code,
		    "getting %s's password via loopback connection to GetPassword",
		    name);
	    exit(1);
	}
	code = ubik_Call(KAM_GetPassword, lpbkConn, 0, name, &key);
	if (code == KANOAUTH) {
	    printf("GetPassword disabled\n");
	    ka_StringToKey(name, localCell, &key);
	} else if (code)
	    goto abort_4;
    }

    /* first just get TGS ticket */
    code = ka_AuthServerConn(localCell, KA_AUTHENTICATION_SERVICE, 0, &aconn);
    if (code) {
      abort:
	afs_com_err(whoami, code, "connecting to authentication service");
	exit(1);
    }
    end = now + 100 * 3600 + 2;
    code =
	ka_Authenticate(name, inst, localCell, aconn,
			KA_TICKET_GRANTING_SERVICE, &key, now, end, &tgt, 0);
    if (code)
	goto abort;
    if (tgt.endTime == end) {
	fprintf(stderr,
		"*** AuthTicket expires too late: must be old style sever ***\n");
    } else if (tgt.endTime != now + 100 * 3600) {
	fprintf(stderr, "Bogus expiration because lifetime (%d) wrong\n",
		tgt.endTime - tgt.startTime);
	exit(1);
    }

    /* try to get ticket w/ time jitter */
    code =
	ka_Authenticate(name, inst, localCell, aconn, KA_MAINTENANCE_SERVICE,
			&key, now + KTC_TIME_UNCERTAINTY / 2, now + 3600,
			&token, 0);
    if (code) {
      abort_1:
	afs_com_err(whoami, code, "using admin ticket with time jitter");
	exit(1);
    }

    code =
	ka_AuthServerConn(localCell, KA_MAINTENANCE_SERVICE, &token, &conn);
    if (code)
	goto abort_1;
    code =
	ubik_Call(KAM_GetEntry, conn, 0, name, inst, KAMAJORVERSION, &tentry);
    if (code)
	goto abort_1;
    print_entry(&tentry, name, inst);

    {
	struct ktc_encryptionKey badkey;

	memcpy(&badkey, &key, sizeof(badkey));
	*(int *)&badkey ^= 1;	/* toggle some bit */
	code = ubik_Call(KAM_SetPassword, conn, 0, name, inst, 0, badkey);
	if (code != KABADKEY) {
	  abort_5:
	    afs_com_err(whoami, code, "Trying to set bad key");
	    exit(1);
	}
	memset(&badkey, 0, sizeof(badkey));
	code = ubik_Call(KAM_SetPassword, conn, 0, name, inst, 0, badkey);
	if (code != KABADKEY)
	    goto abort_5;
	code = ubik_Call(KAM_SetPassword, conn, 0, name, inst, 9999, key);
	if (code != KABADARGUMENT)
	    goto abort_5;
    }

    /* try using ticket with no expiration time */
    {
	struct ktc_encryptionKey akey;
	struct kaentryinfo aentry;

	ka_StringToKey("authserv", localCell, &akey);

	code = ubik_Call(KAM_SetPassword, conn, 0, aname, ainst, 0, akey);
	if (code) {
	  abort_6:
	    afs_com_err(whoami, code, "Checking SetPassword");
	    exit(2);
	}
	code =
	    ubik_Call(KAM_GetEntry, conn, 0, aname, ainst, KAMAJORVERSION,
		      &aentry);
	if (code)
	    goto abort_6;
	atoken.kvno = aentry.key_version;
	for (i = 0; i < sizeof(aentry.key); i++)
	    if (((char *)&aentry.key)[i]) {
		code = KABADKEY;
		goto abort_6;
	    }
	code = ubik_Call(KAM_GetRandomKey, conn, 0, &atoken.sessionKey);
	if (code)
	    goto abort_3;
	printf("Got random sessionKey: ");
	ka_PrintBytes(&atoken.sessionKey, sizeof(key));
	printf("\n");

	atoken.startTime = 0;
	atoken.endTime = NEVERDATE;
	code =
	    tkt_MakeTicket(atoken.ticket, &atoken.ticketLen, &akey, name,
			   inst, "", atoken.startTime, atoken.endTime,
			   &atoken.sessionKey, 0, aname, ainst);
	if (code) {
	  abort_3:
	    afs_com_err(whoami, code, "faking up AuthServer ticket");
	    exit(1);
	}
	{
	    struct ktc_principal client;
	    struct ktc_encryptionKey sessionkey;
	    Date start, end;
	    long host;

	    code =
		tkt_DecodeTicket(atoken.ticket, atoken.ticketLen, &akey,
				 client.name, client.instance, client.cell,
				 &sessionkey, &host, &start, &end);
	    if (code)
		goto abort_3;
	    if (code = tkt_CheckTimes(start, end, time(0)) <= 0)
		goto abort_3;

	    if (!des_check_key_parity(&sessionkey)
		|| des_is_weak_key(&sessionkey)) {
		code = KABADKEY;
		goto abort_3;
	    }
	}
    }

    code =
	ka_AuthServerConn(localCell, KA_MAINTENANCE_SERVICE, &atoken, &conn);
    if (code)
	goto abort_3;
    {
	struct kaentryinfo entry;

	code =
	    ubik_Call(KAM_GetEntry, conn, 0, name, inst, KAMAJORVERSION,
		      &entry);
	if (code)
	    goto abort_3;
	if (memcmp(&tentry, &entry, sizeof(entry)) != 0) {
	    printf("Entries obtained not the same!\n");
	    print_entry(&entry, name, inst);
	}
    }

    /* try bashing a ticket to make sure it fails to work */
    memset(atoken.ticket + 10, 0, 1);
    code =
	ka_AuthServerConn(localCell, KA_MAINTENANCE_SERVICE, &atoken, &conn);
    if (code) {
	afs_com_err(whoami, code, "contacting admin server with bashed ticket");
	exit(0);		/* this is supposed to happen */
    }
    code =
	ubik_Call(KAM_GetEntry, conn, 0, name, inst, KAMAJORVERSION, &tentry);
    if (code != RXKADBADTICKET) {
	afs_com_err(whoami, code,
		"GetEntry failed to fail even with damaged ticket!!!!\n");
	exit(1);
    }

    TestOldKeys(&key);

    printf("All clear!\n");
    if (argc == 2) {
	code = setpag();
	if (code)
	    afs_com_err(whoami, code, "calling SetPAG");
	else
	    printf("Calling SetPAG and exec'ing %s\n", argv[1]);
	execve(argv[1], argv + 1, 0);
	perror("execve returned");
    }
    exit(0);
}
