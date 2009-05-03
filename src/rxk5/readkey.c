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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int
read_service_key(char *service,
    char *instance,
    char *realm,
    int kvno,
    char *file,
    char *key)
{
    int inst, rlm, ky;
    unsigned char vno;
    int fd;
    char *cp;
    char *hold;
    int state;
    int len;
    int i;
    int resid;
    char buffer[512];

    memset(buffer, 0, sizeof buffer);
    if ((fd = open(file, 0)) < 0) {
#if 0
	elcprintf("Failed to open <%s> - %d\n", file, errno);
#endif
	return 1;
    }
/* 3 strings, a byte, and 8 bytes */
    state = 0;
    resid = 0;
    while ((len = read(fd, cp = buffer+resid, sizeof(buffer)-resid))  > 0) {
#if 0
dprintf(DF('k'), "In %d=<%s>, had %d bytes, read %d bytes\n", fd, file, resid, len);
#endif
	len += resid;
	while (cp < buffer + len)
	    switch(state) {
	    case 0:
		hold = cp;
		goto String;
	    case 1:
		goto String2;
	    case 2:
		inst = cp - hold;
		goto String;
	    case 3:
		goto String2;
	    case 4:
		rlm = cp - hold;
		goto String;
	    case 5:
		    goto String2;
	    String:
		    ++state;
	    String2:
		while (cp < buffer+len)
		    if (!*cp++) {
			++state;
			break;
		    }
		break;
	    case 6:
		vno = *cp++;
		++state;
		break;
	    case 7:
		ky = cp - hold;
		++state;
		break;
	    case 8:
		i = len - (cp - buffer);
		if (i < 8) {
		    cp += i;
		    break;
		}
		cp += 8;
		++state;
	    case 9:
		state = 0;
#if 0
dprintf(DF('k'), "Got <%s.%s@%s> in <%s> kvno %d\n",
hold, hold + inst, hold + rlm, file, vno);
#endif
		if (strcmp(hold + rlm, realm))
		    break;
		if (strcmp(hold, service))
		    break;
		if (kvno && kvno != vno)
		    break;
		if (*instance != '*' || instance[1]) {
		    if (strcmp(hold + inst, instance))
			break;
		} else strcpy(instance, hold + inst);
		close(fd);
		memcpy(key, hold+ky, 8);
		memset(buffer, 0, sizeof buffer);
		return 0;
	    }
	if (state)
	    resid = cp - hold;
	else
	    resid = 0;
	if (resid)
	    memcpy(buffer, hold, resid);
    }
    close(fd);
#if 0
    elcprintf ("junk in srvtab <%s> - EOF while looking for %s of %s.%s@%s kvno %d\n",
	file,
	state < 2 ? "name" :
	state < 4 ? "instance" :
	state < 6 ? "realm" :
	state == 6 ? "vno" :
	state <9 ? "key" :
	"mother", service, instance, realm, kvno);
#endif
    memset(buffer, 0, sizeof buffer);
    return 1;	/* XXX need better error code */
}
