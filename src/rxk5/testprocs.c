/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/*
 *	testprocs.c
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "test.h"
#include <lock.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef CONFIG_RXKAD
#include <rx/rxkad.h>
#endif
#ifdef CONFIG_RXK5
#include "rxk5.h"
#endif
extern int errno;

int
print_who(call, who)
	register struct rx_call *call;
	char *who;
{
	int level, kvno, enctype;
	time_t expires;
	char *name = 0;
	int code;
	struct rx_connection *conn = rx_ConnectionOf(call);
	struct rx_peer *peer = rx_PeerOf(conn);
	int host = ntohl(rx_HostOf(peer));
	int si;

	printf ("%ld: %s from %d.%d.%d.%d(%d) ", time(0), who,
		(host>>24)&255,
		(host>>16)&255,
		(host>>8)&255,
		host&255,
		ntohs(rx_PortOf(peer)));

	si = rx_SecurityClassOf(conn);
#ifdef CONFIG_RXKAD
	if (si == 2)
	{
		char tname[64];
		char tinst[64];
		char tcell[64];
		code = rxkad_GetServerInfo(conn,
			(rxkad_level *) &level, (unsigned int *) &expires,
			tname, tinst, tcell, &kvno);
		if (code) printf ("noauth");
		else {
			printf ("%s.%s@%s", tname, tinst, tcell);
			printf (" l=%d kv=%d", *(rxkad_level *)&level, kvno);
			printf (" expires=%d", (int) expires);
		}
	} else {
#endif
		code = rxk5_GetServerInfo(call->conn, &level, &expires,
			&name,
			&kvno, &enctype);
		if (code)
		{
			printf ("noauth");
		} else {
			printf ("%s", name);
			printf (" l=%d e=%d kv=%d", level, enctype, kvno);
			printf (" expires=%d", (int) expires);
		}
#ifdef CONFIG_RXKAD
	}
#endif
	printf ("\n");
	return code;
}

int
STEST_test(z_call, n)
	register struct rx_call *z_call;
{
	(void) print_who(z_call, "TEST_test");
	printf (" n = %d\n", n);
	return 0;
}

int
STEST_fib(z_call, n, name, output)
	register struct rx_call *z_call;
	char **name;
	fib_results *output;
{
	char foo[80];
	unsigned int *results;
	int i;

	(void) print_who(z_call, "TEST_fib");
	sprintf(foo, "Results for %d", n);
printf (" about to store name <%s>\n", foo);
	*name = strdup(foo);
	results = (int *) malloc(sizeof *results * (n+8));
printf (" About to store results in %#x\n", (int) results);
	if (n > 0)
		results[0] = 1;
	if (n > 1)
		results[1] = 1;
printf (" About to store rest of results (%d)\n", n);
	for (i = 2; i < n; ++i)
	{
		results[i] = results[i-1] + results[i-2];
	}
printf (" storing result pointers in %#x\n", (int) output);
	output->fib_results_val = results;
	output->fib_results_len = n;
printf (" done\n");
	return 0;
}
