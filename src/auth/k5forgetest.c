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

#include <stdio.h>
#include <stdlib.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <string.h>

#include "rxk5_utilafs.h"

int main(int argc, char **argv)
{
    int code;
    char keytab[512];
    krb5_context k5context;
    krb5_creds *k5creds;
    struct stat st;
    int stop_here;
    
    int allowed_enctypes[6] = { 
                                ENCTYPE_AES256_CTS_HMAC_SHA1_96,
				ENCTYPE_AES128_CTS_HMAC_SHA1_96,
				ENCTYPE_DES3_CBC_SHA1,
#ifndef USING_HEIMDAL
#define ENCTYPE_ARCFOUR_HMAC_MD5 ENCTYPE_ARCFOUR_HMAC
#define ENCTYPE_ARCFOUR_HMAC_MD5_56 ENCTYPE_ARCFOUR_HMAC_EXP
#endif
				ENCTYPE_ARCFOUR_HMAC_MD5,
				ENCTYPE_ARCFOUR_HMAC_MD5_56,
				ENCTYPE_DES_CBC_CRC };
    
    memset(keytab, 0, 512);
    strcpy(keytab, "/usr/local/etc/openafs/server/afs.keytab"); 
    
    code = stat(keytab, &st);
    if((code != 0) || (!S_ISREG(st.st_mode))) {
    	printf("Can't stat keytab %s\n", keytab);
	exit(1);
    }
    
    code = krb5_init_context(&k5context);
    if(code) {
	printf("Error krb5_init_context\n");
	exit(2);
    }

    code = afs_rxk5_k5forge(
			    k5context, 
			    keytab, "afs-k5@MONKIUS.COM", 
			    "afs-k5@MONKIUS.COM", 
			    time(NULL),
			    time(NULL),
			    allowed_enctypes, 
			    0 /* paddress */, 
			    &k5creds /* out */);

    krb5_free_creds(k5context, k5creds);
    krb5_free_context(k5context);
  
    return 0;
}
