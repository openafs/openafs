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
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <fcntl.h>
#include <sys/utime.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif /* AFS_NT40_ENV */
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <errno.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <sys/stat.h>
#include <des.h>
#include <dirent.h>
#include <stdio.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/ktime.h>
#include <afs/audit.h>
#include <string.h>

#include "bnode.h"
#include "bosint.h"
#include "bosprototypes.h"

extern struct ktime bozo_nextRestartKT, bozo_nextDayKT;

extern struct afsconf_dir *bozo_confdir;
extern int bozo_newKTs;
extern int DoLogging;
#ifdef BOS_RESTRICTED_MODE
extern int bozo_isrestricted;
#endif

afs_int32
SBOZO_GetRestartTime(acall, atype, aktime)
     struct rx_call *acall;
     afs_int32 atype;
     struct bozo_netKTime *aktime;
{
    register afs_int32 code;

    code = 0;			/* assume success */
    switch (atype) {
    case 1:
	memcpy(aktime, &bozo_nextRestartKT, sizeof(struct ktime));
	break;

    case 2:
	memcpy(aktime, &bozo_nextDayKT, sizeof(struct ktime));
	break;

    default:
	code = BZDOM;
	break;
    }

    return code;
}

afs_int32
SBOZO_SetRestartTime(acall, atype, aktime)
     struct rx_call *acall;
     afs_int32 atype;
     struct bozo_netKTime *aktime;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    /* check for proper permissions */
    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing SetRestartTime\n", caller);

    code = 0;			/* assume success */
    switch (atype) {
    case 1:
	memcpy(&bozo_nextRestartKT, aktime, sizeof(struct ktime));
	break;

    case 2:
	memcpy(&bozo_nextDayKT, aktime, sizeof(struct ktime));
	break;

    default:
	code = BZDOM;
	break;
    }

    if (code == 0) {
	/* try to update the bozo init file */
	code = WriteBozoFile(0);
	bozo_newKTs = 1;
    }

  fail:
    osi_auditU(acall, BOS_SetRestartEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_Exec(acall, acmd)
     struct rx_call *acall;
     char *acmd;
{

    char caller[MAXKTCNAMELEN];
    int code;

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
#ifdef BOS_RESTRICTED_MODE
    if (bozo_isrestricted) {
	code = BZACCESS;
	goto fail;
    }
#endif
    if (DoLogging)
	bozo_Log("%s is executing the shell command '%s'\n", caller, acmd);

    /* should copy output to acall, but don't yet cause its hard */
    /*  hard... NOT!  Nnow _at least_ return the exit status */
    code = system(acmd);
    osi_auditU(acall, BOS_ExecEvent, code, AUD_STR, acmd, AUD_END);

  fail:
    return code;
}

afs_int32
SBOZO_GetDates(acall, aname, atime, abakTime, aoldTime)
     struct rx_call *acall;
     char *aname;
     afs_int32 *atime, *abakTime, *aoldTime;
{
    struct stat tstat;
    char *strp;
    char tbuffer[AFSDIR_PATH_MAX];

    *atime = *abakTime = *aoldTime = 0;

    /* construct local path from canonical (wire-format) path */
    if (ConstructLocalBinPath(aname, &strp)) {
	return 0;
    }
    strcpy(tbuffer, strp);
    free(strp);

    strp = tbuffer + strlen(tbuffer);

    if (!stat(tbuffer, &tstat)) {
	*atime = tstat.st_mtime;
    }

    strcpy(strp, ".BAK");
    if (!stat(tbuffer, &tstat)) {
	*abakTime = tstat.st_mtime;
    }

    strcpy(strp, ".OLD");
    if (!stat(tbuffer, &tstat)) {
	*aoldTime = tstat.st_mtime;
    }
    return 0;
}

afs_int32
SBOZO_UnInstall(acall, aname)
     struct rx_call *acall;
     register char *aname;
{
    char *filepath;
    char fpOld[AFSDIR_PATH_MAX], fpBak[AFSDIR_PATH_MAX];
    afs_int32 code;
    char caller[MAXKTCNAMELEN];
    struct stat tstat;

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	osi_auditU(acall, BOS_UnInstallEvent, code, AUD_STR, aname, AUD_END);
	return code;
    }
#ifdef BOS_RESTRICTED_MODE
    if (bozo_isrestricted) {
	code = BZACCESS;
	osi_auditU(acall, BOS_UnInstallEvent, code, AUD_STR, aname, AUD_END);
	return code;
    }
#endif

    /* construct local path from canonical (wire-format) path */
    if (ConstructLocalBinPath(aname, &filepath)) {
	return BZNOENT;
    }

    if (DoLogging)
	bozo_Log("%s is executing UnInstall '%s'\n", caller, filepath);

    strcpy(fpBak, filepath);
    strcat(fpBak, ".BAK");
    strcpy(fpOld, filepath);
    strcat(fpOld, ".OLD");

    code = renamefile(fpBak, filepath);
    if (code) {
	/* can't find .BAK, try .OLD */
	code = renamefile(fpOld, filepath);
	if (code && errno == ENOENT)	/* If doesn't exist don't fail */
	    code = 0;
    } else {
	/* now rename .OLD to .BAK */
	if (stat(fpOld, &tstat) == 0)
	    code = renamefile(fpOld, fpBak);
    }
    if (code)
	code = errno;

    osi_auditU(acall, BOS_UnInstallEvent, code, AUD_STR, filepath, AUD_END);
    free(filepath);

    return code;
}

#define	BOZO_OLDTIME	    (7*24*3600)	/* 7 days old */
static void
SaveOldFiles(char *aname)
{
    register afs_int32 code;
    char bbuffer[AFSDIR_PATH_MAX], obuffer[AFSDIR_PATH_MAX];
    struct stat tstat;
    register afs_int32 now;
    afs_int32 oldTime, bakTime;

    strcpy(bbuffer, aname);
    strcat(bbuffer, ".BAK");
    strcpy(obuffer, aname);
    strcat(obuffer, ".OLD");
    now = FT_ApproxTime();

    code = stat(aname, &tstat);
    if (code < 0)
	return;			/* can't stat file */

    code = stat(obuffer, &tstat);	/* discover old file's time */
    if (code)
	oldTime = 0;
    else
	oldTime = tstat.st_mtime;

    code = stat(bbuffer, &tstat);	/* discover back file's time */
    if (code)
	bakTime = 0;
    else
	bakTime = tstat.st_mtime;

    if (bakTime && (oldTime == 0 || bakTime < now - BOZO_OLDTIME)) {
	/* no .OLD file, or .BAK is at least a week old */
	code = renamefile(bbuffer, obuffer);
    }

    /* finally rename to .BAK extension */
    renamefile(aname, bbuffer);
}

afs_int32
SBOZO_Install(acall, aname, asize, mode, amtime)
     struct rx_call *acall;
     char *aname;
     afs_int32 asize;
     afs_int32 amtime;
     afs_int32 mode;
{
    afs_int32 code;
    int fd;
    afs_int32 len;
    afs_int32 total;
#ifdef AFS_NT40_ENV
    struct _utimbuf utbuf;
#else
    struct timeval tvb[2];
#endif
    char filepath[AFSDIR_PATH_MAX], tbuffer[AFSDIR_PATH_MAX], *fpp;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller))
	return BZACCESS;
#ifdef BOS_RESTRICTED_MODE
    if (bozo_isrestricted)
	return BZACCESS;
#endif

    /* construct local path from canonical (wire-format) path */
    if (ConstructLocalBinPath(aname, &fpp)) {
	return BZNOENT;
    }
    strcpy(filepath, fpp);
    free(fpp);

    if (DoLogging)
	bozo_Log("%s is executing Install '%s'\n", caller, filepath);

    /* open file */
    fpp = filepath + strlen(filepath);
    strcpy(fpp, ".NEW");	/* append ".NEW" to end of filepath */
    fd = open(filepath, O_CREAT | O_RDWR | O_TRUNC, 0777);
    if (fd < 0)
	return errno;
    total = 0;
    while (1) {
	len = rx_Read(acall, tbuffer, sizeof(tbuffer));
	if (len < 0) {
	    close(fd);
	    unlink(filepath);
	    return 102;
	}
	if (len == 0)
	    break;		/* no more input */
	code = write(fd, tbuffer, len);
	if (code != len) {
	    close(fd);
	    unlink(filepath);
	    return 100;
	}
	total += len;		/* track total written for safety check at end */
    }
    close(fd);
    if (asize != total) {
	unlink(filepath);
	return 101;		/* wrong size */
    }

    /* save old files */
    *fpp = '\0';		/* remove ".NEW" from end of filepath */
    SaveOldFiles(filepath);	/* don't care if it works, still install */

    /* all done, rename to final name */
    strcpy(tbuffer, filepath);
    strcat(tbuffer, ".NEW");
    code = (renamefile(tbuffer, filepath) ? errno : 0);

    /* label file with same time for our sanity */
#ifdef AFS_NT40_ENV
    utbuf.actime = utbuf.modtime = amtime;
    _utime(filepath, &utbuf);
#else
    tvb[0].tv_sec = tvb[1].tv_sec = amtime;
    tvb[0].tv_usec = tvb[1].tv_usec = 0;
    utimes(filepath, tvb);
#endif /* AFS_NT40_ENV */

    if (mode)
	chmod(filepath, mode);

    if (code < 0) {
	osi_auditU(acall, BOS_InstallEvent, code, AUD_STR, filepath, AUD_END);
	return errno;
    } else
	return 0;
}

afs_int32
SBOZO_SetCellName(acall, aname)
     struct rx_call *acall;
     char *aname;
{
    struct afsconf_cell tcell;
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];
    char clones[MAXHOSTSPERCELL];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing SetCellName '%s'\n", caller, aname);

    code =
	afsconf_GetExtendedCellInfo(bozo_confdir, NULL, NULL, &tcell,
				    &clones);
    if (code)
	goto fail;

    /* Check that tcell has enough space for the new cellname. */
    if (strlen(aname) > sizeof tcell.name - 1) {
	bozo_Log
	    ("ERROR: SetCellName: cell name '%s' exceeds %ld bytes (cell name not changed)\n",
	     aname, (long)(sizeof tcell.name - 1));
	code = BZDOM;
	goto fail;
    }

    strcpy(tcell.name, aname);
    code =
	afsconf_SetExtendedCellInfo(bozo_confdir, AFSDIR_SERVER_ETC_DIRPATH,
				    &tcell, &clones);

  fail:
    osi_auditU(acall, BOS_SetCellEvent, code, AUD_STR, aname, AUD_END);
    return code;
}

afs_int32
SBOZO_GetCellName(acall, aname)
     struct rx_call *acall;
     char **aname;
{
    register afs_int32 code;
    char tname[MAXCELLCHARS];

    code = afsconf_GetLocalCell(bozo_confdir, tname, sizeof(tname));
    if (code) {
	/* must set output parameters even if aborting */
	*aname = (char *)malloc(1);
	**aname = 0;
    } else {
	*aname = (char *)malloc(strlen(tname) + 1);
	strcpy(*aname, tname);
    }

    return code;
}

afs_int32
SBOZO_GetCellHost(acall, awhich, aname)
     struct rx_call *acall;
     afs_uint32 awhich;
     char **aname;
{
    register afs_int32 code;
    struct afsconf_cell tcell;
    register char *tp;
    char clones[MAXHOSTSPERCELL];

    code =
	afsconf_GetExtendedCellInfo(bozo_confdir, NULL, NULL, &tcell,
				    &clones);
    if (code)
	goto fail;

    if (awhich >= tcell.numServers) {
	code = BZDOM;
	goto fail;
    }

    tp = tcell.hostName[awhich];
    *aname = (char *)malloc(strlen(tp) + 3);
    if (clones[awhich]) {
	strcpy(*aname, "[");
	strcat(*aname, tp);
	strcat(*aname, "]");
    } else
	strcpy(*aname, tp);
    goto done;

  fail:
    *aname = (char *)malloc(1);	/* return fake string */
    **aname = 0;

  done:
    return code;
}

afs_int32
SBOZO_DeleteCellHost(acall, aname)
     struct rx_call *acall;
     char *aname;
{
    register afs_int32 code;
    struct afsconf_cell tcell;
    afs_int32 which;
    register int i;
    char caller[MAXKTCNAMELEN];
    char clones[MAXHOSTSPERCELL];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing DeleteCellHost '%s'\n", caller, aname);

    code =
	afsconf_GetExtendedCellInfo(bozo_confdir, NULL, NULL, &tcell,
				    &clones);
    if (code)
	goto fail;

    which = -1;
    for (i = 0; i < tcell.numServers; i++) {
	if (strcmp(tcell.hostName[i], aname) == 0) {
	    which = i;
	    break;
	}
    }

    if (which < 0) {
	code = BZNOENT;
	goto fail;
    }

    memset(&tcell.hostAddr[which], 0, sizeof(struct sockaddr_in));
    memset(tcell.hostName[which], 0, MAXHOSTCHARS);
    code =
	afsconf_SetExtendedCellInfo(bozo_confdir, AFSDIR_SERVER_ETC_DIRPATH,
				    &tcell, &clones);

  fail:
    osi_auditU(acall, BOS_DeleteHostEvent, code, AUD_STR, aname, AUD_END);
    return code;
}

afs_int32
SBOZO_AddCellHost(acall, aname)
     struct rx_call *acall;
     char *aname;
{
    register afs_int32 code;
    struct afsconf_cell tcell;
    afs_int32 which;
    register int i;
    char caller[MAXKTCNAMELEN];
    char clones[MAXHOSTSPERCELL];
    char *n;
    char isClone = 0;

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing AddCellHost '%s'\n", caller, aname);

    code =
	afsconf_GetExtendedCellInfo(bozo_confdir, NULL, NULL, &tcell,
				    &clones);
    if (code)
	goto fail;

    n = aname;
    if (*n == '[') {
	*(n + strlen(n) - 1) = 0;
	++n;
	isClone = 1;
    }

    which = -1;
    for (i = 0; i < tcell.numServers; i++) {
	if (strcmp(tcell.hostName[i], n) == 0) {
	    which = i;
	    break;
	}
    }
    if (which < 0) {
	which = tcell.numServers;
	tcell.numServers++;

	/*
	 * Check that tcell has enough space for an additional host.
	 *
	 * We assume that tcell.hostAddr[] and tcell.hostName[] have the
	 * same number of entries.
	 */
	if (tcell.numServers >
	    sizeof tcell.hostAddr / sizeof tcell.hostAddr[0]) {
	    bozo_Log
		("ERROR: AddCellHost: attempt to add more than %ld database servers (database server '%s' not added)\n",
		 (long)(sizeof tcell.hostAddr / sizeof tcell.hostAddr[0]),
		 aname);
	    code = BZDOM;
	    goto fail;
	}

	/* Check that tcell has enough space for the new hostname. */
	if (strlen(aname) > sizeof tcell.hostName[0] - 1) {
	    bozo_Log
		("ERROR: AddCellHost: host name '%s' exceeds %ld bytes (not added)\n",
		 aname, (long)(sizeof tcell.hostName[0] - 1));
	    code = BZDOM;
	    goto fail;
	}
    }

    memset(&tcell.hostAddr[which], 0, sizeof(struct sockaddr_in));
    strcpy(tcell.hostName[which], n);
    clones[which] = isClone;
    code =
	afsconf_SetExtendedCellInfo(bozo_confdir, AFSDIR_SERVER_ETC_DIRPATH,
				    &tcell, &clones);

  fail:
    osi_auditU(acall, BOS_AddHostEvent, code, AUD_STR, aname, AUD_END);
    return code;
}

afs_int32
SBOZO_ListKeys(acall, an, akvno, akey, akeyinfo)
     struct rx_call *acall;
     afs_int32 an;
     afs_int32 *akvno;
     struct bozo_keyInfo *akeyinfo;
     struct bozo_key *akey;
{
    struct afsconf_keys tkeys;
    register afs_int32 code;
    struct stat tstat;
    int noauth;
    char caller[MAXKTCNAMELEN];
    rxkad_level enc_level = rxkad_clear;

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing ListKeys\n", caller);

    code = afsconf_GetKeys(bozo_confdir, &tkeys);
    if (code)
	goto fail;

    if (tkeys.nkeys <= an) {
	code = BZDOM;
	goto fail;
    }
    *akvno = tkeys.key[an].kvno;
    memset(akeyinfo, 0, sizeof(struct bozo_keyInfo));

    noauth = afsconf_GetNoAuthFlag(bozo_confdir);
    rxkad_GetServerInfo(acall->conn, &enc_level, 0, 0, 0, 0, 0);
    /* 
     * only return actual keys in noauth or if this is an encrypted connection
     */

    if ((noauth) || (enc_level == rxkad_crypt)) {
	memcpy(akey, tkeys.key[an].key, 8);
    } else
	memset(akey, 0, 8);

    code = stat(AFSDIR_SERVER_KEY_FILEPATH, &tstat);
    if (code == 0) {
	akeyinfo->mod_sec = tstat.st_mtime;
    }
    ka_KeyCheckSum(tkeys.key[an].key, &akeyinfo->keyCheckSum);
    /* only errors is bad key parity */

  fail:
    if (noauth)
	osi_auditU(acall, BOS_UnAuthListKeysEvent, code, AUD_END);
    osi_auditU(acall, BOS_ListKeysEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_AddKey(acall, an, akey)
     struct rx_call *acall;
     afs_int32 an;
     struct bozo_key *akey;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];
    rxkad_level enc_level = rxkad_clear;
    int noauth;

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    noauth = afsconf_GetNoAuthFlag(bozo_confdir);
    rxkad_GetServerInfo(acall->conn, &enc_level, 0, 0, 0, 0, 0);
    if ((!noauth) && (enc_level != rxkad_crypt)) {
	code = BZENCREQ;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing AddKey\n", caller);

    code = afsconf_AddKey(bozo_confdir, an, akey, 0);
    if (code == AFSCONF_KEYINUSE)
	code = BZKEYINUSE;	/* Unique code for afs rpc calls */
  fail:
    osi_auditU(acall, BOS_AddKeyEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_SetNoAuthFlag(acall, aflag)
     register struct rx_call *acall;
     afs_int32 aflag;
{
    register afs_int32 code = 0;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing Set No Authentication\n", caller);

    afsconf_SetNoAuthFlag(bozo_confdir, aflag);

  fail:
    osi_auditU(acall, BOS_SetNoAuthEvent, code, AUD_LONG, aflag, AUD_END);
    return code;
}

afs_int32
SBOZO_DeleteKey(acall, an)
     struct rx_call *acall;
     afs_int32 an;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing DeleteKey\n", caller);

    code = afsconf_DeleteKey(bozo_confdir, an);

  fail:
    osi_auditU(acall, BOS_DeleteKeyEvent, code, AUD_END);
    return code;
}


afs_int32
SBOZO_ListSUsers(acall, an, aname)
     struct rx_call *acall;
     afs_int32 an;
     register char **aname;
{
    register afs_int32 code;
    register char *tp;

    tp = *aname = (char *)malloc(256);
    *tp = 0;			/* in case getnthuser doesn't null-terminate the string */
    code = afsconf_GetNthUser(bozo_confdir, an, tp, 256);

  /* fail: */
    osi_auditU(acall, BOS_ListSUserEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_AddSUser(acall, aname)
     struct rx_call *acall;
     char *aname;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing Add SuperUser '%s'\n", caller, aname);

    code = afsconf_AddUser(bozo_confdir, aname);

  fail:
    osi_auditU(acall, BOS_AddSUserEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_DeleteSUser(acall, aname)
     struct rx_call *acall;
     char *aname;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }

    if (DoLogging)
	bozo_Log("%s is executing Delete SuperUser '%s'\n", caller, aname);

    code = afsconf_DeleteUser(bozo_confdir, aname);

  fail:
    osi_auditU(acall, BOS_DeleteSUserEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_CreateBnode(acall, atype, ainstance, ap1, ap2, ap3, ap4, ap5, notifier)
     struct rx_call *acall;
     char *atype;
     char *ainstance;
     char *ap1, *ap2, *ap3, *ap4, *ap5;
     char *notifier;
{
    struct bnode *tb;
    afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
#ifdef BOS_RESTRICTED_MODE
    if (bozo_isrestricted) {
	if (strcmp(atype, "cron") || strcmp(ainstance, "salvage-tmp")
	    || strcmp(ap2, "now")
	    || strncmp(ap1, AFSDIR_CANONICAL_SERVER_SALVAGER_FILEPATH,
		       strlen(AFSDIR_CANONICAL_SERVER_SALVAGER_FILEPATH))) {
	    code = BZACCESS;
	    goto fail;
	}
    }
#endif

    code =
	bnode_Create(atype, ainstance, &tb, ap1, ap2, ap3, ap4, ap5, notifier,
		     BSTAT_NORMAL, 1);
    if (!code)
	bnode_SetStat(tb, BSTAT_NORMAL);

  fail:
    osi_auditU(acall, BOS_CreateBnodeEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_WaitAll(acall)
     register struct rx_call *acall;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }

    if (DoLogging)
	bozo_Log("%s is executing Wait for All\n", caller);

    code = bnode_WaitAll();

  fail:
    osi_auditU(acall, BOS_WaitAllEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_DeleteBnode(acall, ainstance)
     struct rx_call *acall;
     char *ainstance;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
#ifdef BOS_RESTRICTED_MODE
    if (bozo_isrestricted) {
	code = BZACCESS;
	goto fail;
    }
#endif
    if (DoLogging)
	bozo_Log("%s is executing DeleteBnode '%s'\n", caller, ainstance);

    code = bnode_DeleteName(ainstance);

  fail:
    osi_auditU(acall, BOS_DeleteBnodeEvent, code, AUD_STR, ainstance,
	       AUD_END);
    return code;
}

static
swproc(abnode, arock)
     register struct bnode *abnode;
     char *arock;
{
    if (abnode->goal == BSTAT_NORMAL)
	return 0;		/* this one's not shutting down */
    /* otherwise, we are shutting down */
    bnode_Hold(abnode);
    bnode_WaitStatus(abnode, BSTAT_SHUTDOWN);
    bnode_Release(abnode);
    return 0;			/* don't stop apply function early, no matter what */
}

static
stproc(abnode, arock)
     struct bnode *abnode;
     char *arock;
{
    if (abnode->fileGoal == BSTAT_SHUTDOWN)
	return 0;		/* don't do these guys */

    bnode_Hold(abnode);
    bnode_SetStat(abnode, BSTAT_NORMAL);
    bnode_Release(abnode);
    return 0;
}

static
sdproc(abnode, arock)
     struct bnode *abnode;
     char *arock;
{
    bnode_Hold(abnode);
    bnode_SetStat(abnode, BSTAT_SHUTDOWN);
    bnode_Release(abnode);
    return 0;
}

/* shutdown and leave down */
afs_int32
SBOZO_ShutdownAll(acall)
     struct rx_call *acall;
{
    /* iterate over all bnodes, setting the status to temporarily disabled */
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    /* check for authorization */
    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing ShutdownAll\n", caller);

    code = bnode_ApplyInstance(sdproc, NULL);

  fail:
    osi_auditU(acall, BOS_ShutdownAllEvent, code, AUD_END);
    return code;
}

/* shutdown and restart */
afs_int32
SBOZO_RestartAll(acall)
     struct rx_call *acall;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing RestartAll\n", caller);

    /* start shutdown of all processes */
    code = bnode_ApplyInstance(sdproc, NULL);
    if (code)
	goto fail;

    /* wait for all done */
    code = bnode_ApplyInstance(swproc, NULL);
    if (code)
	goto fail;

    /* start them up again */
    code = bnode_ApplyInstance(stproc, NULL);

  fail:
    osi_auditU(acall, BOS_RestartAllEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_ReBozo(acall)
     register struct rx_call *acall;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    /* acall is null if called internally to restart bosserver */
    if (acall && !afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing ReBozo\n", caller);

    /* start shutdown of all processes */
    code = bnode_ApplyInstance(sdproc, NULL);
    if (code)
	goto fail;

    /* wait for all done */
    code = bnode_ApplyInstance(swproc, NULL);
    if (code)
	goto fail;

    if (acall)
	osi_auditU(acall, BOS_RebozoEvent, code, AUD_END);
    else
	osi_audit(BOS_RebozoIntEvent, code, AUD_END);

    if (acall)
	rx_EndCall(acall, 0);	/* try to get it done */
    rx_Finalize();
    bozo_ReBozo();		/* this reexecs us, and doesn't return, of course */

  fail:
    /* Differentiate between external and internal ReBozo; prevents AFS_Aud_NoCall event */
    if (acall)
	osi_auditU(acall, BOS_RebozoEvent, code, AUD_END);
    else
	osi_audit(BOS_RebozoIntEvent, code, AUD_END);
    return code;		/* should only get here in unusual circumstances */
}

/* make sure all are running */
afs_int32
SBOZO_StartupAll(acall)
     struct rx_call *acall;
{
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing StartupAll\n", caller);
    code = bnode_ApplyInstance(stproc, NULL);

  fail:
    osi_auditU(acall, BOS_StartupAllEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_Restart(acall, ainstance)
     struct rx_call *acall;
     register char *ainstance;
{
    register struct bnode *tb;
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing Restart '%s'\n", caller, ainstance);

    tb = bnode_FindInstance(ainstance);
    if (!tb) {
	code = BZNOENT;
	goto fail;
    }

    /* setup return code */
    code = 0;

    bnode_Hold(tb);
    bnode_SetStat(tb, BSTAT_SHUTDOWN);
    code = bnode_WaitStatus(tb, BSTAT_SHUTDOWN);	/* this can fail */
    bnode_SetStat(tb, BSTAT_NORMAL);
    bnode_Release(tb);

  fail:
    osi_auditU(acall, BOS_RestartEvent, code, AUD_STR, ainstance, AUD_END);
    return code;
}

/* set temp status */
afs_int32
SBOZO_SetTStatus(acall, ainstance, astatus)
     struct rx_call *acall;
     char *ainstance;
     afs_int32 astatus;
{
    register struct bnode *tb;
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing SetTempStatus '%s'\n", caller, ainstance);

    tb = bnode_FindInstance(ainstance);
    if (!tb) {
	code = BZNOENT;
	goto fail;
    }
    bnode_Hold(tb);
    code = bnode_SetStat(tb, astatus);
    bnode_Release(tb);

  fail:
    osi_auditU(acall, BOS_SetTempStatusEvent, code, AUD_STR, ainstance,
	       AUD_END);
    return code;
}

afs_int32
SBOZO_SetStatus(acall, ainstance, astatus)
     struct rx_call *acall;
     char *ainstance;
     afs_int32 astatus;
{
    register struct bnode *tb;
    register afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
    if (DoLogging)
	bozo_Log("%s is executing SetStatus '%s' (status = %d)\n", caller,
		 ainstance, astatus);

    tb = bnode_FindInstance(ainstance);
    if (!tb) {
	code = BZNOENT;
	goto fail;
    }
    bnode_Hold(tb);
    bnode_SetFileGoal(tb, astatus);
    code = bnode_SetStat(tb, astatus);
    bnode_Release(tb);

  fail:
    osi_auditU(acall, BOS_SetStatusEvent, code, AUD_STR, ainstance, AUD_END);
    return code;
}

afs_int32
SBOZO_GetStatus(acall, ainstance, astat, astatDescr)
     struct rx_call *acall;
     char *ainstance;
     afs_int32 *astat;
     char **astatDescr;
{
    register struct bnode *tb;
    register afs_int32 code;

    tb = bnode_FindInstance(ainstance);
    if (!tb) {
	code = BZNOENT;
	goto fail;
    }

    bnode_Hold(tb);
    code = bnode_GetStat(tb, astat);
    if (code) {
	bnode_Release(tb);
	goto fail;
    }

    *astatDescr = (char *)malloc(BOZO_BSSIZE);
    code = bnode_GetString(tb, *astatDescr, BOZO_BSSIZE);
    bnode_Release(tb);
    if (code)
	(*astatDescr)[0] = 0;	/* null string means no further info */
    return 0;

  fail:
    *astatDescr = (char *)malloc(1);
    **astatDescr = 0;
    return code;
}

struct eidata {
    char *iname;
    int counter;
};

static
eifunc(abnode, arock)
     struct bnode *abnode;
     struct eidata *arock;
{
    if (arock->counter-- == 0) {
	/* done */
	strcpy(arock->iname, abnode->name);
	return 1;
    } else {
	/* not there yet */
	return 0;
    }
}

static
ZapFile(adir, aname)
     register char *adir;
     register char *aname;
{
    char tbuffer[256];
    strcpy(tbuffer, adir);
    strcat(tbuffer, "/");
    strcat(tbuffer, aname);
    return unlink(tbuffer);
}

afs_int32
SBOZO_Prune(acall, aflags)
     struct rx_call *acall;
     afs_int32 aflags;
{
    register afs_int32 code;
    DIR *dirp;
    register struct dirent *tde;
    register int i;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
#ifdef BOS_RESTRICTED_MODE
    if (bozo_isrestricted) {
	code = BZACCESS;
	goto fail;
    }
#endif
    if (DoLogging)
	bozo_Log("%s is executing Prune (flags=%d)\n", caller, aflags);

    /* first scan AFS binary directory */
    dirp = opendir(AFSDIR_SERVER_BIN_DIRPATH);
    if (dirp) {
	for (tde = readdir(dirp); tde; tde = readdir(dirp)) {
	    i = strlen(tde->d_name);
	    if (aflags & BOZO_PRUNEOLD) {
		if (i >= 4 && strncmp(tde->d_name + i - 4, ".OLD", 4) == 0)
		    ZapFile(AFSDIR_SERVER_BIN_DIRPATH, tde->d_name);
	    }
	    if (aflags & BOZO_PRUNEBAK) {
		if (i >= 4 && strncmp(tde->d_name + i - 4, ".BAK", 4) == 0)
		    ZapFile(AFSDIR_SERVER_BIN_DIRPATH, tde->d_name);
	    }
	}
	closedir(dirp);
    }

    /* then scan AFS log directory */
    dirp = opendir(AFSDIR_SERVER_LOGS_DIRPATH);
    if (dirp) {
	for (tde = readdir(dirp); tde; tde = readdir(dirp)) {
	    if (aflags & BOZO_PRUNECORE) {
		if (strncmp(tde->d_name, AFSDIR_CORE_FILE, 4) == 0)
		    ZapFile(AFSDIR_SERVER_LOGS_DIRPATH, tde->d_name);
	    }
	}
	closedir(dirp);
    }
    code = 0;

  fail:
    osi_auditU(acall, BOS_PruneLogs, code, AUD_END);
    return code;
}

afs_int32
SBOZO_EnumerateInstance(acall, anum, ainstance)
     struct rx_call *acall;
     afs_int32 anum;
     char **ainstance;
{
    struct eidata tdata;

    *ainstance = (char *)malloc(BOZO_BSSIZE);
    **ainstance = 0;
    tdata.counter = anum;
    tdata.iname = *ainstance;
    bnode_ApplyInstance(eifunc, &tdata);
    if (tdata.counter >= 0)
	return BZDOM;		/* anum > # of actual instances */
    else
	return 0;
}

struct bozo_bosEntryStats bozo_bosEntryStats[] = {
    {NULL, 1, 1, 0755, 02},	/* AFSDIR_SERVER_AFS_DIRPATH    */
    {NULL, 1, 1, 0755, 02},	/* AFSDIR_SERVER_ETC_DIRPATH    */
    {NULL, 1, 1, 0755, 02},	/* AFSDIR_SERVER_BIN_DIRPATH    */
    {NULL, 1, 1, 0755, 02},	/* AFSDIR_SERVER_LOGS_DIRPATH   */
    {NULL, 1, 0, 0700, 07},	/* AFSDIR_SERVER_BACKUP_DIRPATH */
    {NULL, 1, 1, 0700, 07},	/* AFSDIR_SERVER_DB_DIRPATH     */
    {NULL, 1, 1, 0700, 07},	/* AFSDIR_SERVER_LOCAL_DIRPATH  */
    {NULL, 0, 1, 0600, 07},	/* AFSDIR_SERVER_KEY_FILEPATH   */
    {NULL, 0, 1, 0600, 03}
};				/* AFSDIR_SERVER_ULIST_FILEPATH */
int bozo_nbosEntryStats =
    sizeof(bozo_bosEntryStats) / sizeof(bozo_bosEntryStats[0]);

/* This function performs initialization of the bozo_bosEntrystats[]
 * array. This array contains the list of dirs that the bosserver 
 * is interested in along with their recommended permissions
 * NOTE: This initialization is a bit ugly. This was caused because
 * the path names require procedural as opposed to static initialization.
 * The other fields in the struct are however, statically initialized.
 */
int
initBosEntryStats()
{
    bozo_bosEntryStats[0].path = AFSDIR_SERVER_AFS_DIRPATH;
    bozo_bosEntryStats[1].path = AFSDIR_SERVER_ETC_DIRPATH;
    bozo_bosEntryStats[2].path = AFSDIR_SERVER_BIN_DIRPATH;
    bozo_bosEntryStats[3].path = AFSDIR_SERVER_LOGS_DIRPATH;
    bozo_bosEntryStats[4].path = AFSDIR_SERVER_BACKUP_DIRPATH;
    bozo_bosEntryStats[5].path = AFSDIR_SERVER_DB_DIRPATH;
    bozo_bosEntryStats[6].path = AFSDIR_SERVER_LOCAL_DIRPATH;
    bozo_bosEntryStats[7].path = AFSDIR_SERVER_KEY_FILEPATH;
    bozo_bosEntryStats[8].path = AFSDIR_SERVER_ULIST_FILEPATH;

    return 0;
}

/* StatEachEntry - If it's not there, it is okay.  If anything else goes wrong
 * complain.  Otherwise check permissions: shouldn't allow write or (usually)
 * read. */

static int
StatEachEntry(stats)
     IN struct bozo_bosEntryStats *stats;
{
    struct stat info;
    if (stat(stats->path, &info)) {
	if (errno == ENOENT)
	    return 1;		/* no such entry: just ignore it */
	return 0;		/* something else went wrong */
    } else {
	int rights;
	if (((info.st_mode & S_IFDIR) != 0) != stats->dir)
	    return 0;		/* not expected type */
	if (stats->rootOwner && (info.st_uid != 0))
	    return 0;		/* not owned by root */
	rights = (info.st_mode & 0000777);
	if ((rights & stats->reqPerm) != stats->reqPerm)
	    return 0;		/* required permissions not present */
	if ((rights & stats->proPerm) != 0)
	    return 0;		/* prohibited permissions present */
    }
    return 1;
}

/* DirAccessOK - checks the mode bits on the AFS dir and decendents and
 * returns 0 if some are not OK and 1 otherwise.  For efficiency, it doesn't do
 * this check more often than every 5 seconds. */

int
DirAccessOK()
{
#ifdef AFS_NT40_ENV
    /* underlying filesystem may not support directory protection */
    return 1;
#else
    static afs_uint32 lastTime = 0;
    afs_uint32 now = FT_ApproxTime();
    static int lastResult = -1;
    int result;
    int i;

    if ((now - lastTime) < 5)
	return lastResult;
    lastTime = now;

    result = 1;
    for (i = 0; i < bozo_nbosEntryStats; i++) {
	struct bozo_bosEntryStats *e = &bozo_bosEntryStats[i];
	if (!StatEachEntry(e)) {
	    bozo_Log("unhappy with %s which is a %s that should "
		     "have at least rights %o, at most rights %o %s\n",
		     e->path, e->dir ? "dir" : "file", e->reqPerm, 
		     (~e->proPerm & 0777), 
		     e->rootOwner ? ", owned by root" : "");
	    result = 0;
	    break;
	}
    }

    if (result != lastResult) {	/* log changes */
	bozo_Log("Server directory access is %sokay\n",
		 (result ? "" : "not "));
    }
    lastResult = result;
    return lastResult;
#endif /* AFS_NT40_ENV */
}

int
GetRequiredDirPerm(path)
     IN char *path;
{
    int i;
    for (i = 0; i < bozo_nbosEntryStats; i++)
	if (strcmp(path, bozo_bosEntryStats[i].path) == 0)
	    return bozo_bosEntryStats[i].reqPerm;
    return -1;
}

afs_int32
SBOZO_GetInstanceInfo(acall, ainstance, atype, astatus)
     IN struct rx_call *acall;
     IN char *ainstance;
     OUT char **atype;
     OUT struct bozo_status *astatus;
{
    register struct bnode *tb;

    tb = bnode_FindInstance(ainstance);
    *atype = (char *)malloc(BOZO_BSSIZE);
    **atype = 0;
    if (!tb)
	return BZNOENT;
    if (tb->type)
	strcpy(*atype, tb->type->name);
    else
	(*atype)[0] = 0;	/* null string */
    memset(astatus, 0, sizeof(struct bozo_status));	/* good defaults */
    astatus->goal = tb->goal;
    astatus->fileGoal = tb->fileGoal;
    astatus->procStartTime = tb->procStartTime;
    astatus->procStarts = tb->procStarts;
    astatus->lastAnyExit = tb->lastAnyExit;
    astatus->lastErrorExit = tb->lastErrorExit;
    astatus->errorCode = tb->errorCode;
    astatus->errorSignal = tb->errorSignal;
    if (tb->flags & BNODE_ERRORSTOP)
	astatus->flags |= BOZO_ERRORSTOP;
    if (bnode_HasCore(tb))
	astatus->flags |= BOZO_HASCORE;
    if (!DirAccessOK())
	astatus->flags |= BOZO_BADDIRACCESS;
    return 0;
}

afs_int32
SBOZO_GetInstanceParm(acall, ainstance, anum, aparm)
     struct rx_call *acall;
     char *ainstance;
     afs_int32 anum;
     char **aparm;
{
    register struct bnode *tb;
    register char *tp;
    register afs_int32 code;

    tp = (char *)malloc(BOZO_BSSIZE);
    *aparm = tp;
    *tp = 0;			/* null-terminate string in error case */
    tb = bnode_FindInstance(ainstance);
    if (!tb)
	return BZNOENT;
    bnode_Hold(tb);
    if (anum == 999) {
	if (tb->notifier) {
	    memcpy(tp, tb->notifier, strlen(tb->notifier) + 1);
	    code = 0;
	} else
	    code = BZNOENT;	/* XXXXX */
    } else
	code = bnode_GetParm(tb, anum, tp, BOZO_BSSIZE);
    bnode_Release(tb);

    /* Not Currently Audited */
    return code;
}

afs_int32
SBOZO_GetLog(acall, aname)
     register struct rx_call *acall;
     char *aname;
{
    register afs_int32 code;
    FILE *tfile;
    int tc;
    char *logpath;
    char buffer;
    char caller[MAXKTCNAMELEN];

    /* Check access since 'aname' could specify a file outside of the
     * AFS log directory (which is bosserver's working directory).
     */
    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	code = BZACCESS;
	goto fail;
    }
#ifdef BOS_RESTRICTED_MODE
    if (bozo_isrestricted && strchr(aname, '/')
	&& strcmp(aname, AFSDIR_CANONICAL_SERVER_SLVGLOG_FILEPATH)) {
	code = BZACCESS;
	goto fail;
    }
#endif

    /* construct local path from canonical (wire-format) path */
    if (ConstructLocalLogPath(aname, &logpath)) {
	return BZNOENT;
    }
    if (DoLogging)
	bozo_Log("%s is executing GetLog '%s'\n", caller, logpath);
    tfile = fopen(logpath, "r");
    free(logpath);

    if (!tfile) {
	return BZNOENT;
    }

    while (1) {
	tc = getc(tfile);
	if (tc == 0)
	    continue;		/* our termination condition on other end */
	if (tc == EOF)
	    break;
	buffer = tc;
	if (rx_Write(acall, &buffer, 1) != 1) {
	    fclose(tfile);
	    code = BZNET;
	    goto fail;
	}
    }

    /* all done, cleanup and return */
    fclose(tfile);

    /* write out the end delimeter */
    buffer = 0;
    if (rx_Write(acall, &buffer, 1) != 1)
	return BZNET;
    code = 0;

  fail:
    osi_auditU(acall, BOS_GetLogsEvent, code, AUD_END);
    return code;
}

afs_int32
SBOZO_GetInstanceStrings(acall, abnodeName, as1, as2, as3, as4)
     struct rx_call *acall;
     char *abnodeName;
     char **as1, **as2, **as3, **as4;
{
    register struct bnode *tb;

    *as2 = (char *)malloc(1);
    **as2 = 0;
    *as3 = (char *)malloc(1);
    **as3 = 0;
    *as4 = (char *)malloc(1);
    **as4 = 0;
    tb = bnode_FindInstance(abnodeName);
    if (!tb)
	goto fail;

    /* now, return the appropriate error string, if any */
    if (tb->lastErrorName) {
	*as1 = (char *)malloc(strlen(tb->lastErrorName) + 1);
	strcpy(*as1, tb->lastErrorName);
    } else {
	*as1 = (char *)malloc(1);
	**as1 = 0;
    }
    return 0;

  fail:
    *as1 = (char *)malloc(1);
    **as1 = 0;
    return BZNOENT;
}

#ifdef BOS_RESTRICTED_MODE
afs_int32
SBOZO_GetRestrictedMode(acall, arestmode)
     struct rx_call *acall;
     afs_int32 *arestmode;
{
    *arestmode = bozo_isrestricted;
    return 0;
}

afs_int32
SBOZO_SetRestrictedMode(acall, arestmode)
     struct rx_call *acall;
     afs_int32 arestmode;
{
    afs_int32 code;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(bozo_confdir, acall, caller)) {
	return BZACCESS;
    }
    if (bozo_isrestricted) {
	return BZACCESS;
    }
    if (arestmode != 0 && arestmode != 1) {
	return BZDOM;
    }
    bozo_isrestricted = arestmode;
    code = WriteBozoFile(0);
  fail:
    return code;
}
#else
afs_int32
SBOZO_GetRestrictedMode(acall, arestmode)
     struct rx_call *acall;
     afs_int32 *arestmode;
{
    return RXGEN_OPCODE;
}

afs_int32
SBOZO_SetRestrictedMode(acall, arestmode)
     struct rx_call *acall;
     afs_int32 arestmode;
{
    return RXGEN_OPCODE;
}
#endif

void
bozo_ShutdownAndExit(int asignal)
{
    int code;

    bozo_Log
	("Shutdown of BOS server and processes in response to signal %d\n",
	 asignal);

    /* start shutdown of all processes */
    if ((code = bnode_ApplyInstance(sdproc, NULL)) == 0) {
	/* wait for shutdown to complete */
	code = bnode_ApplyInstance(swproc, NULL);
    }

    if (code) {
	bozo_Log("Shutdown incomplete (code = %d); manual cleanup required\n",
		 code);
    }

    rx_Finalize();
    exit(code);
}
