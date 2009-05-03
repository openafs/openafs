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
 * test utility.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <afs/param.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/errno.h>
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <com_err.h>

#include "test.h"

#include "str.h"

char *ctbl[] = {
/* 1 */	"?",
/* 2 */	"q_uit",
/* 3 */	"s_leep",
/* 4 */	"so_urce",
/* 5 */	"k_rbname",
/* 6 */	"po_rt",
/* 7 */	"h_ost",
/* 8 */	"t_est",
/* 9 */	"f_ib",
0};

int gettestconn();
int settestlevel();;
int settestkrbname();;
int settesthost();;
int settestport();;
int settestsecurityindex();

void do_test(char *cp, char *word)
{
    int n;
    int code;
    char *ep;
    struct rx_connection *conn;

    while (*cp && isspace(*cp)) ++cp;
    if (!*cp) {
Usage:
fprintf (stderr,"Usage: test N\n");
	return;
    }
    n = strtol(cp, &ep, 0);
    if (cp == ep) goto Usage;
    cp = ep;
    while (*cp && isspace(*cp)) ++cp;
    if (*cp) goto Usage;
    if ((code = gettestconn(&conn))) {
	fprintf(stderr,"Error getting connection, %d %s\n",
	    code,
	    afs_error_message(code));
	return;
    }
    code = TEST_test(conn, n);
    if (code) {
	fprintf(stderr,"TEST_test failed - code %d %s\n",
	    code,
	    afs_error_message(code));
	return;
    }
}

void do_fib(char *cp, char *word)
{
    int n;
    int code;
    char *ep;
    struct rx_connection *conn;
    char *name;
    fib_results output[1];
    int i;

    while (*cp && isspace(*cp)) ++cp;
    if (!*cp) {
Usage:
fprintf (stderr,"Usage: fib N\n");
	return;
    }
    n = strtol(cp, &ep, 0);
    if (cp == ep) goto Usage;
    cp = ep;
    while (*cp && isspace(*cp)) ++cp;
    if (*cp) goto Usage;
    if ((code = gettestconn(&conn))) {
	fprintf(stderr,"Error getting connection, %d %s\n",
	    code,
	    afs_error_message(code));
	return;
    }
    name = 0;
    memset(output, 0, sizeof *output);
    code = TEST_fib(conn, n, &name, output);
    if (code) {
	fprintf(stderr,"TEST_fib failed - code %d %s\n",
	    code,
	    afs_error_message(code));
	return;
    }
    printf ("name <%s>\n%d:", name, output->fib_results_len);
    for (i = 0; i < output->fib_results_len; ++i)
	printf (" %u", output->fib_results_val[i]);
    printf ("\n");
    free(output->fib_results_val);
    free(name);
}

void do_source(char *cp, char *word)
{
    char line[512];
    FILE *fd;
    void process();

    while (*cp && isspace(*cp)) ++cp;
    fd = fopen(cp, "r");
    if (!fd) {
	return;
    }
    while (fgets(line, sizeof(line), fd)) {
	stripnl(line);
	printf ("> %s\n", line);
	process(line);
    }
}

void process(char *buffer)
{
    int code;
    char word[512];
    char id[512];
    char *cp;

    cp = getword(buffer, word);
    if (!*word || *word == '#') return;
    switch(code = kwscan(word, ctbl)) {
    case 0:
	fprintf(stderr,"<%s> not understood - try ?\n",
	    word);
	break;
    case 1:
	puts("Commands:\n");
	for (code = 0; ctbl[code]; ++code)
	    puts(ctbl[code]);
	puts("");
	break;
    case 2:
	exit(0);
    case 3:
	cp = getword(cp, id);
	if (!*id || (code = atoi(id)) <= 0) {
	    fprintf(stderr,"Bad delay <%s>\n", id);
	    break;
	}
#ifdef AFS_PTHREAD_ENV
	sleep(code);
#else
	IOMGR_Sleep(code);
#endif
	break;
    case 4:
	do_source(cp, word);
	break;
    case 5:
	settestkrbname(cp, word);
	break;
    case 6:
	settestport(cp, word);
	break;
    case 7:
	settesthost(cp, word);
	break;
    case 8:
	do_test(cp, word);
	break;
    case 9:
	do_fib(cp, word);
	break;
    }
}

int
main(int argc, char **argv)
{
    char buffer[512];
    int interactive;
    char *argp;

    while (--argc > 0) if (*(argp = *++argv)=='-')
    while (*++argp) switch(*argp) {
    case 'h':
	if (argc <= 1) goto Usage;
	--argc;
	settesthost(*++argv, buffer);
	break;
    case 'p':
	if (argc <= 1) goto Usage;
	--argc;
	settestport(*++argv, buffer);
	break;
    case 'k':
	if (argc <= 1) goto Usage;
	--argc;
	settestkrbname(*++argv, buffer);
	break;
    case 'A':
	if (argc <= 1) goto Usage;
	--argc;
	settestlevel(*++argv, buffer);
	break;
    case 'B':
	if (argc <= 1) goto Usage;
	--argc;
	settestsecurityindex(*++argv, buffer);
	break;
    case '-':
	break;
    default:
	fprintf (stderr,"Bad switch char <%c>\n", *argp);
    Usage:
	fprintf(stderr, "Usage: testclient [-h host] [-p port] [-k krbname] [-A level] [-B si]\n");
	exit(1);
    }
    else goto Usage;

    rx_Init(0);

    interactive = isatty(0);
    while (interactive ? write(2,"t>",2) : 0,fgets(buffer, sizeof buffer, stdin)) {
	stripnl(buffer);
	process(buffer);
    }
    return 0;
}
