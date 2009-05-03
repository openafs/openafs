/*
 * Copyright (c) 2006
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

#include <afsconfig.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <ifaddrs.h>
#include "k5ssl.h"

typedef struct termios TTYPARMS;
#define Gtty(tb)	tcgetattr(0,(tb))
#define Stty(tb)	tcsetattr(0,TCSANOW,(tb))
#define SLFLAGS(tb)	((tb)->c_lflag)

krb5_error_code
krb5_prompter_posix(krb5_context context, void *data,
const char *name, const char *banner, int num, krb5_prompt prompts[])
{
    struct iovec iov[4];
    int i, r, w, hide;
    static char nl[] = "\n";
    hide = 5;
    TTYPARMS norm[1], noecho[1];
    w = 0;
    if (name) {
	iov[w].iov_base = (void *) name;
	iov[w].iov_len = strlen(name);
	++w;
	iov[w].iov_base = nl;
	iov[w].iov_len = 1;
	++w;
    }
    if (banner) {
	iov[w].iov_base = (void *) banner;
	iov[w].iov_len = strlen(banner);
	++w;
	iov[w].iov_base = nl;
	iov[w].iov_len = 1;
	++w;
    }
    for (i = 0; i < num; ++i) {
	iov[w].iov_base = prompts[i].prompt;
	iov[w].iov_len = strlen(prompts[i].prompt);
	++w;
	iov[w].iov_base = ": ";
	iov[w].iov_len = 2;
	++w;
	if (hide == 5) {
	    Gtty(norm);
	    *noecho = *norm;
	    SLFLAGS(noecho) &= ~ECHO;
	    hide = 0;
	}
	if (hide != prompts[i].hidden) {
	    Stty(prompts[i].hidden ? noecho : norm);
	    hide = !!prompts[i].hidden;
	}
	writev(2, iov, w);
	w = 0;
	r = read(0, prompts[i].reply->data, prompts[i].reply->length);
	if (hide) {
	    iov[w].iov_base = "\n";
	    iov[w].iov_len = 1;
	    ++w;
	}
	if (r <= 0) {
	    if (w) writev(2, iov, w);
	    return 1;
	}
	if ((r-1)[prompts[i].reply->data] = '\n')
	    --r;
	r[prompts[i].reply->data] = 0;
	prompts[i].reply->length = r;
    }
    if (hide == 1) {
	Stty(norm);
    }
    if (w) writev(2, iov, w);
    return 0;
}

krb5_error_code
krb5_os_localaddr(krb5_context context,
    krb5_address ***addrs)
{
    int code;
    struct ifaddrs *ifap = 0, *ifp;
    int n;
    void *p;
    int t, s;
    krb5_address *a, **list, **pp, **pp2;
    if (getifaddrs(&ifap)) {
	code = errno;
	goto Done;
    }
    n = 1;
    for (ifp = ifap; ifp; ifp = ifp->ifa_next) {
	if ((ifp->ifa_flags & (IFF_UP|IFF_LOOPBACK)) != IFF_UP)
	    continue;
	++n;
    }
    code = ENOMEM;
    list = (krb5_address **) malloc(n * sizeof(*list));
    if (!list) goto Done;
    pp = list;
    for (ifp = ifap; ifp; ifp = ifp->ifa_next) {
	if ((ifp->ifa_flags & (IFF_UP|IFF_LOOPBACK)) != IFF_UP)
	    continue;
	t = s = 0;
	p = 0;
	switch(ifp->ifa_addr->sa_family) {
	case AF_INET:
	    t = AF_INET;
	    s = sizeof (struct in_addr);
	    p = &((struct sockaddr_in*)ifp->ifa_addr)->sin_addr;
	    break;
	default:
	    break;
	}
	if (!s) continue;
	for (pp2 = list; pp2 < pp; ++pp2) {
	    if ((*pp2)->addrtype == t && (*pp2)->length == s &&
		!memcmp((*pp2)->contents, p, s)) break;
	}
	if (pp2 >= pp) {
	    *pp++ = a = malloc(sizeof *a);
	    if (!a) goto Done;
	    a->contents = malloc(s);
	    if (!a->contents) goto Done;
	    a->addrtype = t;
	    a->length = s;
	    memcpy(a->contents, p, s);
	}
    }
    *pp = 0;
    *addrs = list;
    list = 0;
    code = 0;
Done:
    krb5_free_addresses(context, list);
    if (ifap) freeifaddrs(ifap);
    return code;
}
