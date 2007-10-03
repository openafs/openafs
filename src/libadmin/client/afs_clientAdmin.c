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
#include "afs_clientAdmin.h"
#include "../adminutil/afs_AdminInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <afs/cellconfig.h>
#ifdef AFS_NT40_ENV
#include <afs/afssyscalls.h>
#include <winsock2.h>
#include <afs/fs_utils.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <afs/venus.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#endif
#include <string.h>
#include <afs/kautils.h>
#include <rx/rx.h>
#include <rx/rx_null.h>
#include <rx/rxkad.h>
#include <afs/dirpath.h>
#include <afs/afs_AdminErrors.h>
#include <afs/afs_vosAdmin.h>
#include <afs/afs_utilAdmin.h>
#include <afs/ptserver.h>
#include <afs/vlserver.h>
#include <afs/pthread_glock.h>

/*
 * AFS client administration functions.
 *
 * Admin functions that are normally associated with the client.
 *
 * All of the functions related to authentication are here, plus
 * some miscellaneous others.
 *
 */

static const unsigned long ADMIN_TICKET_LIFETIME = 24 * 3600;

static const unsigned long SERVER_TTL = 10 * 60;

/*
 * We need a way to track whether or not the client library has been 
 * initialized.  We count on the fact that the other library initialization
 * functions are protected by their own once mechanism.  We only track
 * our own internal status
 */

static int client_init;
static pthread_once_t client_init_once = PTHREAD_ONCE_INIT;

static void
client_once(void)
{
    client_init = 1;
}

/*
 * IsTokenValid - validate a token handle
 *
 * PARAMETERS
 *
 * IN token - the token to be validated.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IsTokenValid(const afs_token_handle_p token, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (token == NULL) {
	tst = ADMCLIENTTOKENHANDLENULL;
	goto fail_IsTokenValid;
    }

    if (token->is_valid == 0) {
	tst = ADMCLIENTTOKENHANDLEINVALID;
	goto fail_IsTokenValid;
    }

    if ((token->begin_magic != BEGIN_MAGIC)
	|| (token->end_magic != END_MAGIC)) {
	tst = ADMCLIENTTOKENHANDLEBADMAGIC;
	goto fail_IsTokenValid;
    }
    rc = 1;

  fail_IsTokenValid:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_TokenGetExisting - get tokens that already exist and
 * are held by the cache manager.
 *
 * PARAMETERS
 *
 * IN cellName - the name of the cell where the token originated.
 *
 * OUT tokenHandle - a handle to the tokens if they were obtained
 * successfully.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * The tokenHandle returned by this function cannot be used for kas
 * related operations, since kas tokens aren't stored in the kernel.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_TokenGetExisting(const char *cellName, void **tokenHandle,
			   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct ktc_principal afs_server;
    afs_token_handle_p t_handle =
	(afs_token_handle_p) calloc(1, sizeof(afs_token_handle_t));

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_TokenGetExisting;
    }

    if (cellName == NULL) {
	tst = ADMCLIENTCELLNAMENULL;
	goto fail_afsclient_TokenGetExisting;
    }

    if (tokenHandle == NULL) {
	tst = ADMCLIENTTOKENHANDLENULL;
	goto fail_afsclient_TokenGetExisting;
    }

    if (t_handle == NULL) {
	tst = ADMNOMEM;
	goto fail_afsclient_TokenGetExisting;
    }

    strcpy(afs_server.name, "afs");
    afs_server.instance[0] = 0;
    strcpy(afs_server.cell, cellName);

    if (!
	(tst =
	 ktc_GetToken(&afs_server, &t_handle->afs_token,
		      sizeof(t_handle->afs_token), &t_handle->client))) {
	/*
	 * The token has been retrieved successfully, initialize
	 * the rest of the token handle structure
	 */
	strncpy(t_handle->cell, cellName, MAXCELLCHARS);
        t_handle->cell[MAXCELLCHARS - 1] = '\0';
	t_handle->afs_token_set = 1;
	t_handle->from_kernel = 1;
	t_handle->kas_token_set = 0;
	t_handle->sc_index = 2;
	t_handle->afs_sc[t_handle->sc_index] =
	    rxkad_NewClientSecurityObject(rxkad_clear,
					  &t_handle->afs_token.sessionKey,
					  t_handle->afs_token.kvno,
					  t_handle->afs_token.ticketLen,
					  t_handle->afs_token.ticket);
	t_handle->afs_encrypt_sc[t_handle->sc_index] =
	    rxkad_NewClientSecurityObject(rxkad_crypt,
					  &t_handle->afs_token.sessionKey,
					  t_handle->afs_token.kvno,
					  t_handle->afs_token.ticketLen,
					  t_handle->afs_token.ticket);
	if ((t_handle->afs_sc[t_handle->sc_index] == NULL)
	    || (t_handle->afs_sc[t_handle->sc_index] == NULL)) {
	    tst = ADMCLIENTTOKENHANDLENOSECURITY;
	    goto fail_afsclient_TokenGetExisting;
	} else {
	    t_handle->begin_magic = BEGIN_MAGIC;
	    t_handle->is_valid = 1;
	    t_handle->end_magic = END_MAGIC;
	    *tokenHandle = (void *)t_handle;
	}
    } else {
	goto fail_afsclient_TokenGetExisting;
    }
    rc = 1;

  fail_afsclient_TokenGetExisting:

    if ((rc == 0) && (t_handle != NULL)) {
	free(t_handle);
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_TokenSet - set the tokens represented by tokenHandle to be
 * active in the kernel (aka ka_SetToken).
 *
 * PARAMETERS
 *
 * IN cellName - the name of the cell where the token originated.
 *
 * OUT tokenHandle - a handle to the tokens if they were obtained
 * successfully.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * The tokenHandle returned by this function cannot be used for kas
 * related operations, since kas tokens aren't stored in the kernel.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_TokenSet(const void *tokenHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct ktc_principal afs_server;
    afs_token_handle_p t_handle = (afs_token_handle_p) tokenHandle;

    if (!IsTokenValid(t_handle, &tst)) {
	goto fail_afsclient_TokenSet;
    }

    strcpy(afs_server.name, "afs");
    afs_server.instance[0] = 0;
    strcpy(afs_server.cell, t_handle->cell);

    tst =
	ktc_SetToken(&afs_server, &t_handle->afs_token, &t_handle->client, 0);

    if (!tst) {
	rc = 1;
    }

  fail_afsclient_TokenSet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * GetKASToken - get a KAS token and store it in the tokenHandle.
 *
 * PARAMETERS
 *
 * IN cellName - the name of the cell where the token should be obtained.
 * 
 * IN principal - the name of the user of the token.
 *
 * IN password - the password for the principal.
 *
 * OUT tokenHandle - a handle to the tokens if they were obtained
 * successfully.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
GetKASToken(const char *cellName, const char *principal, const char *password,
	    afs_token_handle_p tokenHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct ubik_client *unauth_conn;
    afs_int32 expire;
    struct ktc_encryptionKey key;
    struct ktc_token *token;
    unsigned long now = time(0);
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
    int have_server_conn = 0;

    token = &tokenHandle->kas_token;

    ka_StringToKey((char *)password, (char *)cellName, &key);

    /* 
     * Get an unauthenticated connection in the right cell to use for
     * retrieving the token.
     */

    tst =
	ka_AuthServerConn((char *)cellName, KA_AUTHENTICATION_SERVICE, 0,
			  &unauth_conn);
    if (tst != 0) {
	goto fail_GetKASToken;
    }
    have_server_conn = 1;

    tst = ka_ParseLoginName((char *)principal, name, inst, NULL);
    if (tst != 0) {
	goto fail_GetKASToken;
    }

    tst =
	ka_Authenticate(name, inst, (char *)cellName, unauth_conn,
			KA_MAINTENANCE_SERVICE, &key, now,
			now + ADMIN_TICKET_LIFETIME, token, &expire);
    if (tst != 0) {
	goto fail_GetKASToken;
    }
    rc = 1;

  fail_GetKASToken:

    if (have_server_conn) {
	ubik_ClientDestroy(unauth_conn);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * GetAFSToken - get a AFS token and store it in the tokenHandle.
 *
 * PARAMETERS
 *
 * IN cellName - the name of the cell where the token should be obtained.
 * 
 * IN principal - the name of the user of the token.
 *
 * IN password - the password for the principal.
 *
 * OUT tokenHandle - a handle to the tokens if they were obtained
 * successfully.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
GetAFSToken(const char *cellName, const char *principal, const char *password,
	    afs_token_handle_p tokenHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct ubik_client *unauth_conn = NULL, *auth_conn = NULL;
    afs_int32 expire;
    struct ktc_encryptionKey key;
    struct ktc_token auth_token;
    struct ktc_token *token;
    unsigned long now = time(0);

    token = &tokenHandle->afs_token;

    ka_StringToKey((char *)password, (char *)cellName, &key);

    /* 
     * Get an unauthenticated connection in the right cell to use for
     * retrieving the token.
     */

    tst =
	ka_AuthServerConn((char *)cellName, KA_AUTHENTICATION_SERVICE, 0,
			  &unauth_conn);
    if (tst) {
	goto fail_GetAFSToken;
    }

    tst =
	ka_ParseLoginName((char *)principal, tokenHandle->client.name,
			  tokenHandle->client.instance, NULL);
    if (tst) {
	goto fail_GetAFSToken;
    }

    tst =
	ka_Authenticate(tokenHandle->client.name,
			tokenHandle->client.instance, (char *)cellName,
			unauth_conn, KA_TICKET_GRANTING_SERVICE, &key, now,
			now + ADMIN_TICKET_LIFETIME, &auth_token, &expire);
    if (tst) {
	goto fail_GetAFSToken;
    }

    tst =
	ka_AuthServerConn((char *)cellName, KA_TICKET_GRANTING_SERVICE, 0,
			  &auth_conn);
    if (tst) {
	goto fail_GetAFSToken;
    }

    tst =
	ka_CellToRealm((char *)cellName, tokenHandle->client.cell, (int *)0);
    if (tst) {
	goto fail_GetAFSToken;
    }

    tst =
	ka_GetToken("afs", "", (char *)cellName, tokenHandle->client.name,
		    tokenHandle->client.instance, auth_conn, now,
		    now + ADMIN_TICKET_LIFETIME, &auth_token,
		    tokenHandle->client.cell, token);
    if (tst) {
	goto fail_GetAFSToken;
    }
    rc = 1;

  fail_GetAFSToken:

    if (auth_conn) {
	ubik_ClientDestroy(auth_conn);
    }

    if (unauth_conn) {
	ubik_ClientDestroy(unauth_conn);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * afsclient_TokenGetNew - get new tokens for a user and store them
 * in the tokenHandle.
 *
 * PARAMETERS
 *
 * IN cellName - the name of the cell where the tokens should be obtained.
 * 
 * IN principal - the name of the user of the tokens.
 *
 * IN password - the password for the principal.
 *
 * OUT tokenHandle - a handle to the tokens if they were obtained
 * successfully.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_TokenGetNew(const char *cellName, const char *principal,
		      const char *password, void **tokenHandle,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_token_handle_p t_handle =
	(afs_token_handle_p) calloc(1, sizeof(afs_token_handle_t));

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_TokenGetNew;
    }

    if (t_handle == NULL) {
	tst = ADMNOMEM;
	goto fail_afsclient_TokenGetNew;
    }

    /*
     * Check to see if the principal or password is missing.  If it is,
     * get unauthenticated tokens for the cell
     */

    if ((principal == NULL) || (*principal == 0) || (password == NULL)
	|| (*password == 0)) {
	t_handle->from_kernel = 0;
	t_handle->afs_token_set = 1;
	t_handle->kas_token_set = 1;
	t_handle->sc_index = 0;
	t_handle->afs_sc[t_handle->sc_index] =
	    rxnull_NewClientSecurityObject();
	t_handle->afs_encrypt_sc[t_handle->sc_index] =
	    rxnull_NewClientSecurityObject();
	t_handle->kas_sc[t_handle->sc_index] =
	    rxnull_NewClientSecurityObject();
	t_handle->begin_magic = BEGIN_MAGIC;
	t_handle->is_valid = 1;
	t_handle->afs_token.endTime = 0;
	t_handle->end_magic = END_MAGIC;
	*tokenHandle = (void *)t_handle;

    } else {

	/*
	 * create an authenticated token
	 */

	if ((GetAFSToken(cellName, principal, password, t_handle, &tst))
	    && (GetKASToken(cellName, principal, password, t_handle, &tst))) {
	    strncpy(t_handle->cell, cellName, MAXCELLCHARS);
            t_handle->cell[MAXCELLCHARS - 1] = '\0';
	    t_handle->from_kernel = 0;
	    t_handle->afs_token_set = 1;
	    t_handle->kas_token_set = 1;
	    t_handle->sc_index = 2;
	    t_handle->afs_sc[t_handle->sc_index] =
		rxkad_NewClientSecurityObject(rxkad_clear,
					      &t_handle->afs_token.sessionKey,
					      t_handle->afs_token.kvno,
					      t_handle->afs_token.ticketLen,
					      t_handle->afs_token.ticket);
	    t_handle->afs_encrypt_sc[t_handle->sc_index] =
		rxkad_NewClientSecurityObject(rxkad_crypt,
					      &t_handle->afs_token.sessionKey,
					      t_handle->afs_token.kvno,
					      t_handle->afs_token.ticketLen,
					      t_handle->afs_token.ticket);
	    t_handle->kas_sc[t_handle->sc_index] =
		rxkad_NewClientSecurityObject(rxkad_crypt,
					      &t_handle->kas_token.sessionKey,
					      t_handle->kas_token.kvno,
					      t_handle->kas_token.ticketLen,
					      t_handle->kas_token.ticket);
	    if ((t_handle->afs_sc[t_handle->sc_index] != NULL)
		&& (t_handle->afs_encrypt_sc[t_handle->sc_index] != NULL)
		&& (t_handle->kas_sc[t_handle->sc_index] != NULL)) {
		t_handle->begin_magic = BEGIN_MAGIC;
		t_handle->is_valid = 1;
		t_handle->end_magic = END_MAGIC;
		*tokenHandle = (void *)t_handle;
	    } else {
		tst = ADMCLIENTTOKENHANDLENOSECURITY;
		goto fail_afsclient_TokenGetNew;
	    }
	} else {
	    goto fail_afsclient_TokenGetNew;
	}
    }
    rc = 1;

  fail_afsclient_TokenGetNew:

    if ((rc == 0) && (t_handle != NULL)) {
	free(t_handle);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_TokenQuery - get the expiration time of the tokens.
 *
 * PARAMETERS
 *
 * IN tokenHandle - a previously obtained valid token.
 * 
 * OUT expirationDateP - the time at which the tokens expire.
 * 
 * OUT principal - the owning principal
 * 
 * OUT instance - principal instance if it exists.
 * 
 * OUT cell - the principal's cell
 * 
 * OUT hasKasTokens - set to 1 if the token handle contains kas tokens.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * We only check the AFS tokens since we always get these.  The
 * KAS tokens may expirer later than the AFS tokens, but this 
 * difference is minor and reporting an earlier time won't cause
 * the user problems.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_TokenQuery(void *tokenHandle, unsigned long *expirationDateP,
		     char *principal, char *instance, char *cell,
		     int *hasKasTokens, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_token_handle_p t_handle = (afs_token_handle_p) tokenHandle;

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	rc = 0;
	goto fail_afsclient_TokenQuery;
    }

    if (IsTokenValid(t_handle, &tst)) {
	if (principal != NULL) {
	    strcpy(principal, t_handle->client.name);
	}
	if (instance != NULL) {
	    strcpy(instance, t_handle->client.instance);
	}
	if (cell != NULL) {
	    strcpy(cell, t_handle->client.cell);
	}
	if (hasKasTokens != NULL) {
	    *hasKasTokens = t_handle->kas_token_set;
	}
	if (expirationDateP != NULL) {
	    *expirationDateP = t_handle->afs_token.endTime;
	}
	rc = 1;
    }

  fail_afsclient_TokenQuery:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_TokenClose - close an existing token.
 *
 * PARAMETERS
 *
 * IN token - the token to be closed.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_TokenClose(const void *tokenHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_token_handle_p t_handle = (afs_token_handle_p) tokenHandle;

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_TokenClose;
    }

    if (IsTokenValid(t_handle, &tst)) {
	t_handle->is_valid = 0;
	free(t_handle);
	rc = 1;
    }

  fail_afsclient_TokenClose:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

#define NUM_SERVER_TYPES 3

/* must match NUM_SERVER_TYPES */
typedef enum { KAS, PTS, VOS } afs_server_list_t;

typedef struct afs_server {
    char *serv;
    int serviceId;
    struct ubik_client **ubik;
    struct rx_securityClass *sc;
    int *valid;
} afs_server_t, *afs_server_p;

/*
 * afsclient_CellOpen - Open a particular cell for work as a particular
 * user.
 *
 * PARAMETERS
 *
 * IN cellName - the cell where future admin calls will be made.
 *
 * IN tokenHandle - the tokens work will be done under.
 * 
 * OUT cellHandleP - an opaque pointer that is the first parameter to
 * almost all subsequent admin api calls.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_CellOpen(const char *cellName, const void *tokenHandle,
		   void **cellHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_token_handle_p t_handle = (afs_token_handle_p) tokenHandle;
    afs_cell_handle_p c_handle = (afs_cell_handle_p)
	calloc(1, sizeof(afs_cell_handle_t));
    struct afsconf_dir *tdir = NULL;
    struct afsconf_cell info;
    struct rx_connection *serverconns[MAXSERVERS];
    int i, j;
    struct rx_securityClass *sc[3];
    int scIndex;
    char copyCell[MAXCELLCHARS];

    afs_server_t servers[NUM_SERVER_TYPES]
      = { {AFSCONF_KAUTHSERVICE, KA_MAINTENANCE_SERVICE, 0, 0, 0},
	  {AFSCONF_PROTSERVICE, PRSRV, 0, 0, 0},
	  {AFSCONF_VLDBSERVICE, USER_SERVICE_ID, 0, 0, 0}
      };
    
    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_CellOpen;
    }

    if (c_handle == NULL) {
	tst = ADMNOMEM;
	goto fail_afsclient_CellOpen;
    }

    if (t_handle == NULL) {
	tst = ADMCLIENTTOKENHANDLENULL;
	goto fail_afsclient_CellOpen;
    }

    if ((cellName == NULL) || (*cellName == 0)) {
	tst = ADMCLIENTCELLNAMENULL;
	goto fail_afsclient_CellOpen;
    }

    if (cellHandleP == NULL) {
	tst = ADMCLIENTCELLHANDLEPNULL;
	goto fail_afsclient_CellOpen;
    }

    /*
     * Check that the token handle contains valid data and the calloc 
     * succeeded
     */
    if (!t_handle->afs_token_set) {
	tst = ADMCLIENTCELLOPENBADTOKEN;
	goto fail_afsclient_CellOpen;
    }

    /*
     * Use a table to initialize the cell handle structure, since
     * most of the steps are the same for all the servers.
     * 
     * Start by creating rx_securityClass objects for each of the
     * servers.  A potential optimization is to do this in 
     * afsclient_TokenGetNew and just keep the expiration time of
     * the tokens around.
     * Also, initialize the ubik client pointers in the table
     */
    servers[KAS].sc = t_handle->kas_sc[t_handle->sc_index];
    servers[PTS].sc = t_handle->afs_sc[t_handle->sc_index];
    servers[VOS].sc = servers[PTS].sc;
    servers[KAS].ubik = &c_handle->kas;
    servers[PTS].ubik = &c_handle->pts;
    servers[VOS].ubik = &c_handle->vos;
    servers[KAS].valid = &c_handle->kas_valid;
    servers[PTS].valid = &c_handle->pts_valid;
    servers[VOS].valid = &c_handle->vos_valid;
    c_handle->vos_new = 1;

    if ((servers[PTS].sc == NULL) || (servers[VOS].sc == NULL)) {
	tst = ADMCLIENTBADTOKENHANDLE;
	goto fail_afsclient_CellOpen;
    }

    /*
     * If the initialization has succeeded so far, get the address
     * information for each server in the cell
     */

    strncpy(c_handle->working_cell, cellName, MAXCELLCHARS);
    c_handle->working_cell[MAXCELLCHARS - 1] = '\0';
    if (!(tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))) {
	tst = ADMCLIENTBADCLIENTCONFIG;
	goto fail_afsclient_CellOpen;
    }

    /*
     * We must copy the cellName here because afsconf_GetCellInfo
     * actually writes over the cell name it is passed.
     */
    strncpy(copyCell, cellName, MAXCELLCHARS);
    copyCell[MAXCELLCHARS - 1] ='\0';
    for (i = 0; (i < NUM_SERVER_TYPES); i++) {
	if (i == KAS) {
	    tst =
		ka_AuthServerConn((char *)cellName, servers[i].serviceId,
				  ((t_handle->sc_index == 0)
				   || (!t_handle->
				       kas_token_set)) ? 0 : &t_handle->
				  kas_token, servers[i].ubik);
	    if (tst) {
		goto fail_afsclient_CellOpen;
	    }
	} else {
	    tst = afsconf_GetCellInfo(tdir, copyCell, servers[i].serv, &info);
	    if (!tst) {
		/* create ubik client handles for each server */
		scIndex = t_handle->sc_index;
		sc[scIndex] = servers[i].sc;
		for (j = 0; (j < info.numServers); j++) {
		    serverconns[j] =
			rx_GetCachedConnection(info.hostAddr[j].sin_addr.
					       s_addr,
					       info.hostAddr[j].sin_port,
					       servers[i].serviceId,
					       sc[scIndex], scIndex);
		}
		serverconns[j] = 0;
		tst = ubik_ClientInit(serverconns, servers[i].ubik);
		if (tst) {
		    goto fail_afsclient_CellOpen;
		}
	    } else {
		goto fail_afsclient_CellOpen;
	    }
	}
	/* initialization complete, mark handle valid */
	*servers[i].valid = 1;
    }
    c_handle->tokens = t_handle;
    rc = 1;

  fail_afsclient_CellOpen:

    if (tdir) {
	afsconf_Close(tdir);
    }

    /*
     * Upon error, free any obtained resources.
     */
    if (rc == 0) {
	if (c_handle != NULL) {
	    if (c_handle->kas_valid)
		ubik_ClientDestroy(c_handle->kas);
	    if (c_handle->pts_valid)
		ubik_ClientDestroy(c_handle->pts);
	    if (c_handle->vos_valid)
		ubik_ClientDestroy(c_handle->vos);
	    free(c_handle);
	}
    } else {
	c_handle->begin_magic = BEGIN_MAGIC;
	c_handle->is_valid = 1;
	c_handle->is_null = 0;
	c_handle->server_list = NULL;
	c_handle->server_ttl = 0;
	c_handle->end_magic = END_MAGIC;
	*cellHandleP = (void *)c_handle;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_NullCellOpen - open a null cell handle for access.
 *
 * PARAMETERS
 * 
 * OUT cellHandleP - an opaque pointer that is the first parameter to
 * almost all subsequent admin api calls.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_NullCellOpen(void **cellHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p)
	calloc(1, sizeof(afs_cell_handle_t));


    /*
     * Validate parameters
     */

    if (cellHandleP == NULL) {
	tst = ADMCLIENTCELLHANDLEPNULL;
	goto fail_afsclient_NullCellOpen;
    }

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_NullCellOpen;
    }

    if (c_handle == NULL) {
	tst = ADMNOMEM;
	goto fail_afsclient_NullCellOpen;
    }

    /*
     * Get unauthenticated tokens for any cell
     */

    if (!afsclient_TokenGetNew(0, 0, 0, (void *)&c_handle->tokens, &tst)) {
	goto fail_afsclient_NullCellOpen;
    }

    c_handle->begin_magic = BEGIN_MAGIC;
    c_handle->is_valid = 1;
    c_handle->is_null = 1;
    c_handle->end_magic = END_MAGIC;
    c_handle->kas_valid = 0;
    c_handle->pts_valid = 0;
    c_handle->vos_valid = 0;
    c_handle->kas = NULL;
    c_handle->pts = NULL;
    c_handle->vos = NULL;
    c_handle->server_list = NULL;
    c_handle->server_ttl = 0;
    *cellHandleP = (void *)c_handle;
    rc = 1;

  fail_afsclient_NullCellOpen:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_CellClose - close a previously opened cellHandle.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle created by afsclient_CellOpen.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_CellClose(const void *cellHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_CellClose;
    }

    if (c_handle == NULL) {
	tst = ADMCLIENTCELLHANDLENULL;
	goto fail_afsclient_CellClose;
    }

    if (c_handle->server_list)
	free(c_handle->server_list);
    if (c_handle->kas_valid)
	ubik_ClientDestroy(c_handle->kas);
    if (c_handle->pts_valid)
	ubik_ClientDestroy(c_handle->pts);
    if (c_handle->vos_valid)
	ubik_ClientDestroy(c_handle->vos);
    if (c_handle->is_null)
	afsclient_TokenClose(c_handle->tokens, 0);
    c_handle->kas_valid = 0;
    c_handle->pts_valid = 0;
    c_handle->vos_valid = 0;
    c_handle->is_valid = 0;
    free(c_handle);
    rc = 1;

  fail_afsclient_CellClose:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * afsclient_CellNameGet() -- get a pointer to the cell name in a cell handle
 *
 * PARAMETERS
 *
 * IN  cellHandle - a valid cell handle
 * OUT cellNameP  - a pointer to the cell name in the cell handle.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * If cellHandle is closed then the pointer returned by this function
 * is no longer valid.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */
int ADMINAPI
afsclient_CellNameGet(const void *cellHandle, const char **cellNameP,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    if (!CellHandleIsValid(cellHandle, &tst)) {
	goto fail_afsclient_CellNameGet;
    }

    *cellNameP = c_handle->working_cell;
    rc = 1;

  fail_afsclient_CellNameGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * afsclient_LocalCellGet - get the name of the cell the machine
 * belongs to where this process is running.
 *
 * PARAMETERS
 *
 * OUT cellName - an array of characters that must be MAXCELLCHARS
 * long.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * If cellName is smaller than MAXCELLCHARS chars, this function won't
 * detect it.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_LocalCellGet(char *cellName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct afsconf_dir *tdir = NULL;

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_LocalCellGet;
    }

    if (cellName == NULL) {
	tst = ADMCLIENTCELLNAMENULL;
	goto fail_afsclient_LocalCellGet;
    }

    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);

    if (!tdir) {
	tst = ADMCLIENTBADCLIENTCONFIG;
	goto fail_afsclient_LocalCellGet;
    }

    if ((tst = afsconf_GetLocalCell(tdir, cellName, MAXCELLCHARS))) {
	goto fail_afsclient_LocalCellGet;
    }

    rc = 1;

  fail_afsclient_LocalCellGet:

    if (tdir != NULL) {
	afsconf_Close(tdir);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


#ifdef AFS_NT40_ENV

static int
client_ExtractDriveLetter(char *path)
{
    int rc = 0;

    if (path[0] != 0 && path[1] == ':') {
	path[2] = 0;
	rc = 1;
    }

    return rc;
}

/*
 * Determine the parent directory of a give directory
 */

static int
Parent(char *directory, char *parentDirectory)
{
    register char *tp;
    int rc = 0;

    strcpy(parentDirectory, directory);
    tp = strrchr(parentDirectory, '\\');
    if (tp) {
	/* lv trailing slash so Parent("k:\foo") is "k:\" not "k :" */
	*(tp + 1) = 0;
	rc = 1;
    } else {
	if (client_ExtractDriveLetter(parentDirectory)) {
	    strcat(parentDirectory, ".");
	    rc = 1;
	}
    }

    return rc;
}

#else
/*
 * Determine the parent directory of a give directory
 */
static int
Parent(const char *directory, char *parentDirectory)
{
    char *tp;
    int rc = 0;

    strcpy(parentDirectory, directory);
    tp = strrchr(parentDirectory, '/');
    if (tp) {
	*tp = 0;
	rc = 1;
    } else {
	strcpy(parentDirectory, ".");
	rc = 1;
    }

    return rc;
}
#endif

/*
 * afsclient_MountPointCreate - create a mount point for a volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a handle to the cell where volumeName resides.
 *
 * IN directory - the directory where the mountpoint should be created.
 *
 * IN volumeName - the name of the volume to mount.
 *
 * IN volType - the type of mount point to create.
 *
 * IN volCheck - indicates whether or not to check the VLDB to see if
 * volumeName exists.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

#define TMP_DATA_SIZE 2048

int ADMINAPI
afsclient_MountPointCreate(const void *cellHandle, const char *directory,
			   const char *volumeName, vol_type_t volType,
			   vol_check_t volCheck, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    char parent_dir[TMP_DATA_SIZE];
    char space[TMP_DATA_SIZE];
    char directoryCell[MAXCELLCHARS];
    struct ViceIoctl idata;
    int i;
    vos_vldbEntry_t vldbEntry;

    /*
     * Validate arguments
     */

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_MountPointCreate;
    }

    if ((directory == NULL) || (*directory == 0)) {
	tst = ADMCLIENTDIRECTORYNULL;
	goto fail_afsclient_MountPointCreate;
    }

    if ((volumeName == NULL) || (*volumeName == 0)) {
	tst = ADMCLIENTVOLUMENAME;
	goto fail_afsclient_MountPointCreate;
    }

    /*
     * Extract the parent directory and make sure it is in AFS.
     */

    if (!Parent(directory, parent_dir)) {
	tst = ADMCLIENTBADDIRECTORY;
	goto fail_afsclient_MountPointCreate;
    }

    idata.in_size = 0;
    idata.out_size = TMP_DATA_SIZE;
    idata.out = space;
    i = pioctl(parent_dir, VIOC_FILE_CELL_NAME, &idata, 1);
    if (i) {
	if ((errno == EINVAL) || (errno == ENOENT)) {
	    tst = ADMCLIENTNOAFSDIRECTORY;
	    goto fail_afsclient_MountPointCreate;
	}
    }
    strcpy(directoryCell, space);

    /*
     * If the user requested, check that the volume exists
     */

    if (volCheck == CHECK_VOLUME) {
	if (!vos_VLDBGet(cellHandle, 0, 0, volumeName, &vldbEntry, &tst)) {
	    goto fail_afsclient_MountPointCreate;
	}
    }

    /*
     * Begin constructing the pioctl buffer
     */

    if (volType == READ_WRITE) {
	strcpy(space, "%");
    } else {
	strcpy(space, "#");
    }

    /*
     * Append the cell to the mount point if the volume is in a different
     * cell than the directory
     */

    if (strcmp(c_handle->working_cell, directoryCell)) {
	strcat(space, c_handle->working_cell);
	strcat(space, ":");
    }
    strcat(space, volumeName);
    strcat(space, ".");


#ifdef AFS_NT40_ENV
    idata.out_size = 0;
    idata.out = NULL;
    idata.in_size = 1 + strlen(space);
    idata.in = space;
    if (tst = pioctl(directory, VIOC_AFS_CREATE_MT_PT, &idata, 0)) {
	goto fail_afsclient_MountPointCreate;
    }
#else
    if ((tst = symlink(space, directory))) {
	goto fail_afsclient_MountPointCreate;
    }
#endif

    rc = 1;

  fail_afsclient_MountPointCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

typedef struct Acl {
    int dfs;
    char cell[1025];
    int nplus;
    int nminus;
} Acl_t, *Acl_p;

int ADMINAPI
afsclient_ACLEntryAdd(const char *directory, const char *user,
		      const acl_p acl, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct ViceIoctl idata;
    char old_acl_string[2048];
    char new_acl_string[2048];
    int newacl = 0;
    char *ptr;
    Acl_t cur_acl;
    char cur_user[64];
    int cur_user_acl = 0;
    int i;
    char tmp[64 + 35];
    int is_dfs;

    if (client_init == 0) {
	tst = ADMCLIENTNOINIT;
	goto fail_afsclient_ACLEntryAdd;
    }

    if ((directory == NULL) || (*directory == 0)) {
	tst = ADMMISCDIRECTORYNULL;
	goto fail_afsclient_ACLEntryAdd;
    }

    if ((user == NULL) || (*user == 0)) {
	tst = ADMMISCUSERNULL;
	goto fail_afsclient_ACLEntryAdd;
    }

    if (acl == NULL) {
	tst = ADMMISCACLNULL;
	goto fail_afsclient_ACLEntryAdd;
    }

    if (acl->read == READ) {
	newacl |= 0x01;
    }

    if (acl->write == WRITE) {
	newacl |= 0x02;
    }

    if (acl->insert == INSERT) {
	newacl |= 0x04;
    }

    if (acl->lookup == LOOKUP) {
	newacl |= 0x08;
    }

    if (acl->del == DELETE) {
	newacl |= 0x10;
    }

    if (acl->lock == LOCK) {
	newacl |= 0x20;
    }

    if (acl->admin == ADMIN) {
	newacl |= 0x40;
    }

    /*
     * Get the current acl for the directory
     */

    idata.out_size = 2048;
    idata.in_size = 0;
    idata.in = idata.out = old_acl_string;
    tst = pioctl(directory, VIOCGETAL, &idata, 1);

    if (tst != 0) {
	goto fail_afsclient_ACLEntryAdd;
    }

    /*
     * The acl is presented to us in string format.  The format of the
     * string is:
     *
     * A header which contains the number of positive and negative entries
     * and a string indicating whether or not this is a dfs acl:
     *
     * num_pos "\n" dfs_string "\n" num_neg
     *
     * An entry for each acl that's of the form:
     *
     * name rights "\n"
     *
     * There are no blanks in the string between fields, but I use them here
     * to make the reading easier.
     *
     * Since we are only going to add another entry to the acl, our approach
     * is simple.  Get the num_pos dfs_string and num_neg from the current acl,
     * increment num_pos by one and create a new string.  Concatenate the new
     * user and rights to the new string, and then concatenate the remaining
     * contents of the old acl to the new string.
     *
     * Unfortunately, this approach doesn't work since the format the kernel
     * hands the acl back to us in, is NOT WHAT IT WANTS BACK!!!!
     * So instead we need to parse the entire freaking acl and put a space
     * between each user and their acl.
     *
     * This is really ugly.
     */

    /*
     * Parse the first few fields of the acl and see if this is a DFS
     * file.
     */

    is_dfs =
	sscanf(old_acl_string, "%d dfs:%d %s", &cur_acl.nplus, &cur_acl.dfs,
	       cur_acl.cell);
    ptr = strchr(old_acl_string, '\n');
    ptr++;
    sscanf(ptr, "%d", &cur_acl.nminus);
    ptr = strchr(ptr, '\n');
    ptr++;
    if (is_dfs == 3) {
	tst = ADMMISCNODFSACL;
	goto fail_afsclient_ACLEntryAdd;
    } else {
	/*
	 * It isn't a DFS file, so create the beginning of the string
	 * we will hand back to the kernel
	 */
	sprintf(new_acl_string, "%d\n%d\n%s %d\n", (cur_acl.nplus + 1),
		cur_acl.nminus, user, newacl);
    }

    /*
     * Finish scanning the old acl, parsing each user/acl pair and
     * adding a space in the new acl.
     */

    for (i = 0; i < (cur_acl.nplus + cur_acl.nminus); i++) {
	sscanf(ptr, "%s%d\n", cur_user, &cur_user_acl);
	/*
	 * Skip the entry for the user we are replacing/adding
	 */

	if (strcmp(cur_user, user)) {
	    ptr = strchr(ptr, '\n');
	    ptr++;
	    sprintf(tmp, "%s %d\n", cur_user, cur_user_acl);
	    strcat(new_acl_string, tmp);
	}
    }

    strcat(new_acl_string, ptr);

    /*
     * Set the acl
     */

    idata.out_size = 0;
    idata.in_size = strlen(new_acl_string) + 1;
    idata.in = idata.out = new_acl_string;
    tst = pioctl(directory, VIOCSETAL, &idata, 1);

    if (tst != 0) {
	goto fail_afsclient_ACLEntryAdd;
    }
    rc = 1;

  fail_afsclient_ACLEntryAdd:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_Init - initialize AFS components before use.
 *
 * PARAMETERS
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_Init(afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (!client_init)
	pthread_once(&client_init_once, client_once);

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	tst = ADMCLIENTCANTINITWINSOCK;
	goto fail_afsclient_Init;
    }
#endif

    if (!(initAFSDirPath() & AFSDIR_CLIENT_PATHS_OK)) {
	tst = ADMCLIENTCANTINITAFSLOCATION;
	goto fail_afsclient_Init;
    }

    if (rx_Init(0) < 0) {
	tst = ADMCLIENTCANTINITRX;
	goto fail_afsclient_Init;
    }

    if ((tst = ka_CellConfig((char *)AFSDIR_CLIENT_ETC_DIRPATH))) {
	goto fail_afsclient_Init;
    }

    rc = 1;

  fail_afsclient_Init:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_AFSServerGet - determine what kind of server serverName 
 * is and fill in serverEntryP accordingly.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverName - the hostname of the server of interest.
 *
 * OUT serverEntryP - upon successful completion contains a description of
 * the server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_AFSServerGet(const void *cellHandle, const char *serverName,
		       afs_serverEntry_p serverEntryP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    void *iter;
    int found_match = 0;

    if ((serverName == NULL) || (*serverName == 0)) {
	tst = ADMUTILSERVERNAMENULL;
	goto fail_afsclient_AFSServerGet;
    }

    if (serverEntryP == NULL) {
	tst = ADMUTILSERVERENTRYPNULL;
	goto fail_afsclient_AFSServerGet;
    }

    /*
     * Iterate over server entries and try to find a match for serverName
     */

    if (!afsclient_AFSServerGetBegin(cellHandle, &iter, &tst)) {
	goto fail_afsclient_AFSServerGet;
    }

    while (afsclient_AFSServerGetNext(iter, serverEntryP, &tst)) {
	if (!strcmp(serverName, serverEntryP->serverName)) {
	    found_match = 1;
	    break;
	}
    }

    /*
     * If we didn't find a match, the iterator should have terminated
     * normally.  If it didn't, return the error
     */

    if (!found_match) {
	if (tst != ADMITERATORDONE) {
	    afsclient_AFSServerGetDone(iter, 0);
	} else {
	    afsclient_AFSServerGetDone(iter, &tst);
	}
	tst = ADMCLIENTNOMATCHINGSERVER;
	goto fail_afsclient_AFSServerGet;
    } else {
	if (!afsclient_AFSServerGetDone(iter, &tst)) {
	    goto fail_afsclient_AFSServerGet;
	}
    }
    rc = 1;

  fail_afsclient_AFSServerGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the server retrieval functions
 */

typedef struct server_get {
    int total;
    int index;
    afs_serverEntry_t server[MAXHOSTSPERCELL + BADSERVERID];
    afs_serverEntry_t cache[CACHED_ITEMS];
} server_get_t, *server_get_p;

static int
GetServerRPC(void *rpc_specific, int slot, int *last_item,
	     int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    server_get_p serv = (server_get_p) rpc_specific;

    memcpy(&serv->cache[slot], &serv->server[serv->index],
	   sizeof(afs_serverEntry_t));

    serv->index++;
    if (serv->index == serv->total) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetServerFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    server_get_p serv = (server_get_p) rpc_specific;

    memcpy(dest, (const void *)&serv->cache[slot], sizeof(afs_serverEntry_t));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_AFSServerGetBegin - start the process of iterating over
 * every server in the cell.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * OUT iterationIdP - - upon successful completion contains an iterator
 * that can be passed to afsclient_AFSServerGetNext.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_AFSServerGetBegin(const void *cellHandle, void **iterationIdP,
			    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    server_get_p serv = (server_get_p) calloc(1, sizeof(server_get_t));
    server_get_p serv_cache = NULL;
    const char *cellName;
    void *database_iter;
    util_databaseServerEntry_t database_entry;
    void *fileserver_iter;
    vos_fileServerEntry_t fileserver_entry;
    int iserv, iservaddr, ientryaddr, is_dup;
    struct hostent *host;

    if (!CellHandleIsValid(c_handle, &tst)) {
	goto fail_afsclient_AFSServerGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_afsclient_AFSServerGetBegin;
    }

    if ((serv == NULL) || (iter == NULL)) {
	tst = ADMNOMEM;
	goto fail_afsclient_AFSServerGetBegin;
    }

  restart:
    LOCK_GLOBAL_MUTEX;
    if (c_handle->server_list != NULL && c_handle->server_ttl < time(NULL)) {
	serv_cache = c_handle->server_list;
	c_handle->server_list = NULL;
    }
    UNLOCK_GLOBAL_MUTEX;

    if (c_handle->server_list == NULL) {
	if (serv_cache == NULL) {
	    serv_cache = (server_get_p) calloc(1, sizeof(server_get_t));

	    if (serv_cache == NULL) {
		tst = ADMNOMEM;
		goto fail_afsclient_AFSServerGetBegin;
	    }
	}

	/*
	 * Retrieve the list of database servers for this cell.
	 */

	if (!afsclient_CellNameGet(c_handle, &cellName, &tst)) {
	    goto fail_afsclient_AFSServerGetBegin;
	}

	if (!util_DatabaseServerGetBegin(cellName, &database_iter, &tst)) {
	    goto fail_afsclient_AFSServerGetBegin;
	}

	while (util_DatabaseServerGetNext(database_iter, &database_entry, &tst)) {
	    serv->server[serv->total].serverAddress[0] =
		database_entry.serverAddress;
	    serv->server[serv->total].serverType = DATABASE_SERVER;
	    serv->total++;
	}

	if (tst != ADMITERATORDONE) {
	    util_DatabaseServerGetDone(database_iter, 0);
	    goto fail_afsclient_AFSServerGetBegin;
	}

	if (!util_DatabaseServerGetDone(database_iter, &tst)) {
	    goto fail_afsclient_AFSServerGetBegin;
	}

	/*
 	 * Retrieve the list of file servers for this cell.
	 */

	if (!vos_FileServerGetBegin(c_handle, 0, &fileserver_iter, &tst)) {
	    goto fail_afsclient_AFSServerGetBegin;
	}

	while (vos_FileServerGetNext(fileserver_iter, &fileserver_entry, &tst)) {
	    /*
	     * See if any of the addresses returned in this fileserver_entry
	     * structure already exist in the list of servers we're building.
	     * If not, create a new record for this server.
	     */
	    is_dup = 0;
	    for (iserv = 0; iserv < serv->total; iserv++) {
		for (ientryaddr = 0; ientryaddr < fileserver_entry.count; ientryaddr++) {
		    for (iservaddr = 0; iservaddr < AFS_MAX_SERVER_ADDRESS; iservaddr++) {
			if (serv->server[iserv].serverAddress[iservaddr] ==
			     fileserver_entry.serverAddress[ientryaddr]) {
			    is_dup = 1;
			    break;
			}
		    }
		    if (is_dup) {
			break;
		    }
		}
		if (is_dup) {
		    break;
		}
	    }

	    if (is_dup) {
		serv->server[iserv].serverType |= FILE_SERVER;
	    } else {
		iserv = serv->total++;
		serv->server[iserv].serverType = FILE_SERVER;
	    }

	    /*
	     * Add the addresses from the vldb list to the serv->server[iserv]
	     * record.  Remember that VLDB's list-of-addrs is not guaranteed
	     * to be unique in a particular entry, or to return only one entry
	     * per machine--so when we add addresses, always check for
	     * duplicate entries.
	     */

	    for (ientryaddr = 0; ientryaddr < fileserver_entry.count; ientryaddr++) {
		for (iservaddr = 0; iservaddr < AFS_MAX_SERVER_ADDRESS; iservaddr++) {
		    if (serv->server[iserv].serverAddress[iservaddr] ==
			 fileserver_entry.serverAddress[ientryaddr]) {
			break;
		    }
		}
		if (iservaddr == AFS_MAX_SERVER_ADDRESS) {
		    for (iservaddr = 0; iservaddr < AFS_MAX_SERVER_ADDRESS;
			  iservaddr++) {
			if (!serv->server[iserv].serverAddress[iservaddr]) {
			    serv->server[iserv].serverAddress[iservaddr] =
				fileserver_entry.serverAddress[ientryaddr];
			    break;
			}
		    }
		}
	    }
	}

	if (tst != ADMITERATORDONE) {
	    vos_FileServerGetDone(fileserver_iter, 0);
	    goto fail_afsclient_AFSServerGetBegin;
	}

	if (!vos_FileServerGetDone(fileserver_iter, &tst)) {
	    goto fail_afsclient_AFSServerGetBegin;
	}

	/*
	 * Iterate over the list and fill in the hostname of each of the servers
	 */

	for (iserv = 0; iserv < serv->total; iserv++) {
	    int addr = htonl(serv->server[iserv].serverAddress[0]);
	    LOCK_GLOBAL_MUTEX;
	    host = gethostbyaddr((const char *)&addr, sizeof(int), AF_INET);
	    if (host != NULL) {
		strncpy(serv->server[iserv].serverName, host->h_name,
			 AFS_MAX_SERVER_NAME_LEN);
		serv->server[iserv].serverName[AFS_MAX_SERVER_NAME_LEN - 1] = '\0';
	    }
	    UNLOCK_GLOBAL_MUTEX;
	}
    
	memcpy(serv_cache, serv, sizeof(server_get_t));
    } else {
	int race = 0;
	LOCK_GLOBAL_MUTEX;
	if (c_handle->server_list == NULL)
	    race = 1;
	else
	    memcpy(serv, c_handle->server_list, sizeof(server_get_t));
	UNLOCK_GLOBAL_MUTEX;
	if (race)
	    goto restart;
    }

    if (IteratorInit
	    (iter, (void *)serv, GetServerRPC, GetServerFromCache, NULL, NULL,
	     &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_afsclient_AFSServerGetBegin:

    if (rc == 0) {
	if (iter != NULL)
	    free(iter);
	if (serv != NULL)
	    free(serv);
	if (serv_cache != NULL)
	    free(serv_cache);
    } else {
	if (serv_cache) {
	    LOCK_GLOBAL_MUTEX;
	    /* in case there was a race and we constructed the list twice */
	    if (c_handle->server_list)
		free(c_handle->server_list);

	    c_handle->server_list = serv_cache;
	    c_handle->server_ttl = time(NULL) + SERVER_TTL;
	    UNLOCK_GLOBAL_MUTEX;
	}
    }

    if (st != NULL)
	*st = tst;

    return rc;
}

/*
 * afsclient_AFSServerGetNext - retrieve the next server in the cell.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * afsclient_AFSServerGetBegin.
 *
 * OUT serverEntryP - upon successful completion contains the next server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_AFSServerGetNext(void *iterationId, afs_serverEntry_p serverEntryP,
			   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_afsclient_AFSServerGetNext;
    }

    if (serverEntryP == NULL) {
	tst = ADMUTILSERVERENTRYPNULL;
	goto fail_afsclient_AFSServerGetNext;
    }

    rc = IteratorNext(iter, (void *)serverEntryP, &tst);

  fail_afsclient_AFSServerGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_AFSServerGetDone - finish using a server iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * afsclient_AFSServerGetBegin.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_AFSServerGetDone(void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_afsclient_AFSServerGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_afsclient_AFSServerGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_RPCStatOpen - open an rx connection to a server to retrieve
 * statistics.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle created by afsclient_CellOpen.
 *
 * IN serverName - the host name where the server resides.
 *
 * IN type - what type of process to query
 *
 * OUT rpcStatHandleP - contains an rx connection to the server of interest
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_RPCStatOpen(const void *cellHandle, const char *serverName,
		      afs_stat_source_t type,
		      struct rx_connection **rpcStatHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    int servAddr = 0;
    int servPort;
    struct rx_securityClass *sc;

    if (!CellHandleIsValid(cellHandle, &tst)) {
	goto fail_afsclient_RPCStatOpen;
    }

    if (!util_AdminServerAddressGetFromName(serverName, &servAddr, &tst)) {
	goto fail_afsclient_RPCStatOpen;
    }

    if (rpcStatHandleP == NULL) {
	tst = ADMCLIENTRPCSTATHANDLEPNULL;
	goto fail_afsclient_RPCStatOpen;
    }

    switch (type) {

    case AFS_BOSSERVER:
	servPort = AFSCONF_NANNYPORT;
	break;

    case AFS_FILESERVER:
	servPort = AFSCONF_FILEPORT;
	break;

    case AFS_KASERVER:
	servPort = AFSCONF_KAUTHPORT;
	break;

    case AFS_PTSERVER:
	servPort = AFSCONF_PROTPORT;
	break;

    case AFS_VOLSERVER:
	servPort = AFSCONF_VOLUMEPORT;
	break;

    case AFS_VLSERVER:
	servPort = AFSCONF_VLDBPORT;
	break;

    case AFS_CLIENT:
	servPort = AFSCONF_CALLBACKPORT;
	break;

    default:
	tst = ADMTYPEINVALID;
	goto fail_afsclient_RPCStatOpen;
    }

    /*
     * special processing of tokens by server type
     */

    if (type == AFS_KASERVER) {
	if (!c_handle->tokens->kas_token_set) {
	    tst = ADMCLIENTNOKASTOKENS;
	    goto fail_afsclient_RPCStatOpen;
	}
	sc = c_handle->tokens->kas_sc[c_handle->tokens->sc_index];
    } else {
	sc = c_handle->tokens->afs_sc[c_handle->tokens->sc_index];
    }

    *rpcStatHandleP =
	rx_GetCachedConnection(htonl(servAddr), htons(servPort),
			       RX_STATS_SERVICE_ID, sc,
			       c_handle->tokens->sc_index);

    if (*rpcStatHandleP == NULL) {
	tst = ADMCLIENTRPCSTATNOCONNECTION;
	goto fail_afsclient_RPCStatOpen;
    }
    rc = 1;

  fail_afsclient_RPCStatOpen:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_RPCStatOpenPort - open an rx connection to a server to retrieve
 * statistics.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle created by afsclient_CellOpen.
 *
 * IN serverName - the host name where the server resides.
 *
 * IN port - the UDP port number where the server resides.
 *
 * OUT rpcStatHandleP - contains an rx connection to the server of interest
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_RPCStatOpenPort(const void *cellHandle, const char *serverName,
			  const int serverPort,
			  struct rx_connection **rpcStatHandleP,
			  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    int servAddr = 0;
    struct rx_securityClass *sc;

    if (!CellHandleIsValid(cellHandle, &tst)) {
	goto fail_afsclient_RPCStatOpenPort;
    }

    if (!util_AdminServerAddressGetFromName(serverName, &servAddr, &tst)) {
	goto fail_afsclient_RPCStatOpenPort;
    }

    if (rpcStatHandleP == NULL) {
	tst = ADMCLIENTRPCSTATHANDLEPNULL;
	goto fail_afsclient_RPCStatOpenPort;
    }

    /*
     * special processing of tokens by server type
     */

    if (serverPort == AFSCONF_KAUTHPORT) {
	if (!c_handle->tokens->kas_token_set) {
	    tst = ADMCLIENTNOKASTOKENS;
	    goto fail_afsclient_RPCStatOpenPort;
	}
	sc = c_handle->tokens->kas_sc[c_handle->tokens->sc_index];
    } else {
	sc = c_handle->tokens->afs_sc[c_handle->tokens->sc_index];
    }

    *rpcStatHandleP =
	rx_GetCachedConnection(htonl(servAddr), htons(serverPort),
			       RX_STATS_SERVICE_ID, sc,
			       c_handle->tokens->sc_index);

    if (*rpcStatHandleP == NULL) {
	tst = ADMCLIENTRPCSTATNOCONNECTION;
	goto fail_afsclient_RPCStatOpenPort;
    }
    rc = 1;

  fail_afsclient_RPCStatOpenPort:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_RPCStatClose - close a previously opened rx connection.
 *
 * PARAMETERS
 *
 * IN rpcStatHandle - an rx connection returned by afsclient_RPCStatOpen
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_RPCStatClose(struct rx_connection *rpcStatHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (rpcStatHandle == NULL) {
	tst = ADMCLIENTRPCSTATHANDLEPNULL;
	goto fail_afsclient_RPCStatClose;
    }

    rx_ReleaseCachedConnection(rpcStatHandle);
    rc = 1;
  fail_afsclient_RPCStatClose:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_CMStatOpen - open an rx connection to a server to retrieve
 * statistics.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle created by afsclient_CellOpen.
 *
 * IN serverName - the host name where the server resides.
 *
 * OUT cmStatHandleP - contains an rx connection to the server of interest
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_CMStatOpen(const void *cellHandle, const char *serverName,
		     struct rx_connection **cmStatHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    int servAddr = 0;
    struct rx_securityClass *sc;

    if (!CellHandleIsValid(cellHandle, &tst)) {
	goto fail_afsclient_CMStatOpen;
    }

    if (!util_AdminServerAddressGetFromName(serverName, &servAddr, &tst)) {
	goto fail_afsclient_CMStatOpen;
    }

    if (cmStatHandleP == NULL) {
	tst = ADMCLIENTCMSTATHANDLEPNULL;
	goto fail_afsclient_CMStatOpen;
    }

    sc = c_handle->tokens->afs_sc[c_handle->tokens->sc_index];

    *cmStatHandleP =
	rx_GetCachedConnection(htonl(servAddr), htons(AFSCONF_CALLBACKPORT),
			       1, sc, c_handle->tokens->sc_index);

    if (*cmStatHandleP == NULL) {
	tst = ADMCLIENTCMSTATNOCONNECTION;
	goto fail_afsclient_CMStatOpen;
    }
    rc = 1;

  fail_afsclient_CMStatOpen:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_CMStatOpenPort - open an rx connection to a server to retrieve
 * statistics.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle created by afsclient_CellOpen.
 *
 * IN serverName - the host name where the server resides.
 *
 * IN port - the UDP port number where the server resides.
 *
 * OUT cmStatHandleP - contains an rx connection to the server of interest
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_CMStatOpenPort(const void *cellHandle, const char *serverName,
			 const int serverPort,
			 struct rx_connection **cmStatHandleP,
			 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    int servAddr = 0;
    struct rx_securityClass *sc;

    if (!CellHandleIsValid(cellHandle, &tst)) {
	goto fail_afsclient_CMStatOpenPort;
    }

    if (!util_AdminServerAddressGetFromName(serverName, &servAddr, &tst)) {
	goto fail_afsclient_CMStatOpenPort;
    }

    if (cmStatHandleP == NULL) {
	tst = ADMCLIENTCMSTATHANDLEPNULL;
	goto fail_afsclient_CMStatOpenPort;
    }

    sc = c_handle->tokens->afs_sc[c_handle->tokens->sc_index];

    *cmStatHandleP =
	rx_GetCachedConnection(htonl(servAddr), htons(serverPort), 1, sc,
			       c_handle->tokens->sc_index);

    if (*cmStatHandleP == NULL) {
	tst = ADMCLIENTCMSTATNOCONNECTION;
	goto fail_afsclient_CMStatOpenPort;
    }
    rc = 1;

  fail_afsclient_CMStatOpenPort:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_CMStatClose - close a previously opened rx connection.
 *
 * PARAMETERS
 *
 * IN cmStatHandle - an rx connection returned by afsclient_CMStatOpen
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_CMStatClose(struct rx_connection *cmStatHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (cmStatHandle == NULL) {
	tst = ADMCLIENTCMSTATHANDLEPNULL;
	goto fail_afsclient_CMStatClose;
    }

    rx_ReleaseCachedConnection(cmStatHandle);
    rc = 1;
  fail_afsclient_CMStatClose:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_RXDebugOpen - open an rxdebug handle to a server.
 *
 * PARAMETERS
 *
 * IN serverName - the host name where the server resides.
 *
 * IN type - what type of process to query
 *
 * OUT rxdebugHandle_p - contains an rxdebug handle for the server of interest
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_RXDebugOpen(const char *serverName, afs_stat_source_t type,
		      rxdebugHandle_p * rxdebugHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int code;
    rxdebugHandle_p handle;
    rxdebugSocket_t sock;
    struct sockaddr_in taddr;
    int serverPort;
    int serverAddr;

    if (rxdebugHandleP == NULL) {
	tst = ADMCLIENTRXDEBUGHANDLEPNULL;
	goto fail_afsclient_RXDebugOpen;
    }

    switch (type) {

    case AFS_BOSSERVER:
	serverPort = AFSCONF_NANNYPORT;
	break;

    case AFS_FILESERVER:
	serverPort = AFSCONF_FILEPORT;
	break;

    case AFS_KASERVER:
	serverPort = AFSCONF_KAUTHPORT;
	break;

    case AFS_PTSERVER:
	serverPort = AFSCONF_PROTPORT;
	break;

    case AFS_VOLSERVER:
	serverPort = AFSCONF_VOLUMEPORT;
	break;

    case AFS_VLSERVER:
	serverPort = AFSCONF_VLDBPORT;
	break;

    case AFS_CLIENT:
	serverPort = AFSCONF_CALLBACKPORT;
	break;

    default:
	tst = ADMTYPEINVALID;
	goto fail_afsclient_RXDebugOpen;
    }

    if (!util_AdminServerAddressGetFromName(serverName, &serverAddr, &tst)) {
	goto fail_afsclient_RXDebugOpen;
    }

    sock = (rxdebugSocket_t) socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_RXDEBUG_SOCKET) {
	tst = ADMSOCKFAIL;
	goto fail_afsclient_RXDebugOpen;
    }

    memset(&taddr, 0, sizeof(taddr));
    taddr.sin_family = AF_INET;
    taddr.sin_port = 0;
    taddr.sin_addr.s_addr = INADDR_ANY;
    code = bind(sock, (struct sockaddr *)&taddr, sizeof(taddr));
    if (code) {
	close(sock);
	tst = ADMSOCKFAIL;
	goto fail_afsclient_RXDebugOpen;
    }

    handle = (rxdebugHandle_p) malloc(sizeof(rxdebugHandle_t));
    if (!handle) {
	close(sock);
	tst = ADMNOMEM;
	goto fail_afsclient_RXDebugOpen;
    }

    handle->sock = sock;
    handle->ipAddr = serverAddr;
    handle->udpPort = serverPort;
    handle->firstFlag = 1;
    handle->supportedStats = 0;
    *rxdebugHandleP = handle;
    rc = 1;

  fail_afsclient_RXDebugOpen:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_RXDebugOpenPort - open an rxdebug handle to a server.
 *
 * PARAMETERS
 *
 * IN serverName - the host name where the server resides.
 *
 * IN port - the UDP port number where the server resides.
 *
 * OUT rxdebugHandle_p - contains an rxdebug handle for the server of interest
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_RXDebugOpenPort(const char *serverName, int serverPort,
			  rxdebugHandle_p * rxdebugHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int code;
    rxdebugHandle_p handle;
    rxdebugSocket_t sock;
    struct sockaddr_in taddr;
    int serverAddr;

    if (rxdebugHandleP == NULL) {
	tst = ADMCLIENTRXDEBUGHANDLEPNULL;
	goto fail_afsclient_RXDebugOpenPort;
    }

    if (!util_AdminServerAddressGetFromName(serverName, &serverAddr, &tst)) {
	goto fail_afsclient_RXDebugOpenPort;
    }

    sock = (rxdebugSocket_t) socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_RXDEBUG_SOCKET) {
	tst = ADMSOCKFAIL;
	goto fail_afsclient_RXDebugOpenPort;
    }

    memset(&taddr, 0, sizeof(taddr));
    taddr.sin_family = AF_INET;
    taddr.sin_port = 0;
    taddr.sin_addr.s_addr = INADDR_ANY;
    code = bind(sock, (struct sockaddr *)&taddr, sizeof(taddr));
    if (code) {
	close(sock);
	tst = ADMSOCKFAIL;
	goto fail_afsclient_RXDebugOpenPort;
    }

    handle = (rxdebugHandle_p) malloc(sizeof(rxdebugHandle_t));
    if (!handle) {
	close(sock);
	tst = ADMNOMEM;
	goto fail_afsclient_RXDebugOpenPort;
    }

    handle->sock = sock;
    handle->ipAddr = serverAddr;
    handle->udpPort = serverPort;
    handle->firstFlag = 1;
    handle->supportedStats = 0;
    *rxdebugHandleP = handle;
    rc = 1;

  fail_afsclient_RXDebugOpenPort:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * afsclient_RXDebugClose - close a previously opened rxdebug handle.
 *
 * PARAMETERS
 *
 * IN rxdebugHandle - an rxdebug handle returned by afsclient_RXDebugOpen
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
afsclient_RXDebugClose(rxdebugHandle_p rxdebugHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (rxdebugHandle == NULL) {
	tst = ADMCLIENTRXDEBUGHANDLEPNULL;
	goto fail_afsclient_RXDebugClose;
    }

    close(rxdebugHandle->sock);
    free(rxdebugHandle);
    rc = 1;
  fail_afsclient_RXDebugClose:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
