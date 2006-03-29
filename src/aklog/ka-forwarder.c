/*
 * COPYRIGHT NOTICE
 * Copyright (c) 1994 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * See <cmu_copyright.h> for use and distribution information.
 */

/*
 * HISTORY
 * $Log: ka-forwarder.c,v $
 * Revision 1.1  1997/06/03 18:23:54  kenh
 * .
 *
 * Revision 1.4  1996/08/09  01:00:21  jhutz
 * 	When initializing the array of fakeka servers, remember to set
 * 	the address family of each server; otherwise SunOS complains.
 * 	[1996/08/09  00:58:46  jhutz]
 *
 * Revision 1.3  1996/08/09  00:17:19  jhutz
 * 	Merged in changes from Chuck Silvers:
 * 	- Support for more than one fakeka server
 * 	- Support for specifying ports for each fakeka server separately from the
 * 	  others, and from the port we listen on.
 * 
 * 	Plus a minor bug fix to Chuck's code.
 * 	Basically, this version is designed to provide both reliability and
 * 	load-balancing cheaply.  Basically, we forward packets to all of the
 * 	fakeka servers in round-robin fashion.  So, if a client is losing on
 * 	one server, its retry should go to a different one, if more than one
 * 	is specified.
 * 	[1996/08/03  02:13:36  jhutz]
 * 
 * Revision 1.2  1995/02/23  18:26:36  chs
 * 	Created.
 * 	[1995/02/23  18:26:03  chs]
 * 
 * $EndLog$
 */

/*
 * This program is intended to run on afs DB servers.
 * Its function is to forward KA requests to a fakeka server
 * running on an MIT kerberos server.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#if	HAVE_GETOPT_H
#include <getopt.h>
#else
int getopt (int, char * const *, const char *);
int optind, opterr;
char *optarg;
#endif

#define BUFFER_SIZE 2048


char *prog;

int num_servers, cur_server;
struct sockaddr_in *servers;


void
perrorexit(str)
char *str;
{
    perror(str);
    exit(1);
}


void
setup_servers(argc, argv)
int argc;
char **argv;
{
    int i;
    u_int fwdaddr;
    u_short fwdport;

    num_servers = argc;

    servers = malloc(sizeof(*servers) * num_servers);
    if (servers == NULL)
	perrorexit("malloc failed");

    for (i = 0; i < num_servers; i++) {
	char *host, *port;

	fwdport = htons(7004);

	host = argv[i];
	port = strchr(host, '/');
	if (port != NULL) {
	    *port++ = 0;

	    if (isdigit(port[0])) {
		fwdport = htons(atoi(port));
	    }
	    else {
		struct servent *srv = getservbyname(port, "udp");
		if (!srv) {
		    fprintf(stderr, "%s: unknown service %s\n", prog, port);
		    exit(1);
		}
		fwdport = srv->s_port;
	    }
	}

	if (isdigit(host[0])) {
	    fwdaddr = inet_addr(host);
	}
	else {
	    struct hostent *h = gethostbyname(host);
	    if (!h) {
		fprintf(stderr, "%s: unknown host %s\n", prog, host);
		exit(1);
	    }
	    bcopy(h->h_addr, &fwdaddr, 4);
	}

	servers[i].sin_family = AF_INET;
	servers[i].sin_addr.s_addr = fwdaddr;
	servers[i].sin_port = fwdport;
    }
}


int
setup_socket(port)
u_short port;
{
    int s, rv;
    struct sockaddr_in sin;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	perrorexit("Couldn't create socket");

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);

    rv = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (rv < 0)
	perrorexit("Couldn't bind socket");

    return s;
}


int
packet_is_reply(from)
struct sockaddr_in *from;
{
    int i;

    for (i = 0; i < num_servers; i++) {
	struct sockaddr_in *sin = &servers[i];

	if (from->sin_addr.s_addr == sin->sin_addr.s_addr &&
	    from->sin_port == sin->sin_port)
	{
	    return 1;
	}
    }

    return 0;
}


int
main(argc, argv)
int argc;
char **argv;
{
    int c, s, rv;
    u_short port;

    if (argc < 2) {
	fprintf(stderr,
		"usage: %s [-p port] <host>[/port] [host/port ...]\n",
		argv[0]);
	exit(1);
    }

    prog = argv[0];
    port = 7004;

    while ((c = getopt(argc, argv, "p:")) != -1) {
	switch (c) {
	case 'p':
	    port = atoi(optarg);
	    break;
	default:
	    fprintf(stderr, "%s: invalid option '%c'\n", prog, c);
	    exit(1);
	}
    }

    /*
     * hmm, different implementations of getopt seem to do different things
     * when there aren't any options.  linux sets optind = 1, which I would
     * call correct, but sunos sets optind = 0.  try to do the right thing.
     */
    if (optind == 0)
	optind = 1;

    setup_servers(argc - optind, argv + optind);
    s = setup_socket(port);

    openlog("ka-forwarder", LOG_PID, LOG_DAEMON);

    for (;;) {
	char buf[BUFFER_SIZE], *bufp, *sendptr;
	struct sockaddr_in from, reply, *to;
	int fromlen, sendlen;

	bufp = buf + 8;
	fromlen = sizeof(from);

	rv = recvfrom(s, bufp, sizeof(buf) - 8,
		      0, (struct sockaddr *)&from, &fromlen);
	if (rv < 0) {
	    syslog(LOG_ERR, "recvfrom: %m");
	    sleep(1);
	    continue;
	}

	if (packet_is_reply(&from)) {
	    /* this is a reply, forward back to user */

	    to = &reply;
	    reply.sin_family = AF_INET;
	    bcopy(bufp, &reply.sin_addr.s_addr, 4);
	    bcopy(bufp + 4, &reply.sin_port, 2);
	    sendptr = bufp + 8;
	    sendlen = rv - 8;
	}
	else {
	    /* this is a request, forward to server */

	    cur_server = (cur_server + 1) % num_servers;
	    to = &servers[cur_server];

	    bcopy(&from.sin_addr.s_addr, bufp - 8, 4);
	    bcopy(&from.sin_port, bufp - 4, 2);

	    sendptr = bufp - 8;
	    sendlen = rv + 8;
	}

	{
	    char a1[16], a2[16];
	    strcpy(a1, inet_ntoa(from.sin_addr));
	    strcpy(a2, inet_ntoa(to->sin_addr));

	    syslog(LOG_INFO, "forwarding %d bytes from %s/%d to %s/%d\n",
		   sendlen, a1, htons(from.sin_port), a2, htons(to->sin_port));
	}

	rv = sendto(s, sendptr, sendlen,
		    0, (struct sockaddr *)to, sizeof(*to));
	if (rv < 0) {
	    syslog(LOG_ERR, "sendto: %m");
	}
    }
}
