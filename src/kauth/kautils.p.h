/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 2.2  1990/09/27  13:51:37
 * Declare (char *) returning function ka_timestr().
 * Cleanups.
 *
 * Revision 2.1  90/08/07  19:11:51
 * Start with clean version to sync test and dev trees.
 * */

#ifndef __KAUTILS__
#define __KAUTILS__

#if !defined(UKERNEL)
#include <des.h>
#include <afs/auth.h>

#ifndef KAMAJORVERSION
    /* just to be on the safe side, get these two first */
#include <sys/types.h>
#include <rx/xdr.h>

    /* get installed .h file only if not included already from local dir */
#ifndef _RXGEN_KAUTH_
#include <afs/kauth.h>
#endif

#endif

#include <ubik.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>

#else
#include "ubik.h"
#include "afs/auth.h"
#include "afs/cellconfig.h"
#endif /* !defined(UKERNEL) */


#define KA_TIMESTR_LEN 30
#define Date afs_uint32

/*
 * Public function prototypes
 */

extern afs_int32 ka_GetAuthToken(char *name, char *instance, char *cell,
				 struct ktc_encryptionKey *key,
				 afs_int32 lifetime, afs_int32 * pwexpires);

extern afs_int32 ka_GetServerToken(char *name, char *instance, char *cell,
				   Date lifetime, struct ktc_token *token,
				   int newer, int dosetpag);

extern afs_int32 ka_GetAdminToken(char *name, char *instance, char *cell,
				  struct ktc_encryptionKey *key,
				  afs_int32 lifetime, struct ktc_token *token,
				  int newer);

extern afs_int32 ka_VerifyUserToken(char *name, char *instance, char *cell,
				    struct ktc_encryptionKey *key);

extern void ka_ExplicitCell(char *cell, afs_int32 serverList[]
    );

extern afs_int32 ka_GetServers(char *cell, struct afsconf_cell *cellinfo);

extern afs_int32 ka_GetSecurity(int service, struct ktc_token *token,
				struct rx_securityClass **scP, int *siP);

extern afs_int32 ka_SingleServerConn(char *cell, char *server, int service,
				     struct ktc_token *token,
				     struct ubik_client **conn);

extern afs_int32 ka_AuthSpecificServersConn(int service,
					    struct ktc_token *token,
					    struct afsconf_cell *cellinfo,
					    struct ubik_client **conn);

extern afs_int32 ka_AuthServerConn(char *cell, int service,
				   struct ktc_token *token,
				   struct ubik_client **conn);

extern afs_int32 ka_Authenticate(char *name, char *instance, char *cell,
				 struct ubik_client *conn, int service,
				 struct ktc_encryptionKey *key, Date start,
				 Date end, struct ktc_token *token,
				 afs_int32 * pwexpires);

extern afs_int32 ka_GetToken(char *name, char *instance, char *cell,
			     char *cname, char *cinst,
			     struct ubik_client *conn, Date start, Date end,
			     struct ktc_token *auth_token, char *auth_domain,
			     struct ktc_token *token);

extern afs_int32 ka_ChangePassword(char *name, char *instance,
				   struct ubik_client *conn,
				   struct ktc_encryptionKey *oldkey,
				   struct ktc_encryptionKey *newkey);

extern void ka_StringToKey(char *str, char *cell,
			   struct ktc_encryptionKey *key);

extern afs_int32 ka_ReadPassword(char *prompt, int verify, char *cell,
				 struct ktc_encryptionKey *key);

extern afs_int32 ka_ParseLoginName(char *login, char name[MAXKTCNAMELEN],
				   char inst[MAXKTCNAMELEN],
				   char cell[MAXKTCREALMLEN]
    );

#ifdef _MFC_VER
extern "C" {
#endif				/* _MFC_VER */
    extern afs_int32 ka_Init(int flags);
#ifdef _MFC_VER
}
#endif				/* _MFC_VER */
extern int ka_CellConfig(const char *dir);

extern char *ka_LocalCell(void
    );

extern int ka_ExpandCell(char *cell, char *fullCell, int *alocal);

extern int ka_CellToRealm(char *cell, char *realm, int *local);

extern void ka_PrintUserID(char *prefix, char *name, char *instance,
			   char *postfix);

extern void ka_PrintBytes(char bs[], int bl);

extern int ka_ConvertBytes(char *ascii, int alen, char bs[], int bl);

extern int ka_ReadBytes(char *ascii, char *binary, int blen);

extern int umin(afs_uint32 a, afs_uint32 b);

extern afs_int32 ka_KeyCheckSum(char *key, afs_uint32 * cksumP);

extern int ka_KeyIsZero(register char *akey, register int alen);

extern void ka_timestr(afs_int32 time, char *tstr, afs_int32 tlen);

extern void ka_debugKeyCache(struct ka_debugInfo *info);

extern void save_principal(char *p, char *n, char *i, char *c);

extern afs_int32 ka_GetAFSTicket(char *name, char *instance, char *realm,
				 Date lifetime, afs_int32 flags);

extern afs_int32 ka_UserAuthenticateGeneral(afs_int32 flags, char *name,
					    char *instance, char *realm,
					    char *password, Date lifetime,
					    afs_int32 * password_expires,
					    afs_int32 spare2, char **reasonP);

extern afs_int32 ka_UserAuthenticateGeneral2(afs_int32 flags, char *name,
					     char *instance, char *realm,
					     char *password, char *smbname,
					     Date lifetime,
					     afs_int32 * password_expires,
					     afs_int32 spare2,
					     char **reasonP);
extern afs_int32 ka_UserAuthenticate(char *name, char *instance, char *realm,
				     char *password, int doSetPAG,
				     char **reasonP);

extern afs_int32 ka_UserReadPassword(char *prompt, char *password, int plen,
				     char **reasonP);

extern afs_int32 ka_VerifyUserPassword(afs_int32 version, char *name,
				       char *instance, char *realm,
				       char *password, int spare,
				       char **reasonP);
#define KA_USERAUTH_VERSION 1
#define KA_USERAUTH_VERSION_MASK	0x00ffff
#define KA_USERAUTH_DOSETPAG		0x010000
#define KA_USERAUTH_DOSETPAG2		0x020000
#define KA_USERAUTH_ONLY_VERIFY		0x040000
#define KA_USERAUTH_AUTHENT_LOGON	0x100000
#define ka_UserAuthenticate(n,i,r,p,d,rP) \
    ka_UserAuthenticateGeneral \
        (KA_USERAUTH_VERSION + ((d) ? KA_USERAUTH_DOSETPAG : 0), \
	 n,i,r,p, /*lifetime*/0, /*spare1,2*/0,0, rP)
#define ka_UserAuthenticateLife(f,n,i,r,p,l,rP) \
    ka_UserAuthenticateGeneral \
        (KA_USERAUTH_VERSION + (f), n,i,r,p,l, /*spare1,2*/0,0, rP)

#define KA_REUSEPW 1
#define KA_NOREUSEPW 2
#define KA_ISLOCKED 4

#define KA_AUTHENTICATION_SERVICE 731
#define KA_TICKET_GRANTING_SERVICE 732
#define KA_MAINTENANCE_SERVICE 733

#define RX_SCINDEX_NULL	0	/* No security */
#define RX_SCINDEX_VAB 	1	/* vice tokens, with bcrypt */
#define RX_SCINDEX_KAD	2	/* Kerberos/DES */

#define KA_TGS_NAME "krbtgt"
	/* realm is TGS instance */
#define KA_ADMIN_NAME "AuthServer"
#define KA_ADMIN_INST "Admin"

#define KA_LABELSIZE 4
#define KA_GETTGT_REQ_LABEL "gTGS"
#define KA_GETTGT_ANS_LABEL "tgsT"
#define KA_GETADM_REQ_LABEL "gADM"
#define KA_GETADM_ANS_LABEL "admT"
#define KA_CPW_REQ_LABEL "CPWl"
#define KA_CPW_ANS_LABEL "Pass"
#define KA_GETTICKET_ANS_LABEL "gtkt"

struct ka_gettgtRequest {	/* format of request */
    Date time;			/* time of request */
    char label[KA_LABELSIZE];	/* label to verify correct decrypt */
};

/* old interface: see ka_ticketAnswer instead */
struct ka_gettgtAnswer {	/* format of response */
    Date time;			/* the time of the request plus one */
    struct ktc_encryptionKey
      sessionkey;		/* the session key in the ticket */
    afs_int32 kvno;		/* version # of tkt encrypting key */
    afs_int32 ticket_len;	/* the ticket's length */
    char ticket[MAXKTCTICKETLEN];	/* the ticket itself (no padding) */
    char label[KA_LABELSIZE];	/* label to verify correct decrypt */
};

struct ka_ticketAnswer {	/* format of response */
    afs_int32 cksum;		/* function to be defined */
    Date challenge;		/* the time of the request plus one */
    struct ktc_encryptionKey
      sessionKey;		/* the session key in the ticket */
    Date startTime;
    Date endTime;
    afs_int32 kvno;		/* version of ticket encrypting key */
    afs_int32 ticketLen;	/* the ticket's length */
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char cell[MAXKTCNAMELEN];
    char sname[MAXKTCNAMELEN];
    char sinstance[MAXKTCNAMELEN];
    char ticket[MAXKTCTICKETLEN];	/* the ticket (no extra chars) */
    char label[KA_LABELSIZE];	/* for detecting decryption errors */
};

struct ka_cpwRequest {		/* format of request */
    Date time;			/* time of request */
    struct ktc_encryptionKey
      newpw;			/* new key */
    afs_int32 kvno;		/* version number of key */
    afs_int32 spare;		/* must be zero */
    char label[KA_LABELSIZE];	/* label to verify correct decrypt */
};

struct ka_cpwAnswer {		/* format of response */
    Date time;			/* the time of the request plus one */
    char label[KA_LABELSIZE];	/* label to verify correct decrypt */
};

struct ka_getTicketTimes {
    Date start;
    Date end;
};

/* old interface: see ka_ticketAnswer instead */
struct ka_getTicketAnswer {
    struct ktc_encryptionKey sessionKey;
    Date startTime;
    Date endTime;
    afs_int32 kvno;
    afs_int32 ticketLen;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char cell[MAXKTCNAMELEN];
    char sname[MAXKTCNAMELEN];
    char sinstance[MAXKTCNAMELEN];
    char ticket[MAXKTCTICKETLEN];
};

#ifndef ERROR_TABLE_BASE_KA
#define ka_ErrorString afs_error_message
#undef  KAMINERROR
#define KAMINERROR ERROR_TABLE_BASE_KA
#define KAMAXERROR (KAMINERROR+255)
#endif

#endif
