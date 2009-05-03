/*
 * Copyright (c) 2007
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
 * t_cap.c
 *
 *	test PGetProperties
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <afs/stds.h>
#include <netinet/in.h>
#include <afs/venus.h>

#define PIOCTL lpioctl

int dflag;

int
bin_dump(char *cp, int s)
{
    char *buffer;
    char c;
    int w;
    int i;
    long o;

    o = 0;
    buffer = cp;
    while (s > 0) {
	c = 16;
	if (c > s) c = s;
	printf ("%06lx:", o);
	w = 0;
	for (i = 0; i < c/2; ++i)
	    w += 5, printf (" %4x", htons(((unsigned short *)buffer)[i]));
	if (c & 1)
	    w += 3, printf (" %2x", (unsigned char)buffer[c-1]);
	while (w < 41)
	    ++w, putchar(' ');
	for (i = 0; i < c; ++i)
	    if (buffer[i] >= ' ' && buffer[i] < 0x7f)
/* isprint(buffer[i]) */
		putchar(buffer[i]);
	    else
		putchar('.');
	putchar('\n');
	o += c;
	buffer += c;
	s -= c;
    }
    printf ("%06lx:\n", o);
    return 1;
}

void
process(char *fn, int len)
{
    unsigned char buffer[1024]; /* was 16384 */
    int code;
    struct ViceIoctl blob[1];
    int i;
    char *cp;

    memset(buffer, 0xaa, sizeof buffer);

    blob->in_size = len;
    blob->in = fn;
    blob->out_size = sizeof buffer;
    blob->out = buffer;
    code = PIOCTL(0, VIOCGETPROP, &blob, 1);
    if (code) {
	afs_com_err("t_cap", errno, "PGetProperty failed");
	return;
    }
    if (dflag) {
	for (cp = fn; cp < fn+len; cp += strlen(cp)+1)
	    printf (" %s"+(cp==fn), cp);
	printf (":\n");
	for (i = blob->out_size; i--; )
	    if (buffer[i] != 0xaa) {
		++i;
		break;
	    }
	bin_dump(buffer, i);
    } else {
	for (i = 0; buffer[i]; ) {
	    printf ("%s: ", buffer+i);
	    i += strlen(buffer+i)+1;
	    printf ("%s\n", buffer+i);
	    i += strlen(buffer+i)+1;
	}
    }
    return;
}

main(int argc, char **argv)
{
    int f;
    char *argp;
    int i, l;
    struct qstr {
	struct qstr *next;
	char name[1];
    } *qhead = 0, *qp, **qnext = &qhead;

    f = 0;
    while (--argc > 0) if (*(argp = *++argv)=='-')
    while (*++argp) switch(*argp) {
#if 0
    case 'o':
	if (argc <= 1) goto Usage;
	--argc;
	offset = atol(*++argv);
	break;
#endif
    case 'd':
	++dflag;
	break;
    case '-':
	break;
    default:
	fprintf (stderr,"Bad switch char <%c>\n", *argp);
    Usage:
	fprintf(stderr, "Usage: t_cap [-d] property...\n");
	exit(1);
    }
    else switch(f++) {
    default:
	l = strlen(argp) + 1;
	qp = malloc(sizeof *qp + l);
	qp->next = 0;
	memcpy(qp->name, argp, l);
	*qnext = qp;
	qnext = &qp->next;
    }
    if (!qhead) {
	argp = strdup("*");
	i = 2;
    }
    else {
	i = 0;
	for (qp = qhead; qp; qp = qp->next) {
	    i += strlen(qp->name) + 1;
	}
	argp = malloc(i);
	i = 0;
	for (qp = qhead; qp; qp = qp->next) {
	    l = strlen(qp->name) + 1;
	    memcpy(argp + i, qp->name, l);
	    i += l;
	}
    }
    process(argp, i);
    free(argp);
    exit(0);
}
