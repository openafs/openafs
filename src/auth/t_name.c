#include <afsconfig.h>
#include <afs/afsutil.h>
#include <auth/cellconfig.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "rxk5_utilafs.h"

krb5_context
rxk5_get_context(krb5_context x)
{
	int code;
	if (x) return x;
	code = krb5_init_context(&x);
	if (code) x = 0;
	return x;
}

main(int argc, char **argv)
{
	struct afsconf_dir *tdir;
	char buffer[8192];
	krb5_context k5context;
	char *cp, *name;
	int code;

	tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
	if (!tdir) {
		fprintf (stderr,"Cannot open %s\n", AFSDIR_CLIENT_ETC_DIRPATH);
		exit(0);
	}
	while (fgets(buffer, sizeof buffer, stdin))
	{
		cp = strchr(buffer, '\n');
		if (cp) *cp = 0;
		code = afs_rxk5_parse_name_k5(tdir, buffer, &name, argc > 1);
		if (code) {
			printf ("error %d parsing <%s>\n", code, buffer);
			continue;
		}
		printf ("Parsed <%s> as <%s>\n", buffer, name);
		free(name);
	}
	k5context = rxk5_get_context(0);
	if (k5context) krb5_free_context(k5context);
	exit(0);
}
