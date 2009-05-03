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
#include "k5s_tkt.h"

int main(int argc, char **argv)
{
    int code;
    krb5_creds* creds;
    krb5_context ctxt;
    char* princ, *inst;
    char* realm;
    int clen;
    int fd;
    struct stat st;
    FILE *credfile;
    void *credsbuf;

    printf("Starting Up\n");
	
    code = krb5_init_context(&ctxt);
	
    clen = 0;
    fd = open("creds.out", O_RDONLY);
    if(fd == -1) {
        printf("Can't open creds file\n");
	goto out;
    }
    credfile = fdopen(fd, "r");
    if(!credfile) {
	printf("Problem converting fd to FILE*\n");
	goto out;
    }
    code = fstat(fd, &st);
    if(code == -1) {
	printf("Can't stat creds.out\n");
	goto out;
    }
    credsbuf = malloc(st.st_size * sizeof(char));
    code = fread(credsbuf, 1, st.st_size, credfile);
    if(code != st.st_size) {
	printf("Failed reading %d bytes from creds.out\n", st.st_size);
    }
	
    creds = parse_afs_krb5_creds_buf(ctxt, credsbuf);
    free(credsbuf);

    princ = osi_Alloc((creds->client->data[0].length + 1)* sizeof(char));
    memcpy(princ, creds->client->data[0].data, creds->client->data[0].length);
    princ[creds->client->data[0].length] = 0;

    inst = osi_Alloc((creds->client->data[1].length + 1)* sizeof(char));
    memcpy(inst, creds->client->data[1].data, creds->client->data[1].length);
    inst[creds->client->data[1].length] = 0;    
    
    realm = osi_Alloc((creds->client->realm.length + 1) * sizeof(char));
    memcpy(realm, creds->client->realm.data, creds->client->realm.length);
    realm[creds->client->realm.length] = 0;

    afs_warn("PSetK5tokens sees (princ, (inst), realm): %s (%s) %s\n", princ, inst, realm);
    osi_Free(princ, creds->client->data[0].length + 1);
    osi_Free(inst, creds->client->data[1].length + 1);
    osi_Free(realm, creds->client->realm.length + 1);
	
    krb5_free_creds(ctxt, creds);

out:
	return 0;
}

