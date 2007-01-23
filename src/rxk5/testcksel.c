#include "afsconfig.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef USING_SHISHI
#include <shishi.h>
#else
#ifdef USING_SSL
#include <sys/types.h>
#include "k5ssl.h"
#else
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include <krb5.h>
#endif
#endif
#include <assert.h>
#include "rxk5.h"

int
main(int argc, char **argv)
{
	int i, j;
	for (i = -20; i < 30; ++i)
	{
		if (rxk5i_select_ctype(0, i, &j) < 0)
			continue;
		printf ("%d %d\n", i, j);
	}
	exit(0);
}
