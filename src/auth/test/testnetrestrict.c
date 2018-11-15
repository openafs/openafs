/*
 * testnetstrict.c
 *
 * Utility to test NetInfo and NetRestrict parsing
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/cellconfig.h>

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <arpa/inet.h>

char *interfaceList, *filenameNetInfo, *filenameNetRestrict;

/* Prototype for a deprecated function. */
int afsconf_ParseNetInfoFile(afs_uint32 outAddrs[], afs_uint32 outMask[], afs_uint32 outMtu[],
			 int max, char reason[], const char *fileName);

int
rx_getAllAddrMaskMtu(afs_uint32 addrBuffer[],
		     afs_uint32 maskBuffer[],
		     afs_uint32 mtuBuffer[],
		     int maxSize)
{
    FILE *fp;
    int nInterfaces = 0, lineNo = 1;
    int a[4], m[4], mtu;
    char line[80];

    fp = fopen(interfaceList, "r");
    if (fp == NULL) {
	perror("fopen");
	exit(1);
    }

    while (nInterfaces < maxSize) {
	++lineNo;
	if (fgets(line, 80, fp) == NULL)
		break;
	if (sscanf(line, "%d.%d.%d.%d %d.%d.%d.%d %d\n",
		  &a[0], &a[1], &a[2], &a[3],
		  &m[0], &m[1], &m[2], &m[3], &mtu) == 9) {

	    addrBuffer[nInterfaces] = htonl(a[0] << 24 | a[1] << 16
					    | a[2] << 8 | a[3]);
	    maskBuffer[nInterfaces] = htonl(m[0] << 24 | m[1] << 16
					    | m[2] << 8 | m[3]);
	    mtuBuffer[nInterfaces] = htonl(mtu);
	    ++nInterfaces;
	} else {
	    fprintf(stderr, "failed to parse line %d in %s\n", lineNo, interfaceList);
	}
    }
    fclose(fp);

    return nInterfaces;
}

#define MAXADDRS 16

int
main(int argc, char *argv[])
{
    afs_uint32 outAddrs[MAXADDRS], outMask[MAXADDRS], outMtu[MAXADDRS], nAddrs;
    char reason[1024];
    int i, retval;

    if (argc < 4) {
	fprintf(stderr, "usage: testnetrestrict <interface list> <netinfo> <netstrict>\n");
	exit(1);
    }

    interfaceList = argv[1];
    filenameNetInfo = argv[2];
    filenameNetRestrict = argv[3];

    reason[0] = '\0';
    retval = afsconf_ParseNetInfoFile(outAddrs, outMask, outMtu, MAXADDRS,
				      reason, filenameNetInfo);

    printf("afsconf_ParseNetInfoFile() returned %d\n", retval);
    if (reason[0] != '\0')
	printf("reason: %s\n", reason);

    printf("final address list:\n");
    if (retval > 0) {
	for(i = 0; i < retval; ++i) {
	    printf("\taddress 0x%x  mask 0x%x  mtu %d\n",
		   ntohl(outAddrs[i]), ntohl(outMask[i]), ntohl(outMtu[i]));
	}
    }

    reason[0] = '\0';
    retval = afsconf_ParseNetRestrictFile(outAddrs, outMask, outMtu, MAXADDRS,
					  &nAddrs, reason, filenameNetRestrict);

    printf("\nafsconf_ParseNetRestrictFile() returned %d\n", retval);
    if (reason[0] != '\0')
	printf("reason: %s\n", reason);
    if (nAddrs > 0) {
	printf("final address list:\n");
	for(i = 0; i < nAddrs; ++i) {
	    printf("\taddress 0x%x  mask 0x%x  mtu %d\n",
		   ntohl(outAddrs[i]), ntohl(outMask[i]), ntohl(outMtu[i]));
	}
    }

    reason[0] = '\0';
    retval = afsconf_ParseNetFiles(outAddrs, outMask, outMtu, MAXADDRS,
				   reason, filenameNetInfo, filenameNetRestrict);

    printf("\nafsconf_ParseNetFiles() returned %d\n", retval);
    if (reason[0] != '\0')
	printf("reason: %s\n", reason);
    if (retval > 0) {
	printf("final address list:\n");
	for(i = 0; i < retval; ++i) {
	    printf("\taddress 0x%x  mask 0x%x  mtu %d\n",
		   ntohl(outAddrs[i]), ntohl(outMask[i]), ntohl(outMtu[i]));
	}
    }

    exit(0);
}
