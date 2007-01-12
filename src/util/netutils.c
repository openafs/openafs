/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Network utility functions
 * Parsing NetRestrict file and filtering IP addresses
 */

#include <afsconfig.h>
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <ctype.h>
#ifdef KERNEL
#include "afs/sysincludes.h"
#include "afsincludes.h"
#else
#ifdef __FreeBSD__
#include <sys/types.h>
#endif
#include <netinet/in.h>
#include <errno.h>
#include <sys/errno.h>
#endif

#include "assert.h"
#include "afsutil.h"
#include "dirpath.h"

#define AFS_IPINVALID        0xffffffff	/* invalid IP address */
#define AFS_IPINVALIDIGNORE  0xfffffffe	/* no input given to extractAddr */
#define MAX_NETFILE_LINE       2048	/* length of a line in the netrestrict file */
#define MAXIPADDRS             1024	/* from afsd.c */

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK (afs_uint32)0x7f000001
#endif

/* 
 * The line parameter is a pointer to a buffer containing a string of 
 * bytes of the form 
** w.x.y.z 	# machineName
 * returns the network interface IP Address in NBO
 */
afs_uint32
extract_Addr(char *line, int maxSize)
{
    char bytes[4][32];
    int i = 0, n = 0;
    char *endPtr;
    afs_uint32 val[4];
    afs_uint32 retval = 0;

    /* skip empty spaces */
    while (isspace(*line) && maxSize) {
	line++;
	maxSize--;
    }
    /* skip empty lines */
    if (!maxSize || !*line)
	return AFS_IPINVALIDIGNORE;

    for (n = 0; n < 4; n++) {
	while ((*line != '.') && !isspace(*line) && maxSize) {	/* extract nth byte */
	    if (!isdigit(*line))
		return AFS_IPINVALID;
	    if (i > 31)
		return AFS_IPINVALID;	/* no space */
	    bytes[n][i++] = *line++;
	    maxSize--;
	}			/* while */
	if (!maxSize)
	    return AFS_IPINVALID;
	bytes[n][i] = 0;
	i = 0, line++;
	errno = 0;
	val[n] = strtol(bytes[n], &endPtr, 10);
	if ((val[n] == 0) && (errno != 0 || bytes[n] == endPtr))	/* no conversion */
	    return AFS_IPINVALID;
    }				/* for */

    retval = (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
    return htonl(retval);
}




/* parseNetRestrictFile()
 * Get a list of IP addresses for this host removing any address found
 * in the config file (fileName parameter): /usr/vice/etc/NetRestrict
 * for clients and /usr/afs/local/NetRestrict for servers.  
 *
 * Returns the number of valid addresses in outAddrs[] and count in
 * nAddrs.  Returns 0 on success; or 1 if the config file was not
 * there or empty (we still return the host's IP addresses). Returns
 * -1 on fatal failure with reason in the reason argument (so the
 * caller can choose to ignore the entire file but should write
 * something to a log file).
 *
 * All addresses should be in NBO (as returned by rx_getAllAddr() and
 * parsed by extract_Addr().
 */
/*
  afs_uint32  outAddrs[];          * output address array *
  afs_uint32  *mask, *mtu;         * optional mask and mtu *
  afs_uint32 maxAddrs;	   	   * max number of addresses *
  afs_uint32 *nAddrs;              * number of Addresses in output array *
  char       reason[];             * reason for failure *
  const char *fileName;            * filename to parse *
*/

int
parseNetRestrictFile_int(afs_uint32 outAddrs[], afs_uint32 * mask,
			 afs_uint32 * mtu, afs_uint32 maxAddrs,
			 afs_uint32 * nAddrs, char reason[], 
			 const char *fileName, const char *fileName_ni)
{
    FILE *fp;
    char line[MAX_NETFILE_LINE];
    int lineNo, usedfile = 0;
    afs_uint32 i, neaddrs, nOutaddrs;
    afs_uint32 addr, eAddrs[MAXIPADDRS], eMask[MAXIPADDRS], eMtu[MAXIPADDRS];

    assert(outAddrs);
    assert(reason);
    assert(fileName);
    assert(nAddrs);
    if (mask)
	assert(mtu);

    /* Initialize */
    *nAddrs = 0;
    for (i = 0; i < maxAddrs; i++)
	outAddrs[i] = 0;
    strcpy(reason, "");

    /* get all network interfaces from the kernel */
    neaddrs = rxi_getAllAddrMaskMtu(eAddrs, eMask, eMtu, MAXIPADDRS);
    if (neaddrs <= 0) {
	sprintf(reason, "No existing IP interfaces found");
	return -1;
    }
    i = 0;
    if ((neaddrs < MAXIPADDRS) && fileName_ni) 
	i = ParseNetInfoFile_int(&(eAddrs[neaddrs]), &(eMask[neaddrs]), 
				 &(eMtu[neaddrs]), MAXIPADDRS-neaddrs, reason,
				 fileName_ni, 1);

    if (i > 0)
	neaddrs += i;

    if ((fp = fopen(fileName, "r")) == 0) {
	sprintf(reason, "Could not open file %s for reading:%s", fileName,
		strerror(errno));
	goto done;
    }

    /* For each line in the NetRestrict file */
    lineNo = 0;
    usedfile = 0;
    while (fgets(line, MAX_NETFILE_LINE, fp) != NULL) {
	lineNo++;		/* input line number */
	addr = extract_Addr(line, strlen(line));
	if (addr == AFS_IPINVALID) {	/* syntactically invalid */
	    fprintf(stderr, "%s : line %d : parse error - invalid IP\n",
		    fileName, lineNo);
	    continue;
	}
	if (addr == AFS_IPINVALIDIGNORE) {	/* ignore error */
	    fprintf(stderr, "%s : line %d : invalid address ... ignoring\n",
		    fileName, lineNo);
	    continue;
	}
	usedfile = 1;

	/* Check if we need to exclude this address */
	for (i = 0; i < neaddrs; i++) {
	    if (eAddrs[i] && (eAddrs[i] == addr)) {
		eAddrs[i] = 0;	/* Yes - exclude it by zeroing it for now */
	    }
	}
    }				/* while */

    fclose(fp);

    if (!usedfile) {
	sprintf(reason, "No valid IP addresses in %s\n", fileName);
	goto done;
    }

  done:
    /* Collect the addresses we have left to return */
    nOutaddrs = 0;
    for (i = 0; i < neaddrs; i++) {
	if (!eAddrs[i])
	    continue;
	outAddrs[nOutaddrs] = eAddrs[i];
	if (mask) {
	    mask[nOutaddrs] = eMask[i];
	    mtu[nOutaddrs] = eMtu[i];
	}
	if (++nOutaddrs >= maxAddrs)
	    break;
    }
    if (nOutaddrs == 0) {
	sprintf(reason, "No addresses to use after parsing %s", fileName);
	return -1;
    }
    *nAddrs = nOutaddrs;
    return (usedfile ? 0 : 1);	/* 0=>used the file.  1=>didn't use file */
}

int
parseNetRestrictFile(afs_uint32 outAddrs[], afs_uint32 * mask,
			 afs_uint32 * mtu, afs_uint32 maxAddrs,
			 afs_uint32 * nAddrs, char reason[], 
			 const char *fileName)
{
    return parseNetRestrictFile_int(outAddrs, mask, mtu, maxAddrs, nAddrs, reason, fileName, NULL);
}

/*
 * this function reads in stuff from InterfaceAddr file in
 * /usr/vice/etc ( if it exists ) and verifies the addresses
 * specified. 
 * 'final' contains all those addresses that are found to 
 * be valid. This function returns the number of valid
 * interface addresses. Pulled out from afsd.c
 */
int
ParseNetInfoFile_int(afs_uint32 * final, afs_uint32 * mask, afs_uint32 * mtu,
		     int max, char reason[], const char *fileName, 
		     int fakeonly)
{

    afs_uint32 existingAddr[MAXIPADDRS], existingMask[MAXIPADDRS],
	existingMtu[MAXIPADDRS];
    char line[MAX_NETFILE_LINE];
    FILE *fp;
    int i, existNu, count = 0;
    afs_uint32 addr;
    int lineNo = 0;
    int l;

    assert(fileName);
    assert(final);
    assert(mask);
    assert(mtu);
    assert(reason);

    /* get all network interfaces from the kernel */
    existNu =
	rxi_getAllAddrMaskMtu(existingAddr, existingMask, existingMtu,
			      MAXIPADDRS);
    if (existNu < 0)
	return existNu;

    if ((fp = fopen(fileName, "r")) == 0) {
	/* If file does not exist or is not readable, then
	 * use all interface addresses.
	 */
	sprintf(reason,
		"Failed to open %s(%s)\nUsing all configured addresses\n",
		fileName, strerror(errno));
	for (i = 0; i < existNu; i++) {
	    final[i] = existingAddr[i];
	    mask[i] = existingMask[i];
	    mtu[i] = existingMtu[i];
	}
	return existNu;
    }

    /* For each line in the NetInfo file */
    while (fgets(line, MAX_NETFILE_LINE, fp) != NULL) {
	int fake = 0;

	/* See if first char is an 'F' for fake */
	/* Added to allow the fileserver to advertise fake IPS for use with
	 * the translation tables for NAT-like firewalls - defect 12462 */
	for (fake = 0; ((fake < strlen(line)) && isspace(line[fake]));
	     fake++);
	if ((fake < strlen(line))
	    && ((line[fake] == 'f') || (line[fake] == 'F'))) {
	    fake++;
	} else {
	    fake = 0;
	}

	lineNo++;		/* input line number */
	addr = extract_Addr(&line[fake], strlen(&line[fake]));

	if (addr == AFS_IPINVALID) {	/* syntactically invalid */
	    fprintf(stderr, "afs:%s : line %d : parse error\n", fileName,
		    lineNo);
	    continue;
	}
	if (addr == AFS_IPINVALIDIGNORE) {	/* ignore error */
	    continue;
	}

	/* See if it is an address that really exists */
	for (i = 0; i < existNu; i++) {
	    if (existingAddr[i] == addr)
		break;
	}
	if ((i >= existNu) && (!fake))
	    continue;		/* not found/fake - ignore */

	/* Check if it is a duplicate address we alread have */
	for (l = 0; l < count; l++) {
	    if (final[l] == addr)
		break;
	}
	if (l < count) {
	    fprintf(stderr, "afs:%x specified twice in NetInfo file\n",
		    ntohl(addr));
	    continue;		/* duplicate addr - ignore */
	}

	if (count > max) {	/* no more space */
	    fprintf(stderr,
		    "afs:Too many interfaces. The current kernel configuration supports a maximum of %d interfaces\n",
		    max);
	} else if (fake) {
	    if (!fake) 
		fprintf(stderr, "Client (2) also has address %s\n", line);
	    final[count] = addr;
	    mask[count] = 0xffffffff;
	    mtu[count] = htonl(1500);
	    count++;
	} else if (!fakeonly) {
	    final[count] = existingAddr[i];
	    mask[count] = existingMask[i];
	    mtu[count] = existingMtu[i];
	    count++;
	}
    }				/* while */

    /* in case of any error, we use all the interfaces present */
    if (count <= 0) {
	sprintf(reason,
		"Error in reading/parsing Interface file\nUsing all configured interface addresses \n");
	for (i = 0; i < existNu; i++) {
	    final[i] = existingAddr[i];
	    mask[i] = existingMask[i];
	    mtu[i] = existingMtu[i];
	}
	return existNu;
    }
    return count;
}

int
ParseNetInfoFile(afs_uint32 * final, afs_uint32 * mask, afs_uint32 * mtu,
		 int max, char reason[], const char *fileName)
{
    return ParseNetInfoFile_int(final, mask, mtu, max, reason, fileName, 0);
}

/*
 * Given two arrays of addresses, masks and mtus find the common ones
 * and return them in the first buffer. Return number of common
 * entries.
 */
int
filterAddrs(afs_uint32 addr1[], afs_uint32 addr2[], afs_uint32 mask1[],
	    afs_uint32 mask2[], afs_uint32 mtu1[], afs_uint32 mtu2[], int n1,
	    int n2)
{
    afs_uint32 taddr[MAXIPADDRS];
    afs_uint32 tmask[MAXIPADDRS];
    afs_uint32 tmtu[MAXIPADDRS];
    int count = 0, i = 0, j = 0, found = 0;

    assert(addr1);
    assert(addr2);
    assert(mask1);
    assert(mask2);
    assert(mtu1);
    assert(mtu2);

    for (i = 0; i < n1; i++) {
	found = 0;
	for (j = 0; j < n2; j++) {
	    if (addr1[i] == addr2[j]) {
		found = 1;
		break;
	    }
	}

	/* Always mask loopback address */
	if (found && addr1[i] == INADDR_LOOPBACK) 
	    found = 0;

	if (found) {
	    taddr[count] = addr1[i];
	    tmask[count] = mask1[i];
	    tmtu[count] = mtu1[i];
	    count++;
	}
    }
    /* copy everything into addr1, mask1 and mtu1 */
    for (i = 0; i < count; i++) {
	addr1[i] = taddr[i];
	if (mask1) {
	    mask1[i] = tmask[i];
	    mtu1[i] = tmtu[i];
	}
    }
    /* and zero out the rest */
    for (i = count; i < n1; i++) {
	addr1[i] = 0;
	if (mask1) {
	    mask1[i] = 0;
	    mtu1[i] = 0;
	}
    }
    return count;
}

/*
 * parse both netinfo and netrerstrict files and return the final
 * set of IP addresses to use
 */
/* max - Entries in addrbuf, maskbuf and mtubuf */
int
parseNetFiles(afs_uint32 addrbuf[], afs_uint32 maskbuf[], afs_uint32 mtubuf[],
	      afs_uint32 max, char reason[], const char *niFileName,
	      const char *nrFileName)
{
    afs_uint32 addrbuf1[MAXIPADDRS], maskbuf1[MAXIPADDRS],
	mtubuf1[MAXIPADDRS];
    afs_uint32 addrbuf2[MAXIPADDRS], maskbuf2[MAXIPADDRS],
	mtubuf2[MAXIPADDRS];
    int nAddrs1 = 0;
    afs_uint32 nAddrs2 = 0;
    int code, i;

    nAddrs1 =
	ParseNetInfoFile(addrbuf1, maskbuf1, mtubuf1, MAXIPADDRS, reason,
			 niFileName);
    code =
	parseNetRestrictFile_int(addrbuf2, maskbuf2, mtubuf2, MAXIPADDRS,
			     &nAddrs2, reason, nrFileName, niFileName);
    if ((nAddrs1 < 0) && (code)) {
	/* both failed */
	return -1;
    } else if ((nAddrs1 > 0) && (code)) {
	/* netinfo succeeded and netrestrict failed */
	for (i = 0; ((i < nAddrs1) && (i < max)); i++) {
	    addrbuf[i] = addrbuf1[i];
	    if (maskbuf) {
		maskbuf[i] = maskbuf1[i];
		mtubuf[i] = mtubuf1[i];
	    }
	}
	return i;
    } else if ((!code) && (nAddrs1 < 0)) {
	/* netrestrict succeeded and netinfo failed */
	for (i = 0; ((i < nAddrs2) && (i < max)); i++) {
	    addrbuf[i] = addrbuf2[i];
	    if (maskbuf) {
		maskbuf[i] = maskbuf2[i];
		mtubuf[i] = mtubuf2[i];
	    }
	}
	return i;
    } else if ((!code) && (nAddrs1 >= 0)) {
	/* both succeeded */
	/* take the intersection of addrbuf1 and addrbuf2 */
	code =
	    filterAddrs(addrbuf1, addrbuf2, maskbuf1, maskbuf2, mtubuf1,
			mtubuf2, nAddrs1, nAddrs2);
	for (i = 0; ((i < code) && (i < max)); i++) {
	    addrbuf[i] = addrbuf1[i];
	    if (maskbuf) {
		maskbuf[i] = maskbuf1[i];
		mtubuf[i] = mtubuf1[i];
	    }
	}
	return i;
    }
    return 0;
}
