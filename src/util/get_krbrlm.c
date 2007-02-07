/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file <mit-copyright.h>.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include "afsutil.h"

/*
 * Specialized version of the kerberos krb_get_lrealm function.
 * krb_get_lrealm takes a pointer to a string, and a number, n.  It fills
 * in the string, r, with the name of the nth realm specified on the
 * first line of the kerberos config file (KRB_CONF, defined in "krb.h").
 * It returns 0 (KSUCCESS) on success, and KFAILURE on failure. 
 *
 * On the kerberos version if the config file does not exist, and if n=1, a 
 * successful return will occur with r = KRB_REALM (also defined in "krb.h").
 *
 */
#define	KSUCCESS	0
#define	KFAILURE	(-1)

static char *
parse_str(char *buffer, char *result, int size)
{
    int n=0;

    if (!buffer)
        goto cleanup;

    while (*buffer && isspace(*buffer))
        buffer++;
    while (*buffer && !isspace(*buffer)) {
	if (n < size - 1) {
	    *result++=*buffer++;
	    n++;
	} else {
	    buffer++;
	}
    }

  cleanup:
    *result='\0';
    return buffer;
}


int
afs_krb_get_lrealm(char *r, int n)
{
    char linebuf[2048];
    char tr[AFS_REALM_SZ] = "";
    char *p;
    FILE *cnffile/*, *fopen()*/;
    int i;
    int rv = KFAILURE;

    *r = '\0';

    if ((cnffile = fopen(AFSDIR_SERVER_KCONF_FILEPATH, "r")) == NULL) {
	return (KFAILURE);
    }
    if (fgets(linebuf, sizeof(linebuf)-1, cnffile) == NULL) {
	goto cleanup;
    }
    linebuf[sizeof(linebuf)-1] = '\0';
    for (i=0, p=linebuf; i<=n && *p; i++) {
        p = parse_str(p, tr, AFS_REALM_SZ);
    }

    if (*tr) {
	strcpy(r,tr);
	rv = KSUCCESS;
    }

  cleanup:
    (void)fclose(cnffile);
    return rv;
}

int
afs_krb_exclusion(char * name)
{
    char linebuf[2048];
    char excl_name[256] = "";
    FILE *cnffile/*, *fopen()*/;
    int exclude = 0;

    if ((cnffile = fopen(AFSDIR_SERVER_KRB_EXCL_FILEPATH, "r")) == NULL)
	return exclude;

    for (;;) {
	if (fgets(linebuf, sizeof(linebuf)-1, cnffile) == NULL) {
	    goto cleanup;
	}
	linebuf[sizeof(linebuf)-1] = '\0';
        parse_str(linebuf, excl_name, sizeof(excl_name));

	if (!strcmp(name,excl_name)) {
	    exclude = 1;
	    break;
	}
    }

  cleanup:
    (void)fclose(cnffile);
    return exclude;
}

int 
afs_is_foreign_ticket_name(char *tname, char *tinst, char * tcell, char *localrealm)
{
    int foreign = 0;

    if (localrealm && strcasecmp(localrealm, tcell))
	foreign = 1;

#if	defined(AFS_ATHENA_STDENV) || defined(AFS_KERBREALM_ENV)
    if (foreign) {
	static char local_realms[AFS_NUM_LREALMS][AFS_REALM_SZ];
	static int  num_lrealms = -1;
	int lrealm_match, i;
	char uname[256];

	if (num_lrealms == -1) {
	    for (i=0; i<AFS_NUM_LREALMS; i++) {
		if (afs_krb_get_lrealm(local_realms[i], i) != 0 /*KSUCCESS*/)
		    break;
	    }

	    if (i==0 && localrealm) {
		strncpy(local_realms[0], localrealm, AFS_REALM_SZ);
		num_lrealms = 1;
	    } else {
		num_lrealms = i;
	    }
	}

	/* See if the ticket cell matches one of the local realms */
	lrealm_match = 0;
	for ( i=0;i<num_lrealms;i++ ) {
	    if (!strcasecmp(local_realms[i], tcell)) {
		lrealm_match = 1;
		break;
	    }
	}

	/* If yes, then make sure that the name is not present in 
	 * an exclusion list */
	if (lrealm_match) {
	    if (tinst && tinst[0])
		snprintf(uname,sizeof(uname),"%s.%s@%s",tname,tinst,tcell);
	    else
		snprintf(uname,sizeof(uname),"%s@%s",tname,tcell);

	    if (afs_krb_exclusion(uname))
		lrealm_match = 0;
	}

	foreign = !lrealm_match;
    }
#endif
    return foreign;
}



