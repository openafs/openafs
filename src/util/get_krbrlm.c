/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file <mit-copyright.h>.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <string.h>
#include <ctype.h>
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

int
afs_krb_get_lrealm(char *r, int n)
{
    FILE *cnffile/*, *fopen()*/;

    if (n > 1)
	return (KFAILURE);	/* Temporary restriction */

    if ((cnffile = fopen(AFSDIR_SERVER_KCONF_FILEPATH, "r")) == NULL) {
	return (KFAILURE);
    }
    if (fscanf(cnffile, "%s", r) != 1) {
	(void)fclose(cnffile);
	return (KFAILURE);
    }
    (void)fclose(cnffile);
    return (KSUCCESS);
}
