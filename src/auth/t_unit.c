/*
 * Copyright (c) 2005, 2006
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "afs_token.h"
#include "rxk5_tkt.c"

#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strdup(p) strdup(p)

void pp(rxk5_principal *p) {
	int ix;
	for(ix = 0; ix < p->name.name_len; ++ix) {
		printf("%s\n", p->name.name_val[ix]); 
	}
}

void t_parse_rxk5_princ()
{
	int code;
	char *b_rapper;
	rxk5_principal k5_rapper;
    
	b_rapper = afs_strdup("its/vanilla/with/a/nine@ICE.COM");
	parse_rxk5_princ(b_rapper, &k5_rapper);
	pp(&k5_rapper);
	free_rxk5_princ(&k5_rapper);
	free(b_rapper);

	b_rapper = afs_strdup("@LINUXBOX.COM");
	parse_rxk5_princ(b_rapper, &k5_rapper);
	pp(&k5_rapper);
	free_rxk5_princ(&k5_rapper);
	free(b_rapper);

	b_rapper = afs_strdup("matt");
	parse_rxk5_princ(b_rapper, &k5_rapper);
	pp(&k5_rapper);
	free_rxk5_princ(&k5_rapper);
	free(b_rapper);
}

void t_afs_token()
{
	/* If user has a credential cached, use it as input to test token
	   logic */

	int code;
	krb5_creds *k5_creds = 0, in_creds[1];
	krb5_context k5context = 0;
	krb5_ccache cc = 0;
	char *afs_k5_princ = 0;

	afs_k5_princ = afs_strdup("afs-k5/monkius.com@MONKIUS.COM");
		
	code = krb5_init_context(&k5context);
	if(code) goto Failed;

	/* use cached credentials, if any */

	code = krb5_cc_default(k5context, &cc);
	if (code) goto Failed;

	code = krb5_cc_get_principal(k5context, cc, &in_creds->client);
	if (code) goto Failed;

	code = krb5_parse_name(k5context, afs_k5_princ, &in_creds->server);
	if (code) goto Failed;

	/* 0 is cc flags */
	code = krb5_get_credentials(k5context, 0, cc, in_creds, &k5_creds);
	if (code) goto Failed;

	/* fails with bad enctype, but AFS linked binaries don't */

	printf("Ready to make token\n");


Failed:
	printf("Code: %d\n", code);
	free(afs_k5_princ);
	return;
}

int main(int argc, char **argv)
{
	t_parse_rxk5_princ();
	t_afs_token();
out:
	return 0;
}

