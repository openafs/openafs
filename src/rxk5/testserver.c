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
 *	testserver.c
 */

#include "afsconfig.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>
#include <syslog.h>
#include <memory.h>			     /* for memset, etc. */
#include <netinet/in.h>
#ifdef CONFIG_RXK5
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
#endif
#ifdef CONFIG_RXKAD
#endif
#include <afs/stds.h>
#include "rx/rx_globals.h"

#include "test.h"

/*
 * External Functions
 */
#ifdef __STDC__
extern int TEST_ExecuteRequest(struct rx_call *);
extern void rx_StartServer(int);
#else
extern int TEST_ExecuteRequest();
extern void rx_StartServer();
#endif

/*
 * Forward Definitions
 */
void	usage();

extern struct rx_securityClass *rxnull_NewServerSecurityObject();
#ifdef CONFIG_RXKAD
extern int rsk_Wrap();
extern int rsk_Init();
extern struct rx_securityClass *rxkad_NewServerSecurityObject();
#endif
#ifdef CONFIG_RXK5
extern struct rx_securityClass *rxk5_NewServerSecurityObject();
#endif

extern int rx_stackSize;

char	*progname;

/*
 * some globals set by options processing:
 */

/*
 * the server main routine.
 *	process options
 *	initialize
 *	become just another rx thread (& lose our individual identity)
 */

int
main(argc,argv)
	char **argv;
{
#if CONFIG_RXKAD
	char *srvtab = 0;
#endif
#if CONFIG_RXK5
	char *keytab = 0;
#endif
	char *principal = 0;
	int udp_port=TESTPORT;			/* port to use */
	struct rx_securityClass *(sc[6]);	/* Security objects */
	struct rx_service *rxserv_ptr;
	int code;				/* Rx return code */
	char *argp;

	/*
	 * Argument processing
	 */

	progname = argv[0];	/* store the program's name */

#define ERROR(e)	do {argp=(e); goto Error; }while (0);
	while (--argc) if (*(argp = *++argv) == '-')
	while (*++argp) switch(*argp)
	{
#if CONFIG_RXKAD
	case 'F':	/* specify where our srvtab lives */
		if (argc < 1) ERROR("Missing srvtab")
		srvtab = *++argv;
		--argc;
		break;
#endif
#if CONFIG_RXK5
	case 'f':	/* specify where our keytab lives */
		if (argc < 1) ERROR("Missing keytab")
		keytab = *++argv;
		--argc;
		break;
#endif
	case 'p':	/* specify an alternate port */
		if (argc < 1) ERROR("Missing port")
		udp_port = atol(*++argv);
		--argc;
		break;
	case 's':	/* name the principle (should be what's in keytab) */
		if (argc < 1) ERROR("Missing principle")
		principal = *++argv;
		--argc;
		break;
	case 'u':	/* for debugging, turn output buffering off */
		setbuf(stderr, NULL);
		setbuf(stdout, NULL);
		break;
	case 'i':
		if (argc < 1) ERROR("Missing idle timeout")
		rx_idleConnectionTime = atol(*++argv);
		--argc;
		break;
	case '?':
	default:
		fprintf(stderr,"Bad switch <%c>\n", *argp);
		argp = 0;
	Error:
		if (argp)
			fprintf (stderr,"Error: %s\n", argp);
		usage();
		exit(!!argp);
		break;
	}
	else ERROR("No anonymous args yet")

#ifdef ultrix
	openlog("testd", LOG_PID);
#else
	openlog("testd", LOG_NDELAY|LOG_PID, LOG_DAEMON);
#endif

#if CONFIG_RXKAD
	if (!srvtab)
		;
	else if ((code = rsk_Init(principal, srvtab)))
	{
		fprintf(stderr, "%s: ERROR rsk_Init() failed with code %d\n",
			progname, code);
		exit(1);
	}
#endif

    /*
     * Start priming rx server
     */
	code = rx_Init(htons(udp_port));
	if (code)
	{
		fprintf(stderr, "%s: ERROR rx_Init() failed with code %d\n",
			progname, code);
		exit(1);
	}

	memset((void*) sc, 0, sizeof sc);
	if (!(sc[0] = rxnull_NewServerSecurityObject()))
	{
		fprintf(stderr,
			"%s: ERROR unable to create null server security object\n",
			progname);
		exit(1);
	}
#if CONFIG_RXKAD
	if (srvtab)
	sc[2] = (struct rx_securityClass *)
		rxkad_NewServerSecurityObject(0, 0, rsk_Wrap, (int *) 0);
#endif
#if CONFIG_RXK5
	if (keytab)
	sc[5] = (struct rx_securityClass *)
		rxk5_NewServerSecurityObject(rxk5_clear,
			keytab, rxk5_default_get_key, 0, 0);
#endif

	rxserv_ptr = rx_NewService(0,
		(u_short) TESTSERVICEID, "testd",
		sc, 6, TEST_ExecuteRequest);
	if (!rxserv_ptr)
	{
		fprintf(stderr, "%s: ERROR unable to create rx service\n",
			progname);
		exit(1);
	}

	rx_SetStackSize(rxserv_ptr, (int) (RX_DEFAULT_STACK_SIZE * 16));

#define MAXREQUESTS 8
	rx_SetMinProcs(rxserv_ptr, 4);
	rx_SetMaxProcs(rxserv_ptr, MAXREQUESTS);

	rx_StartServer(1);

	fprintf(stderr, "%s: ERROR returned from rx_StartServer()!!!\n",
		progname);

	exit(1);
}

/*
 * usage--basic usage function, prints error message on stderr
 */
void
usage()
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", progname);
#ifdef CONFIG_RXKAD
    fprintf(stderr, "\t-F srvtab -- requires -s\n");
#endif
#ifdef CONFIG_RXK5
    fprintf(stderr, "\t-f keytab\n");
#endif
    fprintf(stderr, "\t-p local-udp-port\n");
    fprintf(stderr, "\t-s principal -- requires -f\n");
    fprintf(stderr, "\t-u -- turn stderr/stdout buffering off\n");
    fprintf(stderr, "\t-i n -- set server idle connection reap timeout\n");

    return;
}
