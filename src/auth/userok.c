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


#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <stdlib.h>		/* for realpath() */
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <rx/xdr.h>
#include <rx/rx.h>
#include <stdio.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>

#ifdef AFS_ATHENA_STDENV
#include <krb.h>
#endif

#include "auth.h"
#include "cellconfig.h"
#include "keys.h"
#include "afs/audit.h"

#if !defined(UKERNEL)
int
afsconf_CheckAuth(void *arock, struct rx_call *acall)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) arock;
    int rc;
    LOCK_GLOBAL_MUTEX;
    rc = ((afsconf_SuperUser(adir, acall, NULL) == 0) ? 10029 : 0);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}
#endif /* !defined(UKERNEL) */

static int
GetNoAuthFlag(struct afsconf_dir *adir)
{
    if (access(AFSDIR_SERVER_NOAUTH_FILEPATH, 0) == 0) {
	osi_audit(NoAuthEvent, 0, AUD_END);	/* some random server is running noauth */
	return 1;		/* if /usr/afs/local/NoAuth file exists, allow access */
    }
    return 0;
}


int
afsconf_GetNoAuthFlag(struct afsconf_dir *adir)
{
    int rc;

    LOCK_GLOBAL_MUTEX;
    rc = GetNoAuthFlag(adir);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}

void
afsconf_SetNoAuthFlag(struct afsconf_dir *adir, int aflag)
{
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    if (aflag == 0) {
	/* turn off noauth flag */
	code = (unlink(AFSDIR_SERVER_NOAUTH_FILEPATH) ? errno : 0);
	osi_audit(NoAuthDisableEvent, code, AUD_END);
    } else {
	/* try to create file */
	code =
	    open(AFSDIR_SERVER_NOAUTH_FILEPATH, O_CREAT | O_TRUNC | O_RDWR,
		 0666);
	if (code >= 0) {
	    close(code);
	    osi_audit(NoAuthEnableEvent, 0, AUD_END);
	} else
	    osi_audit(NoAuthEnableEvent, errno, AUD_END);
    }
    UNLOCK_GLOBAL_MUTEX;
}

/* deletes a user from the UserList file */
int
afsconf_DeleteUser(struct afsconf_dir *adir, char *auser)
{
    char *filename, *nfilename;
    char *buffer;
    FILE *tf;
    FILE *nf;
    int flag;
    char tname[64 + 1];
    char *tp;
    int found;
    struct stat tstat;
    afs_int32 code;

    buffer = malloc(AFSDIR_PATH_MAX);
    if (buffer == NULL)
	return ENOMEM;
    filename = malloc(AFSDIR_PATH_MAX);
    if (filename == NULL) {
	free(buffer);
	return ENOMEM;
    }

    LOCK_GLOBAL_MUTEX;
    strcompose(filename, AFSDIR_PATH_MAX, adir->name, "/",
	       AFSDIR_ULIST_FILE, NULL);
#ifndef AFS_NT40_ENV
    {
	/*
	 * We attempt to fully resolve this pathname, so that the rename
	 * of the temporary file will work even if UserList is a symlink
	 * into a different filesystem.
	 */
	nfilename = malloc(AFSDIR_PATH_MAX);
	if (nfilename == NULL) {
	    UNLOCK_GLOBAL_MUTEX;
	    free(filename);
	    free(buffer);
	    return ENOMEM;
	}
	if (realpath(filename, nfilename)) {
	    free(filename);
	    filename = nfilename;
	} else {
	    free(nfilename);
	}
    }
#endif /* AFS_NT40_ENV */
    if (afs_asprintf(&nfilename, "%s.NXX", filename) < 0) {
	UNLOCK_GLOBAL_MUTEX;
	free(filename);
	free(buffer);
	return -1;
    }
    tf = fopen(filename, "r");
    if (!tf) {
	UNLOCK_GLOBAL_MUTEX;
	free(filename);
	free(nfilename);
	free(buffer);
	return -1;
    }
    code = stat(filename, &tstat);
    if (code < 0) {
	UNLOCK_GLOBAL_MUTEX;
	free(filename);
	free(nfilename);
	free(buffer);
	return code;
    }
    nf = fopen(nfilename, "w+");
    if (!nf) {
	fclose(tf);
	UNLOCK_GLOBAL_MUTEX;
	free(filename);
	free(nfilename);
	free(buffer);
	return EIO;
    }
    flag = 0;
    found = 0;
    while (1) {
	/* check for our user id */
	tp = fgets(buffer, AFSDIR_PATH_MAX, tf);
	if (tp == NULL)
	    break;
	code = sscanf(buffer, "%64s", tname);
	if (code == 1 && strcmp(tname, auser) == 0) {
	    /* found the guy, don't copy to output file */
	    found = 1;
	} else {
	    /* otherwise copy original line  to output */
	    fprintf(nf, "%s", buffer);
	}
    }
    fclose(tf);
    free(buffer);
    if (ferror(nf))
	flag = 1;
    if (fclose(nf) == EOF)
	flag = 1;
    if (flag == 0) {
	/* try the rename */
	flag = renamefile(nfilename, filename);
	if (flag == 0)
	    flag = chmod(filename, tstat.st_mode);
    } else
	unlink(nfilename);

    /* finally, decide what to return to the caller */
    UNLOCK_GLOBAL_MUTEX;
    free(filename);
    free(nfilename);
    if (flag)
	return EIO;		/* something mysterious went wrong */
    if (!found)
	return ENOENT;		/* entry wasn't found, no changes made */
    return 0;			/* everything was fine */
}

/* returns nth super user from the UserList file */
int
afsconf_GetNthUser(struct afsconf_dir *adir, afs_int32 an, char *abuffer,
		   afs_int32 abufferLen)
{
    char *tbuffer;
    FILE *tf;
    char tname[64 + 1];
    char *tp;
    int flag;
    afs_int32 code;

    tbuffer = malloc(AFSDIR_PATH_MAX);
    if (tbuffer == NULL)
	return ENOMEM;

    LOCK_GLOBAL_MUTEX;
    strcompose(tbuffer, AFSDIR_PATH_MAX, adir->name, "/",
	       AFSDIR_ULIST_FILE, NULL);
    tf = fopen(tbuffer, "r");
    if (!tf) {
	UNLOCK_GLOBAL_MUTEX;
	free(tbuffer);
	return 1;
    }
    flag = 1;
    while (1) {
	/* check for our user id */
	tp = fgets(tbuffer, AFSDIR_PATH_MAX, tf);
	if (tp == NULL)
	    break;
	code = sscanf(tbuffer, "%64s", tname);
	if (code == 1 && an-- == 0) {
	    flag = 0;
	    break;
	}
    }
    if (flag == 0)
	strcpy(abuffer, tname);
    fclose(tf);
    UNLOCK_GLOBAL_MUTEX;
    free(tbuffer);
    return flag;
}

/* returns true iff user is in the UserList file */
static int
FindUser(struct afsconf_dir *adir, char *auser)
{
    char *tbuffer;
    bufio_p bp;
    char tname[64 + 1];
    int flag;
    afs_int32 code;
    int rc;

    tbuffer = malloc(AFSDIR_PATH_MAX);
    if (tbuffer == NULL)
	return 0;

    strcompose(tbuffer, AFSDIR_PATH_MAX, adir->name, "/", AFSDIR_ULIST_FILE,
	       NULL);
    bp = BufioOpen(tbuffer, O_RDONLY, 0);
    if (!bp) {
	free(tbuffer);
	return 0;
    }
    flag = 0;
    while (1) {
	/* check for our user id */
	rc = BufioGets(bp, tbuffer, AFSDIR_PATH_MAX);
	if (rc < 0)
	    break;
	code = sscanf(tbuffer, "%64s", tname);
	if (code == 1 && strcmp(tname, auser) == 0) {
	    flag = 1;
	    break;
	}
    }
    BufioClose(bp);
    free(tbuffer);
    return flag;
}

/* add a user to the user list, checking for duplicates */
int
afsconf_AddUser(struct afsconf_dir *adir, char *aname)
{
    FILE *tf;
    afs_int32 code;
    char *tbuffer;

    LOCK_GLOBAL_MUTEX;
    if (FindUser(adir, aname)) {
	UNLOCK_GLOBAL_MUTEX;
	return EEXIST;		/* already in the list */
    }

    tbuffer = malloc(AFSDIR_PATH_MAX);
    strcompose(tbuffer, AFSDIR_PATH_MAX, adir->name, "/", AFSDIR_ULIST_FILE,
	       NULL);
    tf = fopen(tbuffer, "a+");
    free(tbuffer);
    if (!tf) {
	UNLOCK_GLOBAL_MUTEX;
	return EIO;
    }
    fprintf(tf, "%s\n", aname);
    code = 0;
    if (ferror(tf))
	code = EIO;
    if (fclose(tf))
	code = EIO;
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

/* special CompFindUser routine that builds up a princ and then
	calls finduser on it. If found, returns char * to user string,
	otherwise returns NULL. The resulting string should be immediately
	copied to other storage prior to release of mutex. */
static char *
CompFindUser(struct afsconf_dir *adir, char *name, char *sep, char *inst,
	     char *realm)
{
    static char fullname[MAXKTCNAMELEN + MAXKTCNAMELEN + MAXKTCREALMLEN + 3];

    /* always must have name */
    if (!name || !name[0]) {
	return NULL;
    }

    if (strlcpy(fullname, name, sizeof(fullname)) >= sizeof(fullname))
	return NULL;

    /* might have instance */
    if (inst && inst[0]) {
	if (!sep || !sep[0]) {
	    return NULL;
	}

	if (strlcat(fullname, sep, sizeof(fullname)) >= sizeof(fullname))
	    return NULL;

	if (strlcat(fullname, inst, sizeof(fullname)) >= sizeof(fullname))
	    return NULL;
    }

    /* might have realm */
    if (realm && realm[0]) {
	if (strlcat(fullname, "@", sizeof(fullname)) >= sizeof(fullname))
	    return NULL;

	if (strlcat(fullname, realm, sizeof(fullname)) >= sizeof(fullname))
	    return NULL;
    }

    if (FindUser(adir, fullname)) {
	return fullname;
    } else {
	return NULL;
    }
}


/* make sure user authenticated on rx call acall is in list of valid
    users. Copy the "real name" of the authenticated user into namep
    if a pointer is passed.
*/
afs_int32
afsconf_SuperUser(struct afsconf_dir *adir, struct rx_call *acall, char *namep)
{
    struct rx_connection *tconn;
    afs_int32 code;
    int flag;

    LOCK_GLOBAL_MUTEX;
    if (!adir) {
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }

    if (afsconf_GetNoAuthFlag(adir)) {
	if (namep)
	    strcpy(namep, "<NoAuth>");
	UNLOCK_GLOBAL_MUTEX;
	return 1;
    }

    tconn = rx_ConnectionOf(acall);
    code = rx_SecurityClassOf(tconn);
    if (code == 0) {
	UNLOCK_GLOBAL_MUTEX;
	return 0;		/* not authenticated at all, answer is no */
    } else if (code == 1) {
	/* bcrypt tokens */
	UNLOCK_GLOBAL_MUTEX;
	return 0;		/* not supported any longer */
    } else if (code == 2) {
	char tname[MAXKTCNAMELEN];	/* authentication from ticket */
	char tinst[MAXKTCNAMELEN];
	char tcell[MAXKTCREALMLEN];
	char tcell_l[MAXKTCREALMLEN];
	char *tmp;

	/* keep track of which one actually authorized request */
	char uname[MAXKTCNAMELEN + MAXKTCNAMELEN + MAXKTCREALMLEN + 3];

	afs_uint32 exp;
	static char lcell[MAXCELLCHARS] = "";
	static char lrealms[AFS_NUM_LREALMS][AFS_REALM_SZ];
	static int  num_lrealms = -1;
	int lrealm_match = 0, i;

	/* get auth details from server connection */
	code =
	    rxkad_GetServerInfo(acall->conn, NULL, &exp, tname, tinst, tcell,
				NULL);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;		/* bogus connection/other error */
	}

	/* don't bother checking anything else if tix have expired */
#ifdef AFS_PTHREAD_ENV
	if (exp < clock_Sec()) {
#else
	if (exp < FT_ApproxTime()) {
#endif
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;		/* expired tix */
	}

	/* generate lowercased version of cell name */
	strcpy(tcell_l, tcell);
	tmp = tcell_l;
	while (*tmp) {
	    *tmp = tolower(*tmp);
	    tmp++;
	}

	/* determine local cell name. It's static, so will only get
	 * calculated the first time through */
	if (!lcell[0])
	    afsconf_GetLocalCell(adir, lcell, sizeof(lcell));

	/* if running a krb environment, also get the local realm */
	/* note - this assumes AFS_REALM_SZ <= MAXCELLCHARS */
	/* just set it to lcell if it fails */
	if (num_lrealms == -1) {
	    for (i=0; i<AFS_NUM_LREALMS; i++) {
		if (afs_krb_get_lrealm(lrealms[i], i) != 0 /*KSUCCESS*/)
		    break;
	    }

	    if (i == 0) {
		strncpy(lrealms[0], lcell, AFS_REALM_SZ);
		num_lrealms = 1;
	    } else {
		num_lrealms = i;
	    }
	}

	/* See if the ticket cell matches one of the local realms */
	lrealm_match = 0;
	for ( i=0;i<num_lrealms;i++ ) {
	    if (!strcasecmp(lrealms[i], tcell)) {
		lrealm_match = 1;
		break;
	    }
	}

	/* If yes, then make sure that the name is not present in
	 * an exclusion list */
	if (lrealm_match) {
	    if (tinst[0])
		snprintf(uname,sizeof(uname),"%s.%s@%s",tname,tinst,tcell);
	    else
		snprintf(uname,sizeof(uname),"%s@%s",tname,tcell);

	    if (afs_krb_exclusion(uname))
		lrealm_match = 0;
	}

	/* start with no uname and no authorization */
	strcpy(uname, "");
	flag = 0;

	/* localauth special case */
	if (strlen(tinst) == 0 && strlen(tcell) == 0
	    && !strcmp(tname, AUTH_SUPERUSER)) {
	    strcpy(uname, "<LocalAuth>");
	    flag = 1;

	    /* cell of connection matches local cell or one of the realms */
	} else if (!strcasecmp(tcell, lcell) || lrealm_match) {
	    if ((tmp = CompFindUser(adir, tname, ".", tinst, NULL))) {
		strcpy(uname, tmp);
		flag = 1;
#ifdef notyet
	    } else if ((tmp = CompFindUser(adir, tname, "/", tinst, NULL))) {
		strcpy(uname, tmp);
		flag = 1;
#endif
	    }
	    /* cell of conn doesn't match local cell or realm */
	} else {
	    if ((tmp = CompFindUser(adir, tname, ".", tinst, tcell))) {
		strcpy(uname, tmp);
		flag = 1;
#ifdef notyet
	    } else if ((tmp = CompFindUser(adir, tname, "/", tinst, tcell))) {
		strcpy(uname, tmp);
		flag = 1;
#endif
	    } else if ((tmp = CompFindUser(adir, tname, ".", tinst, tcell_l))) {
		strcpy(uname, tmp);
		flag = 1;
#ifdef notyet
	    } else if ((tmp = CompFindUser(adir, tname, "/", tinst, tcell_l))) {
		strcpy(uname, tmp);
		flag = 1;
#endif
	    }
	}

	if (namep)
	    strcpy(namep, uname);
	UNLOCK_GLOBAL_MUTEX;
	return flag;
    } else {			/* some other auth type */
	UNLOCK_GLOBAL_MUTEX;
	return 0;		/* mysterious, just say no */
    }
}
