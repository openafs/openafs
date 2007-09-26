#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#if USING_HEIMDAL
#include "parse_units.h"
#endif
#include "krb5.h"
#else
#include "k5ssl.h"
#endif

int exitcode;

int
main(argc,argv)
	char **argv;
{
	int f;
	char *argp;
	krb5_data c[1], s[1];
	int rbc = KRB5_REALM_BRANCH_CHAR;
	krb5_context context = 0;
	krb5_principal p, *t;
	char *what;
	char *n;
	int i, code;

	f = 0;
	while (--argc > 0) if (*(argp = *++argv)=='-')
	while (*++argp) switch(*argp)
	{
#if 0
	case 'Z':
		if (argc <= 1) goto Usage;
		--argc;
		offset = atol(*++argv);
		break;
#endif
	case '-':
		break;
	default:
		fprintf (stderr,"Bad switch char <%c>\n", *argp);
	Usage:
		fprintf(stderr, "Usage: t_w client_realm server_realm [c]\n");
		exit(1);
	}
	else switch(f++)
	{
	case 0:
		c->data = argp;
		break;
	case 1:
		s->data = argp;
		break;
	case 2:
		rbc = *argp;
		break;
	default:
		goto Usage;
	}
	if (f < 2) goto Usage;

	c->length = strlen(c->data);
	s->length = strlen(s->data);

	what = "krb5_init_context";
	code = krb5_init_context(&context);
	if (code) goto Failed;

	what = "krb5_walk_realm_tree";
	code = krb5_walk_realm_tree(context, c, s, &t, rbc);
	if (code) goto Failed;

	for (i = 0; (p = t[i]); ++i)
	{
		n = 0;
		what = "krb5_unparse_name";
		code = krb5_unparse_name(context, p, &n);
		if (code) goto Failed;
		printf ("%s\n", n);
		free(n);
	}
	krb5_free_realm_tree(context, t);
Failed:
	if (code)
		fprintf(stderr,"Failed in %s - %d\n", what, code);
	exitcode |= !!code;
	exit(exitcode);
}
