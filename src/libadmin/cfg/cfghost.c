/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file implements configuration functions in the following categories:
 *   cfg_Host*()         - manipulate static server configuration information.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>

#ifdef AFS_NT40_ENV
#include <WINNT/vptab.h>
#include <WINNT/afsreg.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif /* AFS_NT40_ENV */

#include <rx/rx.h>
#include <rx/rxstat.h>

#include <afs/afs_Admin.h>
#include <afs/afs_AdminErrors.h>
#include <afs/afs_utilAdmin.h>
#include <afs/afs_bosAdmin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_kasAdmin.h>
#include <afs/afs_ptsAdmin.h>


#include <afs/kautils.h>
#include <afs/bnode.h>
#include <afs/prerror.h>
#include <afs/keys.h>
#include <afs/dirpath.h>
#include <afs/cellconfig.h>

#include "cfginternal.h"
#include "afs_cfgAdmin.h"
#include "../adminutil/afs_AdminInternal.h"



/* Local declarations and definitions */

static int
  KasKeyIsZero(kas_encryptionKey_t * kasKey);

static int
  KasKeyEmbeddedInString(const char *keyString, kas_encryptionKey_t * kasKey);




/* ---------------- Exported Server Host functions ------------------ */


/*
 * cfg_HostQueryStatus() -- Query status of static server configuration
 *     on host, i.e., status of required configuration files, etc.
 *     Upon successful completion *configStP is set to the server
 *     configuration status, with a value of zero (0) indicating that
 *     the configuration is valid.
 *
 *     If server configuration is not valid then *cellNameP is set to NULL;
 *     otherwise, *cellNameP is an allocated buffer containing server cell.
 *
 *     Warning: in determining if server configuration is valid, no check
 *     is made for consistency with other servers in cell; also, the
 *     internal consistency of configuration files may not be verified.
 */
int ADMINAPI
cfg_HostQueryStatus(const char *hostName,	/* name of host */
		    afs_status_p configStP,	/* server config status */
		    char **cellNameP,	/* server's cell */
		    afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    afs_status_t serverSt = 0;
    char *serverCellName = NULL;

    /* validate parameters */

    if (hostName == NULL || *hostName == '\0') {
	tst = ADMCFGHOSTNAMENULL;
    } else if (strlen(hostName) > (MAXHOSTCHARS - 1)) {
	tst = ADMCFGHOSTNAMETOOLONG;
    } else if (configStP == NULL) {
	tst = ADMCFGCONFIGSTATUSPNULL;
    } else if (cellNameP == NULL) {
	tst = ADMCFGCELLNAMEPNULL;
    }

    /* remote configuration not yet supported; hostName must be local host */

    if (tst == 0) {
	short isLocal;

	if (!cfgutil_HostNameIsLocal(hostName, &isLocal, &tst2)) {
	    tst = tst2;
	} else if (!isLocal) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* check for existence and readability of required server config files */

    if (tst == 0) {
	int i;
	const char *cfgfile[4];

	cfgfile[0] = AFSDIR_SERVER_THISCELL_FILEPATH;
	cfgfile[1] = AFSDIR_SERVER_CELLSERVDB_FILEPATH;
	cfgfile[2] = AFSDIR_SERVER_KEY_FILEPATH;
	cfgfile[3] = AFSDIR_SERVER_ULIST_FILEPATH;

	for (i = 0; i < 4; i++) {
	    int fd;
	    if ((fd = open(cfgfile[i], O_RDONLY)) < 0) {
		break;
	    }
	    (void)close(fd);
	}

	if (i < 4) {
	    if (errno == EACCES) {
		tst = ADMNOPRIV;
	    } else {
		serverSt = ADMCFGSERVERBASICINFOINVALID;
	    }
	}
    }

    /* verify the required server config files to the degree possible */

    if (tst == 0 && serverSt == 0) {
	struct afsconf_dir *confdir;

	if ((confdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH)) == NULL) {
	    /* one or more config files appears to be invalid */
	    serverSt = ADMCFGSERVERBASICINFOINVALID;
	} else {
	    struct afsconf_entry *cellentry = NULL;

	    if (confdir->cellName == NULL || *confdir->cellName == '\0') {
		/* no cell set for server */
		serverSt = ADMCFGSERVERNOTINCELL;
	    } else if (confdir->keystr == NULL || confdir->keystr->nkeys == 0) {
		/* no server keys */
		serverSt = ADMCFGSERVERNOKEYS;
	    } else {
		for (cellentry = confdir->entries; cellentry != NULL;
		     cellentry = cellentry->next) {
		    if (!strcasecmp
			(confdir->cellName, cellentry->cellInfo.name)) {
			break;
		    }
		}

		if (cellentry == NULL) {
		    serverSt = ADMCFGSERVERCELLNOTINDB;
		} else if (cellentry->cellInfo.numServers <= 0) {
		    serverSt = ADMCFGSERVERCELLHASNODBENTRIES;
		}
	    }

	    if (tst == 0 && serverSt == 0) {
		/* everything looks good; malloc cell name buffer to return */
		serverCellName =
		    (char *)malloc(strlen(cellentry->cellInfo.name) + 1);
		if (serverCellName == NULL) {
		    tst = ADMNOMEM;
		} else {
		    strcpy(serverCellName, cellentry->cellInfo.name);
		}
	    }

	    (void)afsconf_Close(confdir);
	}
    }

    if (tst == 0) {
	/* return server status and cell name */
	*configStP = serverSt;

	if (serverSt == 0) {
	    *cellNameP = serverCellName;
	} else {
	    *cellNameP = NULL;
	}
    } else {
	/* indicate failure */
	rc = 0;

	/* free cell name if allocated before failure */
	if (serverCellName != NULL) {
	    free(serverCellName);
	}
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostOpen() -- Obtain host configuration handle.
 */
int ADMINAPI
cfg_HostOpen(void *cellHandle,	/* cell handle */
	     const char *hostName,	/* name of host to configure */
	     void **hostHandleP,	/* host config handle */
	     afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host;
    char fullHostName[MAXHOSTCHARS];

    /* validate parameters and resolve host name to fully qualified name */

    if (!CellHandleIsValid(cellHandle, &tst2)) {
	tst = tst2;
    } else if (hostName == NULL || *hostName == '\0') {
	tst = ADMCFGHOSTNAMENULL;
    } else if (strlen(hostName) > (MAXHOSTCHARS - 1)) {
	tst = ADMCFGHOSTNAMETOOLONG;
    } else if (hostHandleP == NULL) {
	tst = ADMCFGHOSTHANDLEPNULL;
    } else if (!cfgutil_HostNameGetFull(hostName, fullHostName, &tst2)) {
	tst = tst2;
    }

    /* remote configuration not yet supported; hostName must be local host */

    if (tst == 0) {
	short isLocal;

	if (!cfgutil_HostNameIsLocal(hostName, &isLocal, &tst2)) {
	    tst = tst2;
	} else if (!isLocal) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* allocate a host configuration handle */

    if (tst == 0) {
	char *localHostName;

	if ((cfg_host = (cfg_host_p) malloc(sizeof(cfg_host_t))) == NULL) {
	    tst = ADMNOMEM;
	} else if ((localHostName = (char *)malloc(strlen(fullHostName) + 1))
		   == NULL) {
	    free(cfg_host);
	    tst = ADMNOMEM;
	} else {
	    /* initialize handle */
	    cfg_host->begin_magic = BEGIN_MAGIC;
	    cfg_host->is_valid = 1;
	    cfg_host->hostName = localHostName;
	    cfg_host->is_local = 1;	/* not yet supporting remote config */
	    cfg_host->cellHandle = cellHandle;
	    cfg_host->bosHandle = NULL;
	    cfg_host->end_magic = END_MAGIC;

	    strcpy(localHostName, fullHostName);

	    if (!afsclient_CellNameGet
		(cfg_host->cellHandle, &cfg_host->cellName, &tst2)) {
		tst = tst2;
	    } else if (pthread_mutex_init(&cfg_host->mutex, NULL)) {
		tst = ADMMUTEXINIT;
	    }

	    if (tst != 0) {
		/* cell name lookup or mutex initialization failed */
		free(localHostName);
		free(cfg_host);
	    }
	}
    }

    if (tst == 0) {
	/* success; return host config handle to user */
	*hostHandleP = cfg_host;
    } else {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostClose() -- Release host configuration handle.
 */
int ADMINAPI
cfg_HostClose(void *hostHandle,	/* host config handle */
	      afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* free handle; can assume no other thread using this handle */

    if (tst == 0) {
	/* mark cfg handle invalid in case use after free (bug catcher) */
	cfg_host->is_valid = 0;

	if (cfg_host->bosHandle != NULL) {
	    if (!bos_ServerClose(cfg_host->bosHandle, &tst2)) {
		tst = tst2;
	    }
	}
	free(cfg_host->hostName);
	(void)pthread_mutex_destroy(&cfg_host->mutex);
	free(cfg_host);
    }

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostSetCell() -- Define server cell membership for host.
 *
 *     The cellDbHosts argument is a multistring containing the names of
 *     the existing database servers already configured in the cell; this
 *     multistring list can be obtained via cfg_CellServDbEnumerate().
 *     If configuring the first server in a new cell then the cellDbHosts
 *     list contains only the name of that host.
 *
 *     Note: The names in cellDbHosts MUST exactly match those in the
 *           cell-wide server CellServDB; using cfg_CellServDbEnumerate()
 *           is highly recommended.
 */
int ADMINAPI
cfg_HostSetCell(void *hostHandle,	/* host config handle */
		const char *cellName,	/* cell name */
		const char *cellDbHosts,	/* cell database hosts */
		afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (cellName == NULL || *cellName == '\0') {
	tst = ADMCFGCELLNAMENULL;
    } else if (strlen(cellName) > (MAXCELLCHARS - 1)) {
	tst = ADMCFGCELLNAMETOOLONG;
    } else if (!cfgutil_HostHandleCellNameCompatible(cfg_host, cellName)) {
	tst = ADMCFGCELLNAMECONFLICT;
    } else if (cellDbHosts == NULL || *cellDbHosts == '\0') {
	tst = ADMCFGCELLDBHOSTSNULL;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* define server cell and cell database hosts */

    if (tst == 0) {
	const char *dbHost = cellDbHosts;
	struct afsconf_cell hostCell;
	memset(&hostCell, 0, sizeof(hostCell));

	strcpy(hostCell.name, cellName);
	hostCell.numServers = 0;

	while (*dbHost != '\0' && tst == 0) {
	    /* fill in each database host */
	    size_t dbHostLen = strlen(dbHost);

	    if (dbHostLen > (MAXHOSTCHARS - 1)) {
		tst = ADMCFGHOSTNAMETOOLONG;
	    } else if (hostCell.numServers >= MAXHOSTSPERCELL) {
		tst = ADMCFGCELLDBHOSTCOUNTTOOLARGE;
	    } else {
		strcpy(hostCell.hostName[hostCell.numServers++], dbHost);
		dbHost += dbHostLen + 1;
	    }
	}

	if (tst == 0) {
	    /* create server ThisCell/CellServDB dir if it does not exist */
#ifdef AFS_NT40_ENV
	    (void)mkdir(AFSDIR_USR_DIRPATH);
	    (void)mkdir(AFSDIR_SERVER_AFS_DIRPATH);
	    (void)mkdir(AFSDIR_SERVER_ETC_DIRPATH);
#else
	    (void)mkdir(AFSDIR_USR_DIRPATH, 0755);
	    (void)mkdir(AFSDIR_SERVER_AFS_DIRPATH, 0755);
	    (void)mkdir(AFSDIR_SERVER_ETC_DIRPATH, 0755);
#endif
	    if (afsconf_SetCellInfo
		(NULL, AFSDIR_SERVER_ETC_DIRPATH, &hostCell)) {
		/* failed; most likely cause is bad host name */
		tst = ADMCFGSERVERSETCELLFAILED;
	    }
	}
    }

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostSetAfsPrincipal() -- Put AFS server principal (afs) key in
 *     host's KeyFile; principal is created if it does not exist.
 *
 *     If first server host in cell, passwd must be initial password for
 *     the afs principal; the afs principal is created.
 *
 *     If additional server host, passwd can be specified or NULL; the
 *     afs principal must already exist by definition.  If passwd is NULL
 *     then an attempt is made to fetch the afs key.  If the key fetch fails
 *     because pre 3.5 database servers are in use (which will only return a
 *     key checksum) then the function fails with a return status of
 *     ADMCFGAFSKEYNOTAVAILABLE; in this case the function should be called
 *     again with passwd specified.  If passwd is specified (not NULL) but the
 *     password key fails a checksum comparison with the current afs key
 *     then the function fails with a return status of ADMCFGAFSPASSWDINVALID.
 *
 * ASSUMPTIONS: Client configured and BOS server started; if first host in
 *     cell then Authentication server must be started as well.
 */
int ADMINAPI
cfg_HostSetAfsPrincipal(void *hostHandle,	/* host config handle */
			short isFirst,	/* first server in cell flag */
			const char *passwd,	/* afs initial password */
			afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if ((isFirst && passwd == NULL)
	       || (passwd != NULL && *passwd == '\0')) {
	tst = ADMCFGPASSWDNULL;
    }

    /* put afs key in host's KeyFile */

    if (tst == 0) {
	kas_identity_t afsIdentity;
	kas_encryptionKey_t afsKey;
	int afsKvno = 0;

	strcpy(afsIdentity.principal, "afs");
	afsIdentity.instance[0] = '\0';

	if (isFirst) {
	    /* create afs principal */
	    if (!kas_PrincipalCreate
		(cfg_host->cellHandle, NULL, &afsIdentity, passwd, &tst2)
		&& tst2 != KAEXIST) {
		/* failed to create principal (and not because existed) */
		tst = tst2;
	    }
	}

	if (tst == 0) {
	    /* retrive afs principal information to verify or obtain key */
	    kas_principalEntry_t afsEntry;

	    if (!kas_PrincipalGet
		(cfg_host->cellHandle, NULL, &afsIdentity, &afsEntry,
		 &tst2)) {
		tst = tst2;
	    } else {
		if (passwd != NULL) {
		    /* password given; form key and verify as most recent */
		    kas_encryptionKey_t passwdKey;
		    unsigned int passwdKeyCksum;

		    if (!kas_StringToKey
			(cfg_host->cellName, passwd, &passwdKey, &tst2)
			|| !kas_KeyCheckSum(&passwdKey, &passwdKeyCksum,
					    &tst2)) {
			/* failed to form key or key checksum */
			tst = tst2;

		    } else if (passwdKeyCksum != afsEntry.keyCheckSum) {
			/* passwd string does not generate most recent key;
			 * check if passwd string embeds key directly.
			 */
			if (KasKeyEmbeddedInString(passwd, &passwdKey)) {
			    /* passwd string embeds kas key */
			    if (!kas_KeyCheckSum
				(&passwdKey, &passwdKeyCksum, &tst2)) {
				tst = tst2;
			    } else if (passwdKeyCksum != afsEntry.keyCheckSum) {
				/* passwd string does not embed valid key */
				tst = ADMCFGAFSPASSWDINVALID;
			    }
			} else {
			    /* passwd string does NOT embed key */
			    tst = ADMCFGAFSPASSWDINVALID;
			}
		    }

		    if (tst == 0) {
			/* passwd seems to generate/embed most recent key */
			afsKey = passwdKey;
			afsKvno = afsEntry.keyVersion;
		    }

		} else {
		    /* password NOT given; check if key retrieved since
		     * pre 3.5 database servers only return key checksum
		     */
		    if (KasKeyIsZero(&afsEntry.key)) {
			tst = ADMCFGAFSKEYNOTAVAILABLE;
		    } else {
			afsKey = afsEntry.key;
			afsKvno = afsEntry.keyVersion;
		    }
		}
	    }
	}

	if (tst == 0) {
	    /* add key to host's KeyFile; RPC must be unauthenticated;
	     * bosserver is presumed to be in noauth mode.
	     */
	    void *cellHandle, *bosHandle;

	    if (!afsclient_NullCellOpen(&cellHandle, &tst2)) {
		tst = tst2;
	    } else {
		if (!bos_ServerOpen
		    (cellHandle, cfg_host->hostName, &bosHandle, &tst2)) {
		    tst = tst2;
		} else {
		    if (!bos_KeyCreate(bosHandle, afsKvno, &afsKey, &tst2)
			&& tst2 != BZKEYINUSE) {
			/* failed to add key (and not because existed) */
			tst = tst2;
		    }

		    if (!bos_ServerClose(bosHandle, &tst2)) {
			tst = tst2;
		    }
		}

		if (!afsclient_CellClose(cellHandle, &tst2)) {
		    tst = tst2;
		}
	    }
	}
    }

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostSetAdminPrincipal() -- Put generic administrator principal in
 *     host's UserList; principal is created if it does not exist.
 *
 *     If first server host in cell, passwd and afsUid must be the initial
 *     password and the AFS UID for the admin principal; the admin principal
 *     is created.
 *
 *     If additional server host, passwd and afsUid are ignored; the admin
 *     principal is assumed to exist.
 *
 * ASSUMPTIONS: Client configured and BOS server started; if first host in
 *     cell then Authentication and Protection servers must be started as well.
 */
int ADMINAPI
cfg_HostSetAdminPrincipal(void *hostHandle,	/* host config handle */
			  short isFirst,	/* first server in cell flag */
			  char *admin,		/* admin principal name */
			  const char *passwd,	/* admin initial password */
			  unsigned int afsUid,	/* admin AFS UID */
			  afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (admin == NULL || *admin == '\0') {
	tst = ADMCFGADMINPRINCIPALNULL;
    } else if (strlen(admin) > (KAS_MAX_NAME_LEN - 1)) {
	tst = ADMCFGADMINPRINCIPALTOOLONG;
    } else if (isFirst && (passwd == NULL || *passwd == '\0')) {
	tst = ADMCFGPASSWDNULL;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* put admin in host's UserList */

    if (tst == 0) {
	if (isFirst) {
	    /* first server host in cell; create admin principal */
	    kas_identity_t adminIdentity;
	    int adminUid = afsUid;
	    kas_admin_t adminFlag = KAS_ADMIN;

	    strcpy(adminIdentity.principal, admin);
	    adminIdentity.instance[0] = '\0';

	    if (!kas_PrincipalCreate
		(cfg_host->cellHandle, NULL, &adminIdentity, passwd, &tst2)
		&& tst2 != KAEXIST) {
		/* failed to create principal (and not because existed) */
		tst = tst2;

	    } else
		if (!kas_PrincipalFieldsSet
		    (cfg_host->cellHandle, NULL, &adminIdentity, &adminFlag,
		     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		     &tst2)) {
		/* failed to set admin attributes */
		tst = tst2;

	    } else
		if (!pts_UserCreate
		    (cfg_host->cellHandle, admin, &adminUid, &tst2)
		    && tst2 != PREXIST) {
		/* failed to create user (and not because existed) */
		tst = tst2;

	    } else
		if (!pts_GroupMemberAdd
		    (cfg_host->cellHandle, admin, "system:administrators",
		     &tst2) && tst2 != PRIDEXIST) {
		/* failed to add to group (not because already there) */
		tst = tst2;
	    }
	}

	if (tst == 0) {
	    /* add admin to host's UserList */
	    if (!bos_AdminCreate(cfg_host->bosHandle, admin, &tst2)
		&& tst2 != EEXIST) {
		/* failed to add admin (and not because existed) */
		/* DANGER: platform-specific errno values being returned */
		tst = tst2;
	    }
	}
    }

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostInvalidate() -- Invalidate static server configuration on host.
 *
 *     Server configuration invalidated only if BOS server is not running.
 */
int ADMINAPI
cfg_HostInvalidate(void *hostHandle,	/* host config handle */
		   afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* make sure bosserver is not running on host */

#ifdef AFS_NT40_ENV
    /* Windows - bosserver is controlled via the BOS control service */
    if (tst == 0) {
	DWORD svcState;

	if (!cfgutil_WindowsServiceQuery
	    (AFSREG_SVR_SVC_NAME, &svcState, &tst2)) {
	    tst = tst2;
	} else if (svcState != SERVICE_STOPPED) {
	    tst = ADMCFGBOSSERVERACTIVE;
	}
    }
#else
    if (tst == 0) {
	/* function not yet implemented for Unix */
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */


    /* remove server state files */

    if (tst == 0) {
	int i;
	const char *cfgdir[3];

	cfgdir[0] = AFSDIR_SERVER_ETC_DIRPATH;
	cfgdir[1] = AFSDIR_SERVER_DB_DIRPATH;
	cfgdir[2] = AFSDIR_SERVER_LOCAL_DIRPATH;

	for (i = 0; i < 3 && tst == 0; i++) {
	    if (!cfgutil_CleanDirectory(cfgdir[i], &tst2)) {
		tst = tst2;
	    }
	}
    }

    /* remove all vice partition table entries */

#ifdef AFS_NT40_ENV
    if (tst == 0) {
	struct vpt_iter vpiter;
	struct vptab vpentry;

	/* note: ignore errors except from removal attempts */

	if (!vpt_Start(&vpiter)) {
	    while (!vpt_NextEntry(&vpiter, &vpentry)) {
		if (vpt_RemoveEntry(vpentry.vp_name)) {
		    /* ENOENT implies entry does not exist; consider removed */
		    if (errno != ENOENT) {
			if (errno == EACCES) {
			    tst = ADMNOPRIV;
			} else {
			    tst = ADMCFGVPTABLEWRITEFAILED;
			}
		    }
		}
	    }
	    (void)vpt_Finish(&vpiter);
	}
    }
#else
    /* function not yet implemented for unix */
    if (tst == 0) {
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}



/*
 * cfg_HostPartitionTableEnumerate() -- Enumerate AFS partition table entries.
 *
 *     If the partition table is empty (or does not exist) then *tablePP
 *     is set to NULL and *nEntriesP is set to zero (0).
 *
 *     Partitions in table are not necessarily those being exported; a table
 *     entry may have been added or removed since the fileserver last started.
 */
int ADMINAPI
cfg_HostPartitionTableEnumerate(void *hostHandle,	/* host config handle */
				cfg_partitionEntry_t ** tablePP,	/* table */
				int *nEntriesP,	/* table entry count */
				afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (tablePP == NULL) {
	tst = ADMCFGVPTABLEPNULL;
    } else if (nEntriesP == NULL) {
	tst = ADMCFGVPTABLECOUNTPNULL;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* enumerate the vice partition table */

#ifdef AFS_NT40_ENV
    if (tst == 0) {
	struct vpt_iter vpiter;
	struct vptab vpentry;
	int vpentryCountMax = 0;

	/* count table entries */

	if (vpt_Start(&vpiter)) {
	    /* ENOENT implies table does not exist (which is OK) */
	    if (errno != ENOENT) {
		if (errno == EACCES) {
		    tst = ADMNOPRIV;
		} else {
		    tst = ADMCFGVPTABLEREADFAILED;
		}
	    }
	} else {
	    while (!vpt_NextEntry(&vpiter, &vpentry)) {
		vpentryCountMax++;
	    }
	    if (errno != ENOENT) {
		tst = ADMCFGVPTABLEREADFAILED;
	    }
	    (void)vpt_Finish(&vpiter);
	}

	/* alloc storage for table entries; handle any entry count change */

	if (tst == 0) {
	    if (vpentryCountMax == 0) {
		*nEntriesP = 0;
		*tablePP = NULL;
	    } else {
		/* return a two-part table; first points into second */
		void *metaTablep;
		size_t metaTableSize;

		metaTableSize =
		    vpentryCountMax * (sizeof(cfg_partitionEntry_t) +
				       sizeof(struct vptab));

		if ((metaTablep = (void *)malloc(metaTableSize)) == NULL) {
		    tst = ADMNOMEM;
		} else {
		    int i;
		    cfg_partitionEntry_t *cpePart;
		    struct vptab *vptPart;
		    int vpentryCount = 0;

		    cpePart = (cfg_partitionEntry_t *) metaTablep;
		    vptPart = (struct vptab *)(&cpePart[vpentryCountMax]);

		    for (i = 0; i < vpentryCountMax; i++) {
			cpePart[i].partitionName = vptPart[i].vp_name;
			cpePart[i].deviceName = vptPart[i].vp_dev;
		    }

		    if (vpt_Start(&vpiter)) {
			/* ENOENT implies table does not exist (which is OK) */
			if (errno != ENOENT) {
			    if (errno == EACCES) {
				tst = ADMNOPRIV;
			    } else {
				tst = ADMCFGVPTABLEREADFAILED;
			    }
			}
		    } else {
			for (i = 0; i < vpentryCountMax; i++) {
			    if (vpt_NextEntry(&vpiter, &vptPart[i])) {
				break;
			    }
			}

			if (i < vpentryCountMax && errno != ENOENT) {
			    tst = ADMCFGVPTABLEREADFAILED;
			} else {
			    vpentryCount = i;
			}
			(void)vpt_Finish(&vpiter);
		    }

		    if (tst == 0) {
			*nEntriesP = vpentryCount;

			if (vpentryCount != 0) {
			    *tablePP = (cfg_partitionEntry_t *) metaTablep;
			} else {
			    *tablePP = NULL;
			    free(metaTablep);
			}
		    } else {
				free(metaTablep);
		    }
		}
	    }
	}
    }
#else
    /* function not yet implemented for Unix */
    if (tst == 0) {
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostPartitionTableAddEntry() -- Add or update AFS partition table entry.
 */
int ADMINAPI
cfg_HostPartitionTableAddEntry(void *hostHandle,	/* host config handle */
			       const char *partName,	/* partition name */
			       const char *devName,	/* device name */
			       afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst = 0;
#ifdef AFS_NT40_ENV
    afs_status_t tst2;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
#endif

#ifdef AFS_NT40_ENV
    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (partName == NULL) {
	tst = ADMCFGPARTITIONNAMENULL;
    } else if (!vpt_PartitionNameValid(partName)) {
	tst = ADMCFGPARTITIONNAMEBAD;
    } else if (devName == NULL) {
	tst = ADMCFGDEVICENAMENULL;
    } else if (!vpt_DeviceNameValid(devName)) {
	tst = ADMCFGDEVICENAMEBAD;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* add entry to table */

    if (tst == 0) {
	struct vptab vpentry;

	strcpy(vpentry.vp_name, partName);
	strcpy(vpentry.vp_dev, devName);

	if (vpt_AddEntry(&vpentry)) {
	    if (errno == EACCES) {
		tst = ADMNOPRIV;
	    } else if (errno == EINVAL) {
		/* shouldn't happen since checked partition/dev names */
		tst = ADMCFGVPTABLEENTRYBAD;
	    } else {
		tst = ADMCFGVPTABLEWRITEFAILED;
	    }
	}
    }
#else
    /* function not yet implemented for unix */
    if (tst == 0) {
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostPartitionTableRemoveEntry() -- Remove AFS partition table entry.
 */
int ADMINAPI
cfg_HostPartitionTableRemoveEntry(void *hostHandle,	/* host config handle */
				  const char *partName,	/* partition name */
				  afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst = 0;
#ifdef AFS_NT40_ENV
    afs_status_t tst2;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
#endif

#ifdef AFS_NT40_ENV
    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (partName == NULL) {
	tst = ADMCFGPARTITIONNAMENULL;
    } else if (!vpt_PartitionNameValid(partName)) {
	tst = ADMCFGPARTITIONNAMEBAD;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* remove entry from table */

    if (tst == 0) {
	if (vpt_RemoveEntry(partName)) {
	    /* ENOENT implies entry does not exist; consider to be removed */
	    if (errno != ENOENT) {
		if (errno == EACCES) {
		    tst = ADMNOPRIV;
		} else if (errno == EINVAL) {
		    /* shouldn't happen since checked partition/dev names */
		    tst = ADMCFGPARTITIONNAMEBAD;
		} else {
		    tst = ADMCFGVPTABLEWRITEFAILED;
		}
	    }
	}
    }
#else
    /* function not yet implemented for unix */
    if (tst == 0) {
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_HostPartitionNameValid() -- check partition name syntax.
 */
int ADMINAPI
cfg_HostPartitionNameValid(const char *partName,	/* partition name */
			   short *isValidP,	/* syntax is valid */
			   afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst = 0;

    /* validate parameters */

    if (partName == NULL) {
	tst = ADMCFGPARTITIONNAMENULL;
    } else if (isValidP == NULL) {
	tst = ADMCFGVALIDFLAGPNULL;
    }

    /* check name syntax */

#ifdef AFS_NT40_ENV
    if (tst == 0) {
	*isValidP = vpt_PartitionNameValid(partName);
    }
#else
    /* function not yet implemented for Unix */
    if (tst == 0) {
	tst = ADMCFGNOTSUPPORTED;
    }
#endif

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}



/*
 * cfg_HostDeviceNameValid() -- check device name syntax.
 */
int ADMINAPI
cfg_HostDeviceNameValid(const char *devName,	/* device name */
			short *isValidP,	/* syntax is valid */
			afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst = 0;

    /* validate parameters */

    if (devName == NULL) {
	tst = ADMCFGDEVICENAMENULL;
    } else if (isValidP == NULL) {
	tst = ADMCFGVALIDFLAGPNULL;
    }

    /* check name syntax */

#ifdef AFS_NT40_ENV
    if (tst == 0) {
	*isValidP = vpt_DeviceNameValid(devName);
    }
#else
    /* function not yet implemented for Unix */
    if (tst == 0) {
	tst = ADMCFGNOTSUPPORTED;
    }
#endif

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}



/* ---------------- Exported Utility functions ------------------ */


/*
 * cfg_StringDeallocate() -- Deallocate (multi)string returned by library.
 */
int ADMINAPI
cfg_StringDeallocate(char *stringDataP,	/* (multi)string to deallocate */
		     afs_status_p st)
{				/* completion status */
    free((void *)stringDataP);
    if (st != NULL) {
	*st = 0;
    }
    return 1;
}


/*
 * cfg_PartitionListDeallocate() -- Deallocate partition table enumeration
 *     returned by library.
 */
int ADMINAPI
cfg_PartitionListDeallocate(cfg_partitionEntry_t * partitionListDataP,
			    afs_status_p st)
{
    free((void *)partitionListDataP);
    if (st != NULL) {
	*st = 0;
    }
    return 1;
}




/* ---------------- Local functions ------------------ */


/*
 * KasKeyIsZero() -- determine if kas key is zero
 *
 * RETURN CODES: 1 if zero, 0 otherwise
 */
static int
KasKeyIsZero(kas_encryptionKey_t * kasKey)
{
    char *keyp = (char *)kasKey;
    int i;

    for (i = 0; i < sizeof(*kasKey); i++) {
	if (*keyp++ != 0) {
	    return 0;
	}
    }
    return 1;
}


/*
 * KasKeyEmbeddedInString() -- determine if kas key is embedded in string
 *     and return key if extant.
 *
 * RETURN CODES: 1 if embedded key found, 0 otherwise
 */
static int
KasKeyEmbeddedInString(const char *keyString, kas_encryptionKey_t * kasKey)
{
    char *octalDigits = "01234567";

    /* keyString format is exactly 24 octal digits if embeds kas key */
    if (strlen(keyString) == 24 && strspn(keyString, octalDigits) == 24) {
	/* kas key is embedded in keyString; extract it */
	int i;

	for (i = 0; i < 24; i += 3) {
	    char keyPiece[4];
	    unsigned char keyPieceVal;

	    keyPiece[0] = keyString[i];
	    keyPiece[1] = keyString[i + 1];
	    keyPiece[2] = keyString[i + 2];
	    keyPiece[3] = '\0';

	    keyPieceVal = (unsigned char)strtoul(keyPiece, NULL, 8);

	    *((unsigned char *)kasKey + (i / 3)) = keyPieceVal;
	}
	return 1;
    } else {
	/* key NOT embedded in keyString */
	return 0;
    }
}
