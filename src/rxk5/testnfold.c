/*
 *	testnfold.c
 *
 * Copyright (c) 2003,2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the university of
 * michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  if the
 * above copyright notice or any other identification of the
 * university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * this software is provided as is, without representation
 * from the university of michigan as to its fitness for any
 * purpose, and without warranty by the university of
 * michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose. the
 * regents of the university of michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include "afsconfig.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>

#ifdef USING_K5SSL
#include <sys/types.h>
#include "k5ssl.h"
#endif
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#include <krb5.h>
#endif

#ifdef HAVE_CK_H
#define rxk5i_nfold(a,b,c,d)	krb5i_nfold(a,b,c,d)
#else
int rxk5i_nfold();
#endif
int bin_dump();

struct testcase {
    char *in;
    int inlen;
    char *out;
    int outlen;
} tc[] = {
    /* RFC 3961 */

    { "012345", 6,
    "\xbe\x07\x26\x31\x27\x6b\x19\x55", 8 },

    { "password", 8,
    "\x78\xa0\x7b\x6c\xaf\x85\xfa", 7 },

    { "Rough Consensus, and Running Code", 33,
    "\xbb\x6e\xd3\x08\x70\xb7\xf0\xe0", 8 },

    { "password", 8,
"\x59\xe4\xa8\xca\x7c\x03\x85\xc3\xc3\x7b\x3f\x6d\x20\x00\x24\x7c\xb6\xe6\xbd\x5b\x3e", 168/8 },

    { "MASSACHVSETTS INSTITVTE OF TECHNOLOGY", 37,
"\xdb\x3b\x0d\x8f\x0b\x06\x1e\x60\x32\x82\xb3\x08\xa5\x08\x41\x22\x9a\xd7\x98\xfa\xb9\x54\x0c\x1b", 24 },
    { "Q",1,
"\x51\x8a\x54\xa2\x15\xa8\x45\x2a\x51\x8a\x54\xa2\x15\xa8\x45\x2a\x51\x8a\x54\xa2\x15", 21 },
    { "ba",2,
"\xfb\x25\xd5\x31\xae\x89\x74\x49\x9f\x52\xfd\x92\xea\x98\x57\xc4\xba\x24\xcf\x29\x7e", 21 },
    { "kerberos",8,
    "\x6b\x65\x72\x62\x65\x72\x6f\x73", 8 },
    { "kerberos",8,
    "\x6b\x65\x72\x62\x65\x72\x6f\x73\x7b\x9b\x5b\x2b\x93\x13\x2b\x93",16 },

    { "kerberos",8,
"\x83\x72\xc2\x36\x34\x4e\x5f\x15\x50\xcd\x07\x47\xe1\x5d\x62\xca\x7a\x5a\x3b\xce\xa4", 21 },
    { "kerberos",8,
"\x6b\x65\x72\x62\x65\x72\x6f\x73\x7b\x9b\x5b\x2b\x93\x13\x2b\x93\x5c\x9b\xdc\xda\xd9\x5c\x98\x99\xc4\xca\xe4\xde\xe6\xd6\xca\xe4",32 },

    { "\xe4\xca\xd6\xe6\xde\xe4\xca\xc4", 8,
    "\x29\x1e\x3a\x1e\x5b\xbf\x57\x23\xad\x8a\x55\x5e\x63", 13 },

    {"\xf0\x8c\x0a\x46\x44\xb4\x35\xf0\xe0\x5b\x7e\x77\xe2", 13,
    "\x88\xf2\x53\x7b\xf0\x84\x00", 7 },

    {"iU", 2,
    "\x3b\xab\xe0\x60\x00\x00\x00", 7 },
{0}};

int
main(int argc, char **argv)
{
    int r;
    struct testcase *t;
    char buffer[512];
    r = 0;
    for (t = tc; t->in; ++t) {
	printf ("In:\n");
	bin_dump(t->in, t->inlen);
	rxk5i_nfold(t->in, t->inlen, buffer, t->outlen);
	if (memcmp(buffer, t->out, t->outlen)) {
	    r = 1;
	    printf ("**** EXPECTED\n");
	    printf ("Out:\n");
	    bin_dump(t->out, t->outlen);
	    printf ("**** GOT\n");
	}
	printf ("Out:\n");
	bin_dump(buffer, t->outlen);
    }
    if (r) printf ("**** FAILED\n");
    exit(r);
}
