/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
/* getifinfo.c retrives the IP configuration information for the machine.
 * NT stores this information in different places depending on the type
 * on interface.
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <winsock2.h>

#include <WINNT/syscfg.h>



#define MAXIPADDRS 16
int rxi_numNetAddrs;
int addrs[MAXIPADDRS];
int masks[MAXIPADDRS];
int mtus[MAXIPADDRS];
int flags[MAXIPADDRS];

main(int ac, char **av)
{
    int maxAddrs = 0;
    int i;
    char addrStr[32];
    struct in_addr ina;

    rxi_numNetAddrs = MAXIPADDRS;

    if ((maxAddrs = syscfg_GetIFInfo(&rxi_numNetAddrs,
				     addrs, masks, mtus, flags)) < 0) {
	printf("Failed reading interface information\n");
    } else {
	printf("Found %d useable addresses, max = %d\n",
	       rxi_numNetAddrs, maxAddrs);

	for (i=0; i<rxi_numNetAddrs; i++) {
	    ina.S_un.S_addr = htonl((unsigned long)addrs[i]);
	    (void) strcpy(addrStr, inet_ntoa(ina));
	    ina.S_un.S_addr = htonl((unsigned long)masks[i]);
	    printf("IP: 0x%x %s  MASK: 0x%x %s\n",
		   addrs[i], addrStr, masks[i],
		   inet_ntoa(ina));
	}
    }
    return 0;
}
