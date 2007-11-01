/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* ticket caching code */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/stds.h"
#include "afs/pthread_glock.h"
#include "afs/vice.h"
#include "afs/auth.h"
#include "afs/venus.h"
#include "afs/pthread_glock.h"
#include "afs/dirpath.h"

#if !defined(min)
#define min(a,b) ((a)<(b)?(a):(b))
#endif /* !defined(min) */

#else /* defined(UKERNEL) */

#ifdef	AFS_SUN5_ENV
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <string.h>
#include <afs/vice.h>
#ifdef	AFS_AIX_ENV
#include <sys/lockf.h>
#ifdef AFS_AIX51_ENV
#include <sys/cred.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "auth.h"
#include <afs/venus.h>
#include <afs/afsutil.h>

#endif /* defined(UKERNEL) */

/* For malloc() */
#include <stdlib.h>
#include "ktc.h"

#ifdef	notdef
/* AFS_KERBEROS_ENV is now conditionally defined in the Makefile */
#define AFS_KERBEROS_ENV
#endif

#ifdef AFS_KERBEROS_ENV
#include <fcntl.h>
#include <sys/file.h>
#include "cellconfig.h"
static char lcell[MAXCELLCHARS];

#define TKT_ROOT "/tmp/tkt"

#define KSUCCESS 0
#define KFAILURE 255

/* Definitions for ticket file utilities */
#define	R_TKT_FIL	0
#define	W_TKT_FIL	1

/* Error codes returned by ticket file utilities */
#define		NO_TKT_FIL	76	/* No ticket file found */
#define		TKT_FIL_ACC	77	/* Couldn't access tkt file */
#define		TKT_FIL_LCK	78	/* Couldn't lock ticket file */
#define		TKT_FIL_FMT	79	/* Bad ticket file format */
#define		TKT_FIL_INI	80	/* afs_tf_init not called first */

/* Values returned by get_credentials */
#define		RET_TKFIL      21	/* Can't read ticket file */

#ifndef BUFSIZ
#define BUFSIZ 4096
#endif

#ifdef	AFS_HPUX_ENV
#include <unistd.h>
#endif
#if	defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV)
static struct flock fileWlock = { F_WRLCK, 0, 0, 0, 0, 0 };
static struct flock fileRlock = { F_RDLCK, 0, 0, 0, 0, 0 };
static struct flock fileUlock = { F_UNLCK, 0, 0, 0, 0, 0 };
#endif
#ifdef AFS_HPUX_ENV
static struct flock fileWlock = { F_WRLCK, 0, 0, 0, 0 };
static struct flock fileRlock = { F_RDLCK, 0, 0, 0, 0 };
static struct flock fileUlock = { F_UNLCK, 0, 0, 0, 0 };
#endif

#ifndef EOF
#define EOF (-1)
#endif

/* the following routines aren't static anymore on behalf of the kerberos IV
 * compatibility library built in subtree krb.
 */
int afs_tf_init(char *, int);
int afs_tf_get_pname(char *);
int afs_tf_get_pinst(char *);
int afs_tf_get_cred(struct ktc_principal *, struct ktc_token *);
int afs_tf_save_cred(struct ktc_principal *, struct ktc_token *, 
		     struct ktc_principal *);
int afs_tf_close(void);
int afs_tf_create(char *, char *);
int afs_tf_dest_tkt(void);
static void ktc_LocalCell(void);
#endif /* AFS_KERBEROS_ENV */

#ifdef AFS_DUX40_ENV
#define PIOCTL afs_pioctl
#elif defined(UKERNEL)
#define PIOCTL(A,B,C,D) call_syscall(AFSCALL_PIOCTL,A,B,C,D)
#else
#define PIOCTL pioctl
#endif


#ifdef KERNEL_KTC_COMPAT

#ifndef KTC_SYSCALL
#define KTC_SYSCALL 	32
#endif

/* Kernel call opcode definitions */
#define KTC_OPCODE_BASE		4300
#define KTC_NO_OP		(0+KTC_OPCODE_BASE)
#define KTC_SETTOKEN_OP 	(1+KTC_OPCODE_BASE)
#define KTC_GETTOKEN_OP 	(2+KTC_OPCODE_BASE)
#define KTC_LISTTOKENS_OP 	(3+KTC_OPCODE_BASE)
#define KTC_FORGETTOKEN_OP 	(4+KTC_OPCODE_BASE)
#define KTC_FORGETALLTOKENS_OP  (5+KTC_OPCODE_BASE)
#define KTC_STATUS_OP		(6+KTC_OPCODE_BASE)
#define KTC_GC_OP		(7+KTC_OPCODE_BASE)

#define KTC_INTERFACE_VERSION 3

/* We have to determine if the kernel supports the ktc system call.  To do so
 * we attempt to execute its noop function.  If this is successful we use the
 * kernel calls in the future otherwise we let the old code run. */

/* To safely check to see whether a system call exists we have to intercept the
 * SIGSYS signal which is caused by executing a non-existant system call.  If
 * it is ignored the syscall routine returns EINVAL.  The SIGSYS is reset to
 * its old value after the return from syscall.  */

static int kernelKTC = 0;

#ifdef	AFS_DECOSF_ENV
/*
 * SIGSYS semantics are broken on Dec AXP OSF/1 v1.2 systems.  You need
 * to ignore SIGTRAP too.  It is claimed to be fixed under v1.3, but...
 */

#define CHECK_KERNEL   							\
    if (kernelKTC == 0) {						\
	int code, ecode;						\
	int (*old)();							\
	int (*old_t)();							\
	old = (int (*)())signal(SIGSYS, SIG_IGN);			\
	old_t = (int (*)())signal(SIGTRAP, SIG_IGN);			\
	code = syscall (KTC_SYSCALL, KTC_NO_OP, 0,0,0,0,0);		\
	ecode = errno;							\
	signal(SIGSYS, old);						\
	signal(SIGTRAP, old_t);						\
	if (code == 0) kernelKTC = 1;					\
	else kernelKTC = 2;						\
/* printf ("returned from KTC_NO_OP kernelKTC <= %d; code=%d, errno=%d\n", kernelKTC, code, errno); */\
    }

#else /* AFS_DECOSF_ENV */

#define CHECK_KERNEL   							\
    if (kernelKTC == 0) {						\
	int code, ecode;						\
	int (*old)();							\
	old = (int (*)())signal(SIGSYS, SIG_IGN);			\
	code = syscall (KTC_SYSCALL, KTC_NO_OP, 0,0,0,0,0);		\
	ecode = errno;							\
	signal(SIGSYS, old);						\
	if (code == 0) kernelKTC = 1;					\
	else kernelKTC = 2;						\
/* printf ("returned from KTC_NO_OP kernelKTC <= %d; code=%d, errno=%d\n", kernelKTC, code, errno); */\
    }
#endif /* AFS_DECOSF_ENV */

#define TRY_KERNEL(cmd,a1,a2,a3,a4)					\
{   CHECK_KERNEL;							\
    if (kernelKTC == 1)							\
	return syscall (KTC_SYSCALL, cmd,				\
			KTC_INTERFACE_VERSION, a1,a2,a3,a4);		\
}

#else
#define TRY_KERNEL(cmd,a1,a2,a3,a4)
#endif /* KERNEL_KTC_COMPAT */

#if !defined(UKERNEL)
/* this is a structure used to communicate with the afs cache mgr, but is
 * otherwise irrelevant */
struct ClearToken {
    afs_int32 AuthHandle;
    char HandShakeKey[8];
    afs_int32 ViceId;
    afs_int32 BeginTimestamp;
    afs_int32 EndTimestamp;
};
#endif /* !defined(UKERNEL) */

#define MAXLOCALTOKENS 4

static struct {
    int valid;
    struct ktc_principal server;
    struct ktc_principal client;
    struct ktc_token token;
} local_tokens[MAXLOCALTOKENS] = { {
0}, {
0}, {
0}, {
0}};

/* new interface routines to the ticket cache.  Only handle afs service right
 * now. */

static int
NewSetToken(struct ktc_principal *aserver, 
	    struct ktc_token *atoken, 
	    struct ktc_principal *aclient, 
            afs_int32 flags)
{
    TRY_KERNEL(KTC_SETTOKEN_OP, aserver, aclient, atoken,
	       sizeof(struct ktc_token));
    /* no kernel ticket cache */
    return EINVAL;
}

#define MAXPIOCTLTOKENLEN \
(3*sizeof(afs_int32)+MAXKTCTICKETLEN+sizeof(struct ClearToken)+MAXKTCREALMLEN)

static int
OldSetToken(struct ktc_principal *aserver, struct ktc_token *atoken, 
	    struct ktc_principal *aclient, afs_int32 flags)
{
    struct ViceIoctl iob;
    char tbuffer[MAXPIOCTLTOKENLEN];
    register char *tp;
    struct ClearToken ct;
    register afs_int32 code;
    afs_int32 temp;

    if (strcmp(aserver->name, "afs") != 0) {
	int found = -1;
	int i;
	for (i = 0; i < MAXLOCALTOKENS; i++)
	    if (local_tokens[i].valid) {
		if ((strcmp(local_tokens[i].server.name, aserver->name) == 0)
		    &&
		    (strcmp
		     (local_tokens[i].server.instance,
		      aserver->instance) == 0)
		    && (strcmp(local_tokens[i].server.cell, aserver->cell) ==
			0)) {
		    found = i;	/* replace existing entry */
		    break;
		} else		/* valid, but no match */
		    ;
	    } else
		found = i;	/* remember this empty slot */
	if (found == -1)
	    return KTC_NOENT;
	memcpy(&local_tokens[found].token, atoken, sizeof(struct ktc_token));
	local_tokens[found].server = *aserver;
	local_tokens[found].client = *aclient;
	local_tokens[found].valid = 1;
	return 0;
    }
    tp = tbuffer;		/* start copying here */
    if ((atoken->ticketLen < MINKTCTICKETLEN)
	|| (atoken->ticketLen > MAXKTCTICKETLEN))
	return KTC_TOOBIG;
    memcpy(tp, &atoken->ticketLen, sizeof(afs_int32));	/* copy in ticket length */
    tp += sizeof(afs_int32);
    memcpy(tp, atoken->ticket, atoken->ticketLen);	/* copy in ticket */
    tp += atoken->ticketLen;
    /* next, copy in the "clear token", describing who we are */
    ct.AuthHandle = atoken->kvno;	/* hide auth handle here */
    memcpy(ct.HandShakeKey, &atoken->sessionKey, 8);

    ct.BeginTimestamp = atoken->startTime;
    ct.EndTimestamp = atoken->endTime;
    if (ct.BeginTimestamp == 0)
	ct.BeginTimestamp = 1;

    if ((strlen(aclient->name) > strlen("AFS ID "))
	&& (aclient->instance[0] == 0)) {
	int sign = 1;
	afs_int32 viceId = 0;
	char *cp = aclient->name + strlen("AFS ID ");
	if (*cp == '-') {
	    sign = -1;
	    cp++;
	}
	while (*cp) {
	    if (isdigit(*cp))
		viceId = viceId * 10 + (int)(*cp - '0');
	    else
		goto not_vice_id;
	    cp++;
	}
	ct.ViceId = viceId * sign;	/* OK to let any value here? */
	if (((ct.EndTimestamp - ct.BeginTimestamp) & 1) == 0)
	    ct.BeginTimestamp++;	/* force lifetime to be odd */
    } else {
      not_vice_id:
	ct.ViceId = getuid();	/* wrong, but works in primary cell */
	if (((ct.EndTimestamp - ct.BeginTimestamp) & 1) == 1)
	    ct.BeginTimestamp++;	/* force lifetime to be even */
    }

#ifdef UKERNEL
    /*
     * Information needed by the user space cache manager
     */
    u.u_expiration = ct.EndTimestamp;
    u.u_viceid = ct.ViceId;
#endif

    temp = sizeof(struct ClearToken);
    memcpy(tp, &temp, sizeof(afs_int32));
    tp += sizeof(afs_int32);
    memcpy(tp, &ct, sizeof(struct ClearToken));
    tp += sizeof(struct ClearToken);

    /* next copy in primary flag */
    temp = 0;

    /*
     * The following means that setpag will happen inside afs just before
     * the authentication to prevent the setpag/klog race condition.
     *
     * The following means that setpag will affect the parent process as
     * well as the current process.
     */
    if (flags & AFS_SETTOK_SETPAG)
	temp |= 0x8000;

    memcpy(tp, &temp, sizeof(afs_int32));
    tp += sizeof(afs_int32);

    /* finally copy in the cell name */
    temp = strlen(aserver->cell);
    if (temp >= MAXKTCREALMLEN)
	return KTC_TOOBIG;
    strcpy(tp, aserver->cell);
    tp += temp + 1;

    /* now setup for the pioctl */
    iob.in = tbuffer;
    iob.in_size = tp - tbuffer;
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

#if defined(NO_AFS_CLIENT)
    {
	int fd;			/* DEBUG */
	char *tkfile;
	if ((tkfile = getenv("TKTFILE"))
	    &&
	    ((fd =
	      open(tkfile, O_WRONLY | O_APPEND | O_TRUNC | O_CREAT,
		   0644)) >= 0)) {
	    printf("Writing ticket to: %s\n", tkfile);
	    code = (write(fd, iob.in, iob.in_size) != iob.in_size);
	    close(fd);
	} else
	    code = KTC_PIOCTLFAIL;
    }
#else /* NO_AFS_CLIENT */
    code = PIOCTL(0, VIOCSETTOK, &iob, 0);
#endif /* NO_AFS_CLIENT */
    if (code)
	return KTC_PIOCTLFAIL;
    return 0;
}

int
ktc_SetToken(struct ktc_principal *aserver,
    struct ktc_token *atoken,
    struct ktc_principal *aclient,
    afs_int32 flags)
{
    int ncode, ocode;

    LOCK_GLOBAL_MUTEX;
#ifdef AFS_KERBEROS_ENV
    if (!lcell[0])
	ktc_LocalCell();

    if (			/*!strcmp(aclient->cell, lcell) && this would only store local creds */
	   (strcmp(aserver->name, "AuthServer")
	    || strcmp(aserver->instance, "Admin"))) {
	if (strcmp(aserver->name, "krbtgt") == 0) {
	    static char lrealm[MAXKTCREALMLEN];

	    if (!lrealm[0])
		ucstring(lrealm, lcell, MAXKTCREALMLEN);
	    if (strcmp(aserver->instance, lrealm) == 0) {
		afs_tf_create(aclient->name, aclient->instance);
	    }
	}

	ncode = afs_tf_init(ktc_tkt_string(), W_TKT_FIL);
	if (ncode == NO_TKT_FIL) {
	    (void)afs_tf_create(aclient->name, aclient->instance);
	    ncode = afs_tf_init(ktc_tkt_string(), W_TKT_FIL);
	}

	if (!ncode) {
	    afs_tf_save_cred(aserver, atoken, aclient);
	}
	afs_tf_close();
#ifdef NO_AFS_CLIENT
	UNLOCK_GLOBAL_MUTEX;
	return ncode;
#endif /* NO_AFS_CLIENT */
    }
#endif

#ifndef NO_AFS_CLIENT
    ncode = NewSetToken(aserver, atoken, aclient, flags);
    if (ncode ||		/* new style failed */
	(strcmp(aserver->name, "afs") == 0)) {	/* for afs tokens do both */
	ocode = OldSetToken(aserver, atoken, aclient, flags);
    } else
	ocode = 0;
    if (ncode && ocode) {
	UNLOCK_GLOBAL_MUTEX;
	if (ocode == -1)
	    ocode = errno;
	else if (ocode == KTC_PIOCTLFAIL)
	    ocode = errno;
	if (ocode == ESRCH)
	    return KTC_NOCELL;
	if (ocode == EINVAL)
	    return KTC_NOPIOCTL;
	if (ocode == EIO)
	    return KTC_NOCM;
	return KTC_PIOCTLFAIL;
    }
#endif /* NO_AFS_CLIENT */
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* get token, given server we need and token buffer.  aclient will eventually
 * be set to our identity to the server.
 */
int
ktc_GetToken(struct ktc_principal *aserver, struct ktc_token *atoken, 
	     int atokenLen, struct ktc_principal *aclient)
{
    struct ViceIoctl iob;
    char tbuffer[MAXPIOCTLTOKENLEN];
    register afs_int32 code;
    int index;
    char *stp, *cellp;		/* secret token ptr */
    struct ClearToken ct;
    register char *tp;
    afs_int32 temp;
    int maxLen;			/* biggest ticket we can copy */
    int tktLen;			/* server ticket length */
#ifdef AFS_KERBEROS_ENV
    char found = 0;
#endif

    LOCK_GLOBAL_MUTEX;
#ifndef NO_AFS_CLIENT
    TRY_KERNEL(KTC_GETTOKEN_OP, aserver, aclient, atoken, atokenLen);
#endif /* NO_AFS_CLIENT */

#ifdef AFS_KERBEROS_ENV
    if (!lcell[0])
	ktc_LocalCell();
#endif
#ifndef NO_AFS_CLIENT
    if (strcmp(aserver->name, "afs") != 0)
#endif /* NO_AFS_CLIENT */
    {
	int i;
	/* try the local tokens */
	for (i = 0; i < MAXLOCALTOKENS; i++)
	    if (local_tokens[i].valid
		&& (strcmp(local_tokens[i].server.name, aserver->name) == 0)
		&& (strcmp(local_tokens[i].server.instance, aserver->instance)
		    == 0)
		&& (strcmp(local_tokens[i].server.cell, aserver->cell) == 0)) {
		memcpy(atoken, &local_tokens[i].token,
		       min(atokenLen, sizeof(struct ktc_token)));
		if (aclient)
		    *aclient = local_tokens[i].client;
		UNLOCK_GLOBAL_MUTEX;
		return 0;
	    }
#ifdef AFS_KERBEROS_ENV
	if (!afs_tf_init(ktc_tkt_string(), R_TKT_FIL)) {
	    if (aclient) {
		if (!afs_tf_get_pname(aclient->name)
		    && !afs_tf_get_pinst(aclient->instance))
		    found = 1;
	    } else {
		char tmpstring[MAXHOSTCHARS];
		afs_tf_get_pname(tmpstring);
		afs_tf_get_pinst(tmpstring);
		found = 1;
	    }
	}
	if (found) {
	    struct ktc_principal cprincipal;
	    struct ktc_token ctoken;

	    while (!afs_tf_get_cred(&cprincipal, &ctoken)) {
		if (strcmp(cprincipal.name, aserver->name) == 0
		    && strcmp(cprincipal.instance, aserver->instance) == 0
		    && strcmp(cprincipal.cell, aserver->cell) == 0) {

		    if (aclient)
			strcpy(aclient->cell, lcell);
		    memcpy(atoken, &ctoken,
			   min(atokenLen, sizeof(struct ktc_token)));

		    afs_tf_close();
		    UNLOCK_GLOBAL_MUTEX;
		    return 0;
		}
	    }
	}
	afs_tf_close();
#endif
	UNLOCK_GLOBAL_MUTEX;
	return KTC_NOENT;
    }
#ifndef NO_AFS_CLIENT
    for (index = 0; index < 200; index++) {	/* sanity check in case pioctl fails */
	iob.in = (char *)&index;
	iob.in_size = sizeof(afs_int32);
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

	code = PIOCTL(0, VIOCGETTOK, &iob, 0);

	if (code) {
	    /* failed to retrieve specified token */
	    if (code < 0 && errno == EDOM) {
		UNLOCK_GLOBAL_MUTEX;
		return KTC_NOENT;
	    }
	} else {
	    /* token retrieved; parse buffer */
	    tp = tbuffer;

	    /* get ticket length */
	    memcpy(&temp, tp, sizeof(afs_int32));
	    tktLen = temp;
	    tp += sizeof(afs_int32);

	    /* remember where ticket is and skip over it */
	    stp = tp;
	    tp += tktLen;

	    /* get size of clear token and verify */
	    memcpy(&temp, tp, sizeof(afs_int32));
	    if (temp != sizeof(struct ClearToken)) {
		UNLOCK_GLOBAL_MUTEX;
		return KTC_ERROR;
	    }
	    tp += sizeof(afs_int32);

	    /* copy clear token */
	    memcpy(&ct, tp, temp);
	    tp += temp;

	    /* skip over primary flag */
	    tp += sizeof(afs_int32);

	    /* remember where cell name is */
	    cellp = tp;

	    if ((strcmp(cellp, aserver->cell) == 0)
#ifdef	AFS_KERBEROS_ENV
		|| (*aserver->cell == '\0' && strcmp(cellp, lcell) == 0)
#endif
		) {
		/* got token for cell; check that it will fit */
		maxLen =
		    atokenLen - sizeof(struct ktc_token) + MAXKTCTICKETLEN;
		if (maxLen < tktLen) {
		    UNLOCK_GLOBAL_MUTEX;
		    return KTC_TOOBIG;
		}

		/* set return values */
		memcpy(atoken->ticket, stp, tktLen);
		atoken->startTime = ct.BeginTimestamp;
		atoken->endTime = ct.EndTimestamp;
		if (ct.AuthHandle == -1) {
		    ct.AuthHandle = 999;
		}
		atoken->kvno = ct.AuthHandle;
		memcpy(&atoken->sessionKey, ct.HandShakeKey,
		       sizeof(struct ktc_encryptionKey));
		atoken->ticketLen = tktLen;

		if (aclient) {
		    strcpy(aclient->cell, cellp);
		    aclient->instance[0] = 0;

		    if ((atoken->kvno == 999) ||	/* old style bcrypt ticket */
			(ct.BeginTimestamp &&	/* new w/ prserver lookup */
			 (((ct.EndTimestamp - ct.BeginTimestamp) & 1) == 1))) {
			sprintf(aclient->name, "AFS ID %d", ct.ViceId);
		    } else {
			sprintf(aclient->name, "Unix UID %d", ct.ViceId);
		    }
		}
		UNLOCK_GLOBAL_MUTEX;
		return 0;
	    }
	}
    }
#endif /* NO_AFS_CLIENT */

    UNLOCK_GLOBAL_MUTEX;
    if ((code < 0) && (errno == EINVAL))
	return KTC_NOPIOCTL;
    return KTC_PIOCTLFAIL;	/* probable cause */
}

/*
 * Forget tokens for this server and the calling user.
 * NOT IMPLEMENTED YET!
 */
#ifndef NO_AFS_CLIENT
int
ktc_ForgetToken(struct ktc_principal *aserver)
{
    int rc;

    LOCK_GLOBAL_MUTEX;
    TRY_KERNEL(KTC_FORGETTOKEN_OP, aserver, 0, 0, 0);

    rc = ktc_ForgetAllTokens();	/* bogus, but better */
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}
#endif /* NO_AFS_CLIENT */

/* ktc_ListTokens - list all tokens.  start aprevIndex at 0, it returns the
 * next rock in (*aindex).  (*aserver) is set to the relevant ticket on
 * success.  */

int
ktc_ListTokens(int aprevIndex,
    int *aindex,
    struct ktc_principal *aserver)
{
    struct ViceIoctl iob;
    char tbuffer[MAXPIOCTLTOKENLEN];
    register afs_int32 code;
    register char *tp;
    afs_int32 temp, index;

    memset(tbuffer, 0, sizeof(tbuffer));

    LOCK_GLOBAL_MUTEX;
#ifndef NO_AFS_CLIENT
    TRY_KERNEL(KTC_LISTTOKENS_OP, aserver, aprevIndex, aindex, 0);
#endif /* NO_AFS_CLIENT */

    index = aprevIndex;
#ifdef NO_AFS_CLIENT
    if (index < 214)
	index = 214;
#endif /* NO_AFS_CLIENT */
#ifdef AFS_KERBEROS_ENV
    if (index >= 214) {
	int i;
	struct ktc_principal cprincipal;
	struct ktc_token ctoken;

	if (afs_tf_init(ktc_tkt_string(), R_TKT_FIL)
	    || afs_tf_get_pname(tbuffer) || afs_tf_get_pinst(tbuffer)) {
	    afs_tf_close();
	    UNLOCK_GLOBAL_MUTEX;
	    return KTC_NOENT;
	}

	for (i = 214; i < index; i++) {
	    if (afs_tf_get_cred(&cprincipal, &ctoken)) {
		afs_tf_close();
		UNLOCK_GLOBAL_MUTEX;
		return KTC_NOENT;
	    }
	}

      again:
	if (afs_tf_get_cred(&cprincipal, &ctoken)) {
	    afs_tf_close();
	    UNLOCK_GLOBAL_MUTEX;
	    return KTC_NOENT;
	}
	index++;

#ifndef NO_AFS_CLIENT
	if (!strcmp(cprincipal.name, "afs") && cprincipal.instance[0] == 0) {
	    goto again;
	}
#endif /* NO_AFS_CLIENT */

	for (i = 0; i < MAXLOCALTOKENS; i++) {
	    if (!strcmp(cprincipal.name, local_tokens[i].server.name)
		&& !strcmp(cprincipal.instance,
			   local_tokens[i].server.instance)
		&& !strcmp(cprincipal.cell, local_tokens[i].server.cell)) {
		goto again;
	    }
	}

	*aserver = cprincipal;
	*aindex = index;
	afs_tf_close();
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }
#endif

#ifndef NO_AFS_CLIENT
    if (index >= 123) {		/* special hack for returning TCS */
	while (index - 123 < MAXLOCALTOKENS) {
	    if (local_tokens[index - 123].valid) {
		*aserver = local_tokens[index - 123].server;
		*aindex = index + 1;
		UNLOCK_GLOBAL_MUTEX;
		return 0;
	    }
	    index++;
	}
	UNLOCK_GLOBAL_MUTEX;
#ifdef AFS_KERBEROS_ENV
	return ktc_ListTokens(214, aindex, aserver);
#else
	return KTC_NOENT;
#endif
    }

    /* get tokens from the kernel */
    while (index < 200) {	/* sanity check in case pioctl fails */
	iob.in = (char *)&index;
	iob.in_size = sizeof(afs_int32);
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);
	code = PIOCTL(0, VIOCGETTOK, &iob, 0);
	if (code < 0 && errno == EDOM) {
	    if (index < 123) {
		int rc;
		rc = ktc_ListTokens(123, aindex, aserver);
		UNLOCK_GLOBAL_MUTEX;
		return rc;
	    } else {
		UNLOCK_GLOBAL_MUTEX;
		return KTC_NOENT;
	    }
	}
	if (code == 0)
	    break;		/* got a ticket */
	/* otherwise we should skip this ticket slot */
	index++;
    }
    if (code < 0) {
	UNLOCK_GLOBAL_MUTEX;
	if (errno == EINVAL)
	    return KTC_NOPIOCTL;
	return KTC_PIOCTLFAIL;
    }

    /* parse buffer */
    tp = tbuffer;

    /* next iterator determined by earlier loop */
    *aindex = index + 1;

    memcpy(&temp, tp, sizeof(afs_int32));	/* get size of secret token */
    tp += sizeof(afs_int32);
    tp += temp;			/* skip ticket for now */
    memcpy(&temp, tp, sizeof(afs_int32));	/* get size of clear token */
    if (temp != sizeof(struct ClearToken)) {
	UNLOCK_GLOBAL_MUTEX;
	return KTC_ERROR;
    }
    tp += sizeof(afs_int32);	/* skip length */
    tp += temp;			/* skip clear token itself */
    tp += sizeof(afs_int32);	/* skip primary flag */
    /* tp now points to the cell name */
    strcpy(aserver->cell, tp);
    aserver->instance[0] = 0;
    strcpy(aserver->name, "afs");
#endif /* NO_AFS_CLIENT */
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* discard all tokens from this user's cache */

static int
NewForgetAll(void)
{
#ifndef NO_AFS_CLIENT
    TRY_KERNEL(KTC_FORGETALLTOKENS_OP, 0, 0, 0, 0);
#endif /* NO_AFS_CLIENT */
    return EINVAL;
}

static int
OldForgetAll(void)
{
    struct ViceIoctl iob;
    register afs_int32 code;
    int i;

    for (i = 0; i < MAXLOCALTOKENS; i++)
	local_tokens[i].valid = 0;

    iob.in = 0;
    iob.in_size = 0;
    iob.out = 0;
    iob.out_size = 0;
#ifndef NO_AFS_CLIENT
    code = PIOCTL(0, VIOCUNPAG, &iob, 0);
    if (code)
	return KTC_PIOCTLFAIL;
#endif /* NO_AFS_CLIENT */
    return 0;
}

int
ktc_ForgetAllTokens(void)
{
    int ncode, ocode;

    LOCK_GLOBAL_MUTEX;
#ifdef AFS_KERBEROS_ENV
    (void)afs_tf_dest_tkt();
#endif

    ncode = NewForgetAll();
    ocode = OldForgetAll();
    if (ncode && ocode) {
	if (ocode == -1)
	    ocode = errno;
	else if (ocode == KTC_PIOCTLFAIL)
	    ocode = errno;
	UNLOCK_GLOBAL_MUTEX;
	if (ocode == EINVAL)
	    return KTC_NOPIOCTL;
	return KTC_PIOCTLFAIL;
    }
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* ktc_OldPioctl - returns a boolean true if the kernel supports only the old
 * pioctl interface for delivering AFS tickets to the cache manager. */

int
ktc_OldPioctl(void)
{
    int rc;
    LOCK_GLOBAL_MUTEX;
#ifdef	KERNEL_KTC_COMPAT
    CHECK_KERNEL;
    rc = (kernelKTC != 1);	/* old style interface */
#else
    rc = 1;
#endif
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}


#ifdef AFS_KERBEROS_ENV
 /*
  * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
  *
  * For copying and distribution information, please see the file
  * <mit-copyright.h>.
  */

#if 0
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <krb.h>
#endif

#define TOO_BIG -1
#define TF_LCK_RETRY ((unsigned)2)	/* seconds to sleep before
					 * retry if ticket file is
					 * locked */

/*
 * fd must be initialized to something that won't ever occur as a real
 * file descriptor. Since open(2) returns only non-negative numbers as
 * valid file descriptors, and afs_tf_init always stuffs the return value
 * from open in here even if it is an error flag, we must
 * 	a. Initialize fd to a negative number, to indicate that it is
 * 	   not initially valid.
 *	b. When checking for a valid fd, assume that negative values
 *	   are invalid (ie. when deciding whether afs_tf_init has been
 *	   called.)
 *	c. In tf_close, be sure it gets reinitialized to a negative
 *	   number. 
 */
static int fd = -1;
static int curpos;			/* Position in tfbfr */
static int lastpos;			/* End of tfbfr */
static char tfbfr[BUFSIZ];	/* Buffer for ticket data */

static int tf_gets(char *, int);
static int tf_read(char *, int);

/*
 * This file contains routines for manipulating the ticket cache file.
 *
 * The ticket file is in the following format:
 *
 *      principal's name        (null-terminated string)
 *      principal's instance    (null-terminated string)
 *      CREDENTIAL_1
 *      CREDENTIAL_2
 *      ...
 *      CREDENTIAL_n
 *      EOF
 *
 *      Where "CREDENTIAL_x" consists of the following fixed-length
 *      fields from the CREDENTIALS structure (see "krb.h"):
 *
 *              char            service[MAXKTCNAMELEN]
 *              char            instance[MAXKTCNAMELEN]
 *              char            realm[REALM_SZ]
 *              C_Block         session
 *              int             lifetime
 *              int             kvno
 *              KTEXT_ST        ticket_st
 *              afs_int32            issue_date
 *
 * Short description of routines:
 *
 * afs_tf_init() opens the ticket file and locks it.
 *
 * afs_tf_get_pname() returns the principal's name.
 *
 * afs_tf_get_pinst() returns the principal's instance (may be null).
 *
 * afs_tf_get_cred() returns the next CREDENTIALS record.
 *
 * afs_tf_save_cred() appends a new CREDENTIAL record to the ticket file.
 *
 * afs_tf_close() closes the ticket file and releases the lock.
 *
 * tf_gets() returns the next null-terminated string.  It's an internal
 * routine used by afs_tf_get_pname(), afs_tf_get_pinst(), and 
 * afs_tf_get_cred().
 *
 * tf_read() reads a given number of bytes.  It's an internal routine
 * used by afs_tf_get_cred().
 */

/*
 * afs_tf_init() should be called before the other ticket file routines.
 * It takes the name of the ticket file to use, "tf_name", and a
 * read/write flag "rw" as arguments. 
 *
 * It tries to open the ticket file, checks the mode, and if everything
 * is okay, locks the file.  If it's opened for reading, the lock is
 * shared.  If it's opened for writing, the lock is exclusive. 
 *
 * Returns 0 if all went well, otherwise one of the following: 
 *
 * NO_TKT_FIL   - file wasn't there
 * TKT_FIL_ACC  - file was in wrong mode, etc.
 * TKT_FIL_LCK  - couldn't lock the file, even after a retry
 */

int
afs_tf_init(char *tf_name, int rw)
{
    int wflag;
    int me;
    struct stat stat_buf;

    switch (rw) {
    case R_TKT_FIL:
	wflag = 0;
	break;
    case W_TKT_FIL:
	wflag = 1;
	break;
    default:
	return TKT_FIL_ACC;
    }
    if (lstat(tf_name, &stat_buf) < 0)
	switch (errno) {
	case ENOENT:
	    return NO_TKT_FIL;
	default:
	    return TKT_FIL_ACC;
	}
    me = getuid();
    if ((stat_buf.st_uid != me && me != 0)
	|| ((stat_buf.st_mode & S_IFMT) != S_IFREG))
	return TKT_FIL_ACC;

    /*
     * If "wflag" is set, open the ticket file in append-writeonly mode
     * and lock the ticket file in exclusive mode.  If unable to lock
     * the file, sleep and try again.  If we fail again, return with the
     * proper error message. 
     */

    curpos = sizeof(tfbfr);

    if (wflag) {
	fd = open(tf_name, O_RDWR, 0600);
	if (fd < 0) {
	    return TKT_FIL_ACC;
	}
#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)
	if (fcntl(fd, F_SETLK, &fileWlock) == -1) {
	    sleep(TF_LCK_RETRY);
	    if (fcntl(fd, F_SETLK, &fileWlock) == -1) {
#else
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
	    sleep(TF_LCK_RETRY);
	    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
#endif
		(void)close(fd);
		fd = -1;
		return TKT_FIL_LCK;
	    }
	}
	return 0;
    }
    /*
     * Otherwise "wflag" is not set and the ticket file should be opened
     * for read-only operations and locked for shared access. 
     */

    fd = open(tf_name, O_RDONLY, 0600);
    if (fd < 0) {
	return TKT_FIL_ACC;
    }
#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)
    if (fcntl(fd, F_SETLK, &fileRlock) == -1) {
	sleep(TF_LCK_RETRY);
	if (fcntl(fd, F_SETLK, &fileRlock) == -1) {
#else
    if (flock(fd, LOCK_SH | LOCK_NB) < 0) {
	sleep(TF_LCK_RETRY);
	if (flock(fd, LOCK_SH | LOCK_NB) < 0) {
#endif
	    (void)close(fd);
	    fd = -1;
	    return TKT_FIL_LCK;
	}
    }
    return 0;
}

/*
 * afs_tf_get_pname() reads the principal's name from the ticket file. It
 * should only be called after afs_tf_init() has been called.  The
 * principal's name is filled into the "p" parameter.  If all goes well,
 * 0 is returned.  If afs_tf_init() wasn't called, TKT_FIL_INI is
 * returned.  If the name was null, or EOF was encountered, or the name
 * was longer than MAXKTCNAMELEN, TKT_FIL_FMT is returned. 
 */

int
afs_tf_get_pname(char *p)
{
    if (fd < 0) {
	return TKT_FIL_INI;
    }
    if (tf_gets(p, MAXKTCNAMELEN) < 2)	/* can't be just a null */
	return TKT_FIL_FMT;
    return 0;
}

/*
 * afs_tf_get_pinst() reads the principal's instance from a ticket file.
 * It should only be called after afs_tf_init() and afs_tf_get_pname() have
 * been called.  The instance is filled into the "inst" parameter.  If all
 * goes well, 0 is returned.  If afs_tf_init() wasn't called,
 * TKT_FIL_INI is returned.  If EOF was encountered, or the instance
 * was longer than MAXKTCNAMELEN, TKT_FIL_FMT is returned.  Note that the
 * instance may be null. 
 */

int
afs_tf_get_pinst(char *inst)
{
    if (fd < 0) {
	return TKT_FIL_INI;
    }
    if (tf_gets(inst, MAXKTCNAMELEN) < 1)
	return TKT_FIL_FMT;
    return 0;
}

/*
 * afs_tf_get_cred() reads a CREDENTIALS record from a ticket file and fills
 * in the given structure "c".  It should only be called after afs_tf_init(),
 * afs_tf_get_pname(), and afs_tf_get_pinst() have been called. If all goes 
 * well, 0 is returned.  Possible error codes are: 
 *
 * TKT_FIL_INI  - afs_tf_init wasn't called first
 * TKT_FIL_FMT  - bad format
 * EOF          - end of file encountered
 */

int
afs_tf_get_cred(struct ktc_principal *principal, struct ktc_token *token)
{
    int k_errno;
    int kvno, lifetime;
    long mit_compat;		/* MIT Kerberos 5 with Krb4 uses a "long" for issue_date */

    if (fd < 0) {
	return TKT_FIL_INI;
    }
    if ((k_errno = tf_gets(principal->name, MAXKTCNAMELEN)) < 2)
	switch (k_errno) {
	case TOO_BIG:
	case 1:		/* can't be just a null */
	    return TKT_FIL_FMT;
	case 0:
	    return EOF;
	}
    if ((k_errno = tf_gets(principal->instance, MAXKTCNAMELEN)) < 1)
	switch (k_errno) {
	case TOO_BIG:
	    return TKT_FIL_FMT;
	case 0:
	    return EOF;
	}
    if ((k_errno = tf_gets(principal->cell, MAXKTCREALMLEN)) < 2)
	switch (k_errno) {
	case TOO_BIG:
	case 1:		/* can't be just a null */
	    return TKT_FIL_FMT;
	case 0:
	    return EOF;
	}
    lcstring(principal->cell, principal->cell, MAXKTCREALMLEN);
    if (tf_read((char *)&(token->sessionKey), 8) < 1
	|| tf_read((char *)&(lifetime), sizeof(lifetime)) < 1
	|| tf_read((char *)&(kvno), sizeof(kvno)) < 1
	|| tf_read((char *)&(token->ticketLen), sizeof(token->ticketLen))
	< 1 ||
	/* don't try to read a silly amount into ticket->dat */
	token->ticketLen > MAXKTCTICKETLEN
	|| tf_read((char *)(token->ticket), token->ticketLen) < 1
	|| tf_read((char *)&mit_compat, sizeof(mit_compat)) < 1) {
	return TKT_FIL_FMT;
    }
    token->startTime = mit_compat;
    token->endTime = life_to_time(token->startTime, lifetime);
    token->kvno = kvno;
    return 0;
}

/*
 * tf_close() closes the ticket file and sets "fd" to -1. If "fd" is
 * not a valid file descriptor, it just returns.  It also clears the
 * buffer used to read tickets.
 *
 * The return value is not defined.
 */

int
afs_tf_close(void)
{
    if (!(fd < 0)) {
#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)
	(void)fcntl(fd, F_SETLK, &fileUlock);
#else
	(void)flock(fd, LOCK_UN);
#endif
	(void)close(fd);
	fd = -1;		/* see declaration of fd above */
    }
    memset(tfbfr, 0, sizeof(tfbfr));
    return 0;
}

/*
 * tf_gets() is an internal routine.  It takes a string "s" and a count
 * "n", and reads from the file until either it has read "n" characters,
 * or until it reads a null byte. When finished, what has been read exists
 * in "s".
 *
 * Possible return values are:
 *
 * n            the number of bytes read (including null terminator)
 *              when all goes well
 *
 * 0            end of file or read error
 *
 * TOO_BIG      if "count" characters are read and no null is
 *		encountered. This is an indication that the ticket
 *		file is seriously ill.
 */

static int
tf_gets(register char *s, int n)
{
    register int count;

    if (fd < 0) {
	return TKT_FIL_INI;
    }
    for (count = n - 1; count > 0; --count) {
	if (curpos >= sizeof(tfbfr)) {
	    lastpos = read(fd, tfbfr, sizeof(tfbfr));
	    curpos = 0;
	}
	if (curpos == lastpos) {
	    return 0;
	}
	*s = tfbfr[curpos++];
	if (*s++ == '\0')
	    return (n - count);
    }
    return TOO_BIG;
}

/*
 * tf_read() is an internal routine.  It takes a string "s" and a count
 * "n", and reads from the file until "n" bytes have been read.  When
 * finished, what has been read exists in "s".
 *
 * Possible return values are:
 *
 * n		the number of bytes read when all goes well
 *
 * 0		on end of file or read error
 */

static int
tf_read(register char *s, register int n)
{
    register int count;

    for (count = n; count > 0; --count) {
	if (curpos >= sizeof(tfbfr)) {
	    lastpos = read(fd, tfbfr, sizeof(tfbfr));
	    curpos = 0;
	}
	if (curpos == lastpos) {
	    return 0;
	}
	*s++ = tfbfr[curpos++];
    }
    return n;
}

/*
 * afs_tf_save_cred() appends an incoming ticket to the end of the ticket
 * file.  You must call afs_tf_init() before calling afs_tf_save_cred().
 *
 * The "service", "instance", and "realm" arguments specify the
 * server's name; "aticket" contains the credential.
 *
 * Returns 0 if all goes well, TKT_FIL_INI if afs_tf_init() wasn't
 * called previously, and KFAILURE for anything else that went wrong.
 */

int
afs_tf_save_cred(struct ktc_principal *aserver, 
		 struct ktc_token *atoken, 
		 struct ktc_principal *aclient)
{
    char realm[MAXKTCREALMLEN + 1];
    char junk[MAXKTCNAMELEN];
    struct ktc_principal principal;
    struct ktc_token token;
    int status;
    off_t start;
    int lifetime, kvno;
    int count;			/* count for write */
    long mit_compat;		/* MIT Kerberos 5 with Krb4 uses a "long" for issue_date */

    if (fd < 0) {		/* fd is ticket file as set by afs_tf_init */
	return TKT_FIL_INI;
    }

    ucstring(realm, aserver->cell, MAXKTCREALMLEN);
    realm[MAXKTCREALMLEN] = '\0';

    /* Look for a duplicate ticket */
    (void)lseek(fd, (off_t) 0L, 0);
    curpos = sizeof(tfbfr);

    if (afs_tf_get_pname(junk) || strcmp(junk, aclient->name)
	|| afs_tf_get_pinst(junk) || strcmp(junk, aclient->instance))
	goto bad;

    do {
	start = lseek(fd, (off_t) 0L, 1) - lastpos + curpos;
	status = afs_tf_get_cred(&principal, &token);
    } while (status == 0
	     && (strcmp(aserver->name, principal.name) != 0
		 || strcmp(aserver->instance, principal.instance) != 0
		 || strcmp(aserver->cell, principal.cell) != 0));

    /*
     * Two tickets for the same user authenticating to the same service
     * should be the same length, but we check here just to make sure.
     */
    if (status == 0 && token.ticketLen != atoken->ticketLen)
	return KFAILURE;
    if (status && status != EOF)
	return status;

    /* Position over the credential we just matched (or the EOF) */
    lseek(fd, start, 0);
    curpos = lastpos = sizeof(tfbfr);

    /* Write the ticket and associated data */
    /* Service */
    count = strlen(aserver->name) + 1;
    if (write(fd, aserver->name, count) != count)
	goto bad;
    /* Instance */
    count = strlen(aserver->instance) + 1;
    if (write(fd, aserver->instance, count) != count)
	goto bad;
    /* Realm */
    count = strlen(realm) + 1;
    if (write(fd, realm, count) != count)
	goto bad;
    /* Session key */
    if (write(fd, (char *)&atoken->sessionKey, 8) != 8)
	goto bad;
    /* Lifetime */
    lifetime = time_to_life(atoken->startTime, atoken->endTime);
    if (write(fd, (char *)&lifetime, sizeof(int)) != sizeof(int))
	goto bad;
    /* Key vno */
    kvno = atoken->kvno;
    if (write(fd, (char *)&kvno, sizeof(int)) != sizeof(int))
	goto bad;
    /* Tkt length */
    if (write(fd, (char *)&(atoken->ticketLen), sizeof(int)) != sizeof(int))
	goto bad;
    /* Ticket */
    count = atoken->ticketLen;
    if (write(fd, atoken->ticket, count) != count)
	goto bad;
    /* Issue date */
    mit_compat = atoken->startTime;
    if (write(fd, (char *)&mit_compat, sizeof(mit_compat))
	!= sizeof(mit_compat))
	goto bad;

    /* Actually, we should check each write for success */
    return (0);
  bad:
    return (KFAILURE);
}

/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

/*
 * This routine is used to generate the name of the file that holds
 * the user's cache of server tickets and associated session keys.
 *
 * If it is set, krb_ticket_string contains the ticket file name.
 * Otherwise, the filename is constructed as follows:
 *
 * If it is set, the environment variable "KRBTKFILE" will be used as
 * the ticket file name.  Otherwise TKT_ROOT (defined in "krb.h") and
 * the user's uid are concatenated to produce the ticket file name
 * (e.g., "/tmp/tkt123").  A pointer to the string containing the ticket
 * file name is returned.
 */

static char krb_ticket_string[4096] = "";

char *
ktc_tkt_string(void)
{
    return ktc_tkt_string_uid(getuid());
}

char *
ktc_tkt_string_uid(afs_uint32 uid)
{
    char *env;

    LOCK_GLOBAL_MUTEX;
    if (!*krb_ticket_string) {
	if ((env = getenv("KRBTKFILE"))) {
	    (void)strncpy(krb_ticket_string, env,
			  sizeof(krb_ticket_string) - 1);
	    krb_ticket_string[sizeof(krb_ticket_string) - 1] = '\0';
	} else {
	    /* 32 bits of signed integer will always fit in 11 characters
	     * (including the sign), so no need to worry about overflow */
	    (void)sprintf(krb_ticket_string, "%s%d", TKT_ROOT, uid);
	}
    }
    UNLOCK_GLOBAL_MUTEX;
    return krb_ticket_string;
}

/*
 * This routine is used to set the name of the file that holds the user's
 * cache of server tickets and associated session keys.
 *
 * The value passed in is copied into local storage.
 *
 * NOTE:  This routine should be called during initialization, before other
 * Kerberos routines are called; otherwise tkt_string() above may be called
 * and return an undesired ticket file name until this routine is called.
 */

void
ktc_set_tkt_string(char * val)
{

    LOCK_GLOBAL_MUTEX;
    (void)strncpy(krb_ticket_string, val, sizeof(krb_ticket_string) - 1);
    krb_ticket_string[sizeof(krb_ticket_string) - 1] = '\0';
    UNLOCK_GLOBAL_MUTEX;
    return;
}

/*
 * tf_create() is used to initialize the ticket store.  It creates the
 * file to contain the tickets and writes the given user's name "pname"
 * and instance "pinst" in the file.  in_tkt() returns KSUCCESS on
 * success, or KFAILURE if something goes wrong.
 */

int
afs_tf_create(char *pname, char *pinst)
{
    int tktfile;
    int me, metoo;
    int count;
    char *file = ktc_tkt_string();
    int fd;
    register int i;
    char zerobuf[1024];
    struct stat sbuf;

    me = getuid();
    metoo = geteuid();

    if (lstat(file, &sbuf) == 0) {
	if ((sbuf.st_uid != me && me != 0)
	    || ((sbuf.st_mode & S_IFMT) != S_IFREG) || sbuf.st_mode & 077) {
	    return KFAILURE;
	}
	/* file already exists, and permissions appear ok, so nuke it */
	if ((fd = open(file, O_RDWR, 0)) < 0)
	    goto out;		/* can't zero it, but we can still try truncating it */

	memset(zerobuf, 0, sizeof(zerobuf));

	for (i = 0; i < sbuf.st_size; i += sizeof(zerobuf))
	    if (write(fd, zerobuf, sizeof(zerobuf)) != sizeof(zerobuf)) {
		(void)fsync(fd);
		(void)close(fd);
		goto out;
	    }

	(void)fsync(fd);
	(void)close(fd);
    }

  out:
    /* arrange so the file is owned by the ruid
     * (swap real & effective uid if necessary).
     * This isn't a security problem, since the ticket file, if it already
     * exists, has the right uid (== ruid) and mode. */
    if (me != metoo) {
	if (setreuid(metoo, me) < 0) {
	    return (KFAILURE);
	}
    }
    tktfile = creat(file, 0600);
    if (me != metoo) {
	if (setreuid(me, metoo) < 0) {
	    /* can't switch??? fail! */
	    return (KFAILURE);
	}
    }
    if (tktfile < 0) {
	return (KFAILURE);
    }
    count = strlen(pname) + 1;
    if (write(tktfile, pname, count) != count) {
	(void)close(tktfile);
	return (KFAILURE);
    }
    count = strlen(pinst) + 1;
    if (write(tktfile, pinst, count) != count) {
	(void)close(tktfile);
	return (KFAILURE);
    }
    (void)close(tktfile);
    return (KSUCCESS);
}

/*
 * dest_tkt() is used to destroy the ticket store upon logout.
 * If the ticket file does not exist, dest_tkt() returns RET_TKFIL.
 * Otherwise the function returns 0 on success, KFAILURE on
 * failure.
 */

int
afs_tf_dest_tkt(void)
{
    char *file = ktc_tkt_string();
    int i, fd;
    struct stat statb;
    char buf[BUFSIZ];

    errno = 0;
    if (lstat(file, &statb) < 0)
	goto out;

    if (!(statb.st_mode & S_IFREG))
	goto out;

    if ((fd = open(file, O_RDWR, 0)) < 0)
	goto out;

    memset(buf, 0, BUFSIZ);

    for (i = 0; i < statb.st_size; i += BUFSIZ)
	if (write(fd, buf, BUFSIZ) != BUFSIZ) {
	    (void)fsync(fd);
	    (void)close(fd);
	    goto out;
	}

    (void)fsync(fd);
    (void)close(fd);

    (void)unlink(file);

  out:
    if (errno == ENOENT)
	return RET_TKFIL;
    else if (errno != 0)
	return KFAILURE;
    return 0;
}

static afs_uint32
curpag(void)
{
#if defined(AFS_AIX51_ENV)
    afs_int32 pag;

    if (get_pag(PAG_AFS, &pag) < 0 || pag == 0)
        pag = NOPAG;
    return pag;
#else
    gid_t groups[NGROUPS_MAX];
    afs_uint32 g0, g1;
    afs_uint32 h, l, ret;

    if (getgroups(sizeof groups / sizeof groups[0], groups) < 2)
	return 0;

    g0 = groups[0] & 0xffff;
    g1 = groups[1] & 0xffff;
    g0 -= 0x3f00;
    g1 -= 0x3f00;
    if (g0 < 0xc000 && g1 < 0xc000) {
	l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
	h = (g0 >> 14);
	h = (g1 >> 14) + h + h + h;
	ret = ((h << 28) | l);
	/* Additional testing */
	if (((ret >> 24) & 0xff) == 'A')
	    return ret;
	else
	    return -1;
    }
    return -1;
#endif
}

int
ktc_newpag(void)
{
    extern char **environ;

    afs_uint32 pag;
    struct stat sbuf;
    char fname[256], *prefix = "/ticket/";
    int numenv;
    char **newenv, **senv, **denv;

    LOCK_GLOBAL_MUTEX;
    if (stat("/ticket", &sbuf) == -1) {
	prefix = "/tmp/tkt";
    }

    pag = curpag() & 0xffffffff;
    if (pag == -1) {
	sprintf(fname, "%s%d", prefix, getuid());
    } else {
	sprintf(fname, "%sp%ld", prefix, (long int) pag);
    }
    ktc_set_tkt_string(fname);

    for (senv = environ, numenv = 0; *senv; senv++)
	numenv++;
    newenv = (char **)malloc((numenv + 2) * sizeof(char *));

    for (senv = environ, denv = newenv; *senv; senv++) {
	if (strncmp(*senv, "KRBTKFILE=", 10) != 0)
	    *denv++ = *senv;
    }

    *denv = (char *)malloc(10 + strlen(fname) + 1);
    strcpy(*denv, "KRBTKFILE=");
    strcat(*denv, fname);
    *++denv = 0;
    environ = newenv;
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/*
 * BLETCH!  We have to invoke the entire afsconf package just to
 * find out what the local cell is.
 */
static void
ktc_LocalCell(void)
{
    int code;
    struct afsconf_dir *conf;

    if ((conf = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))
	|| (conf = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH))) {
	code = afsconf_GetLocalCell(conf, lcell, sizeof(lcell));
	afsconf_Close(conf);
    }
    if (!conf || code) {
	printf("** Can't determine local cell name!\n");
    }
}

#endif
