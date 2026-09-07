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
#include <afs/param.h>

#include <roken.h>
#include <ctype.h>

#include <afs/opr.h>

#include <rx/rx.h>
#include <afs/dirpath.h>

#include "cellconfig.h"

#define AFS_IPINVALID		 -1	/* invalid IP address */
#define AFS_IPINVALIDIGNORE	 -2	/* no input given to extractAddr */
#define MAX_NETFILE_LINE       2048	/* length of a line in the NetRestrict file */
#define MAXIPADDRS             1024	/* from afsd.c */

static int ParseNetInfoFile_int(afs_uint32 *, afs_uint32 *, afs_uint32 *,
                         int, char reason[], const char *,
                         int);

#ifdef HAVE_IPV6
/**
 * Parse a single NetRestrict-file line for an IPv6 literal address,
 * optionally followed by a "/N" prefix length (0-128), mirroring
 * extract_Addr()'s IPv4 "w.x.y.z[/n]" syntax.
 *
 * A bare address with no "/" is treated as an implicit /128 - an exact
 * match, identical to this function's original (pre-CIDR) behavior. This
 * is a hard backward-compatibility requirement: existing NetRestrict files
 * written before CIDR support was added (one real address per line, no
 * "/" anywhere) must keep meaning exactly what they always meant.
 *
 * Still much narrower than extract_Addr()'s IPv4 handling in one respect:
 * no interface-exclusion semantics, no NetInfo-style merge - a v6
 * NetRestrict line only ever adds an inclusion (exact address or prefix)
 * to register, it never excludes an existing interface address the way
 * the v4 NetRestrict/NetInfo machinery does. That larger feature remains
 * out of scope here, exactly as before.
 *
 * @param[in] line
 *     Pointer to a string of bytes, same convention as extract_Addr()
 * @param[in] maxSize
 *     Length to search in line for an address
 * @param[out] addr
 *     The parsed IPv6 address, on success
 * @param[out] prefixlen
 *     The prefix length, on success: 128 if no "/N" suffix was present
 *     (an exact-match address), otherwise the parsed 0-128 value
 *
 * @return
 *      @retval 0 success
 *      @retval AFS_IPINVALID the token is not a valid IPv6 literal, or its
 *          "/N" suffix is malformed or out of the 0-128 range - the same
 *          rejection AFS_IPINVALID already means for a malformed address
 *      @retval AFS_IPINVALIDIGNORE blank line that can be ignored
 */
static int
extract_Addr6(char *line, int maxSize, struct in6_addr *addr,
	      afs_uint32 *prefixlen)
{
    char token[128];
    char *slash;
    long plen;
    int i = 0;

    /* skip empty spaces, same convention as extract_Addr() */
    while (isspace((unsigned char)*line) && maxSize) {
	line++;
	maxSize--;
    }
    /* skip empty lines */
    if (!maxSize || !*line)
	return AFS_IPINVALIDIGNORE;

    while (maxSize && *line && !isspace((unsigned char)*line)) {
	if (i > (int)sizeof(token) - 2)
	    return AFS_IPINVALID;	/* far too long for a real v6 literal */
	token[i++] = *line++;
	maxSize--;
    }
    token[i] = '\0';

    /*
     * Optional "/N" prefix length. No suffix at all -> *prefixlen = 128,
     * i.e. exactly the address itself - preserving every pre-CIDR
     * NetRestrict entry's meaning unchanged.
     */
    *prefixlen = 128;
    slash = strchr(token, '/');
    if (slash) {
	char *endPtr;

	*slash = '\0';
	slash++;
	if (*slash == '\0')
	    return AFS_IPINVALID;	/* "/" with nothing after it */
	errno = 0;
	plen = strtol(slash, &endPtr, 10);
	if (*endPtr != '\0' || errno != 0 || plen < 0 || plen > 128)
	    return AFS_IPINVALID;	/* malformed or out-of-range prefix */
	*prefixlen = (afs_uint32)plen;
    }

    if (inet_pton(AF_INET6, token, addr) != 1)
	return AFS_IPINVALID;
    return 0;
}

/**
 * Scan a NetRestrict file for IPv6-literal lines and return each as a
 * struct rx_sockaddr plus its prefix length (128 for a bare address, i.e.
 * an exact match - see extract_Addr6()), so a caller can match either an
 * exact address or a real CIDR prefix such as fd00:af5:1::/64. A v4
 * entry, a blank line, or any other unparseable line is silently skipped
 * here (extract_Addr()/parseNetRestrictFile_int() already handle v4
 * lines; this pass only looks for v6 ones).
 *
 * Unlike parseNetRestrictFile_int(), a missing/unreadable file, or a file
 * with no v6 lines, is not an error - IPv6 entries in NetRestrict are
 * entirely optional, and the caller's job is to tell "an operator
 * configured this" from "nothing was configured" apart.
 *
 * @param[out] outAddrs
 *     IPv6 addresses found, as struct rx_sockaddr
 * @param[out] outPrefixLens
 *     Prefix length for each entry in outAddrs[] (same indexing) - 128
 *     for a bare address, or the parsed "/N" value
 * @param[in] maxAddrs
 *     Length of outAddrs[]/outPrefixLens[]
 * @param[in] fileName
 *     NetRestrict file to scan (may be NULL)
 *
 * @return
 *     The number of v6 addresses found (0 if none, ever)
 */
static int
parseNetRestrictFileV6_int(struct rx_sockaddr outAddrs[],
			   afs_uint32 outPrefixLens[], afs_uint32 maxAddrs,
			   const char *fileName)
{
    FILE *fp;
    char line[MAX_NETFILE_LINE];
    afs_uint32 n = 0;
    struct in6_addr v6addr;
    afs_uint32 prefixlen;

    if (!fileName || maxAddrs == 0)
	return 0;

    if ((fp = fopen(fileName, "r")) == NULL)
	return 0;

    while (n < maxAddrs && fgets(line, MAX_NETFILE_LINE, fp) != NULL) {
	if (extract_Addr6(line, strlen(line), &v6addr, &prefixlen) != 0)
	    continue;		/* not a v6 literal - fine, e.g. a v4 line */

	memset(&outAddrs[n], 0, sizeof(outAddrs[n]));
	rx_ipv6_to_sockaddr((unsigned char *)&v6addr, 0, 0, &outAddrs[n]);
	outPrefixLens[n] = prefixlen;
	n++;
    }
    fclose(fp);
    return (int)n;
}
#endif /* HAVE_IPV6 */

/**
 * The line parameter is a pointer to a buffer containing a string of
 * bytes of the form:
 *
 * w.x.y.z[/n] 	# machineName
 *
 * Returns an IPv4 address and mask in network byte order.  Optionally,
 * a '/' may be used to specify a subnet mask length.
 *
 * @param[in] line
 *     Pointer to a string of bytes
 * @param[out] maxSize
 *     Length to search in line for addresses
 * @param[out] addr
 *     IPv4 address in network byte order
 * @param[out] mask
 *     IPv4 subnet mask in network byte order, default to 0xffffffff
 *
 * @return
 *      @retval 0 success
 *      @retval AFS_IPINVALID the address is invalid or parsing failed
 *      @retval AFS_IPINVALIDIGNORE blank line that can be ignored
 */
static int
extract_Addr(char *line, int maxSize, afs_uint32 *addr, afs_uint32 *mask)
{
    char bytes[4][32];
    int i = 0, n = 0;
    char *endPtr;
    afs_uint32 val[4];
    int subnet_len = 32;

    /* skip empty spaces */
    while (isspace(*line) && maxSize) {
	line++;
	maxSize--;
    }
    /* skip empty lines */
    if (!maxSize || !*line)
	return AFS_IPINVALIDIGNORE;

    /* init to 0.0.0.0 for strtol() */
    for (n = 0; n < 4; n++) {
	bytes[n][0] = '0';
	bytes[n][1] = '\0';
    }

    for (n = 0; n < 4; n++) {
	while ((*line != '.') && !isspace(*line)
	       && (*line != '/') && maxSize) {	/* extract nth byte */
	    if (!isdigit(*line))
		return AFS_IPINVALID;
	    if (i > 31)
		return AFS_IPINVALID;	/* no space */
	    bytes[n][i++] = *line++;
	    maxSize--;
	}			/* while */
	if (!maxSize)
	    return AFS_IPINVALID;
	bytes[n][i] = '\0';
	if (*line == '/')
	    break;
	i = 0;
	line++;
    }

    if (*line == '.')
	++line;			/* single trailing . allowed */

    if (*line == '/') {		/* have a subnet length */
	line++;
        subnet_len = 0;
	while (isdigit(*line)) {
	    subnet_len = subnet_len * 10 + (*line - '0');
	    if (subnet_len > 32)
		    return AFS_IPINVALID;	/* subnet length too long */
	    ++line;
	}
	if (subnet_len == 0)
	    return AFS_IPINVALID;	/* subnet length too short */
    }

    if (!isspace(*line) && (*line != '\0'))
	    return AFS_IPINVALID;	/* improperly formed comment */

    for (n = 0; n < 4; n++) {
	errno = 0;
	val[n] = strtol(bytes[n], &endPtr, 10);
	if ((val[n] == 0) && (errno != 0 || bytes[n] == endPtr)) /* no conversion */
	    return AFS_IPINVALID;
    }

    *mask = 0;
    while (subnet_len--) {
	*mask = (*mask >> 1) | 0x80000000;
    }

    *mask = htonl(*mask);
    *addr = htonl((val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3]);
    return 0;
}

/**
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
 * All addresses should be in network byte order as returned by
 * rx_getAllAddrMaskMtu() and parsed by extract_Addr().
 *
 * @param[out] outAddrs
 *     All the address that are found to be valid.
 * @param[out] outMask
 *     Optional associated netmask for address
 * @param[out] outMtu
 *     Optional associated MTU for address
 * @param[in] maxAddres
 *     Length of the above output arrays
 * @param[out] nAddrs
 *     Count of valid addresses
 * @param[out] reason
 *     Reason (if any) for the parsing failure
 * @param[in] fileName
 *     Configuration file to parse
 *
 * @return
 *     0 on success; 1 if the config file was not used; -1 on
 *     fatal failure.
 */
static int
parseNetRestrictFile_int(afs_uint32 outAddrs[], afs_uint32 outMask[],
			 afs_uint32 outMtu[], afs_uint32 maxAddrs,
			 afs_uint32 *nAddrs, char reason[],
			 const char *fileName, const char *fileName_ni)
{
    FILE *fp;
    char line[MAX_NETFILE_LINE];
    int lineNo, usedfile = 0;
    afs_uint32 i, neaddrs, nOutaddrs;
    afs_uint32 addr, mask, eAddrs[MAXIPADDRS], eMask[MAXIPADDRS], eMtu[MAXIPADDRS];
    int retval;

    opr_Assert(outAddrs);
    opr_Assert(reason);
    opr_Assert(fileName);
    opr_Assert(nAddrs);
    if (outMask)
	opr_Assert(outMtu);

    /* Initialize */
    *nAddrs = 0;
    for (i = 0; i < maxAddrs; i++)
	outAddrs[i] = 0;
    strcpy(reason, "");

    /* get all network interfaces from the kernel */
    neaddrs = rx_getAllAddrMaskMtu(eAddrs, eMask, eMtu, MAXIPADDRS);
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
	retval = extract_Addr(line, strlen(line), &addr, &mask);
	if (retval == AFS_IPINVALID) {	/* syntactically invalid */
	    fprintf(stderr, "%s : line %d : parse error - invalid IP\n",
		    fileName, lineNo);
	    continue;
	}
	if (retval == AFS_IPINVALIDIGNORE) {	/* ignore error */
	    fprintf(stderr, "%s : line %d : invalid address ... ignoring\n",
		    fileName, lineNo);
	    continue;
	}
	usedfile = 1;

	/* Check if we need to exclude this address */
	for (i = 0; i < neaddrs; i++) {
	    if (eAddrs[i] && ((eAddrs[i] & mask) == (addr & mask))) {
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
	if (outMask) {
	    outMask[nOutaddrs] = eMask[i];
	    outMtu[nOutaddrs] = eMtu[i];
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
afsconf_ParseNetRestrictFile(afs_uint32 outAddrs[], afs_uint32 outMask[],
			     afs_uint32 outMtu[], afs_uint32 maxAddrs,
			     afs_uint32 * nAddrs, char reason[],
			     const char *fileName)
{
    return parseNetRestrictFile_int(outAddrs, outMask, outMtu, maxAddrs, nAddrs, reason, fileName, 0);
}

/**
 * Get a list of IP addresses for this host allowing only addresses found
 * in the config file (fileName parameter): /usr/vice/etc/NetInfo for
 * clients and /usr/afs/local/NetInfo for servers.
 *
 * All addresses should be in network byte order as returned by
 * rx_getAllAddrMaskMtu() and parsed by extract_Addr().
 *
 * @param[out] outAddrs
 *     All the address that are found to be valid.
 * @param[out] outMask
 *     Associated netmask for interface
 * @param[out] outMtu
 *     Associated MTU for interface
 * @param[in] max
 *     Length of the output above arrays
 * @param[out] reason
 *     Reason for the parsing failure
 * @param[in] fileName
 *     File to parse
 * @param[in] fakeonly
 *     Only return addresses if they are marked as fake
 *
 * @return
 *     The number of valid address on success or < 0 on fatal failure.
 */
static int
ParseNetInfoFile_int(afs_uint32 outAddrs[], afs_uint32 outMask[], afs_uint32 outMtu[],
		     int max, char reason[], const char *fileName,
		     int fakeonly)
{

    afs_uint32 existingAddr[MAXIPADDRS], existingMask[MAXIPADDRS],
	existingMtu[MAXIPADDRS];
    char line[MAX_NETFILE_LINE];
    FILE *fp;
    int i, existNu, count = 0;
    afs_uint32 addr, mask;
    int lineNo = 0;
    int l;
    int retval;

    opr_Assert(fileName);
    opr_Assert(outAddrs);
    opr_Assert(outMask);
    opr_Assert(outMtu);
    opr_Assert(reason);

    /* get all network interfaces from the kernel */
    existNu =
	rx_getAllAddrMaskMtu(existingAddr, existingMask, existingMtu,
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
	    outAddrs[i] = existingAddr[i];
	    outMask[i] = existingMask[i];
	    outMtu[i] = existingMtu[i];
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
	retval = extract_Addr(&line[fake], strlen(&line[fake]), &addr, &mask);

	if (retval == AFS_IPINVALID) {	/* syntactically invalid */
	    fprintf(stderr, "afs:%s : line %d : parse error\n", fileName,
		    lineNo);
	    continue;
	}
	if (fake && ntohl(mask) != 0xffffffff) {
	    fprintf(stderr, "afs:%s : line %d : bad fake address\n", fileName,
		    lineNo);
	    continue;
	}
	if (retval == AFS_IPINVALIDIGNORE) {	/* ignore error */
	    continue;
	}

	/* See if it is an address that really exists */
	for (i = 0; i < existNu; i++) {
	    if ((existingAddr[i] & mask) == (addr & mask))
		break;
	}
	if ((i >= existNu) && (!fake))
	    continue;		/* not found/fake - ignore */

	/* Check if it is a duplicate address we alread have */
	for (l = 0; l < count; l++) {
	    if ((outAddrs[l] & mask) == (addr & mask))
		break;
	}
	if (l < count) {
	    fprintf(stderr, "afs:%x matched more than once in NetInfo file\n",
		    ntohl(outAddrs[l]));
	    continue;		/* duplicate addr - ignore */
	}

	if (count > max) {	/* no more space */
	    fprintf(stderr,
		    "afs:Too many interfaces. The current kernel configuration supports a maximum of %d interfaces\n",
		    max);
	} else if (fake) {
	    if (!fake)
		fprintf(stderr, "Client (2) also has address %s\n", line);
	    outAddrs[count] = addr;
	    outMask[count] = 0xffffffff;
	    outMtu[count] = htonl(1500);
	    count++;
	} else if (!fakeonly) {
	    outAddrs[count] = existingAddr[i];
	    outMask[count] = existingMask[i];
	    outMtu[count] = existingMtu[i];
	    count++;
	}
    }				/* while */

    /* in case of any error, we use all the interfaces present */
    if (count <= 0) {
	sprintf(reason,
		"Error in reading/parsing Interface file\nUsing all configured interface addresses \n");
	for (i = 0; i < existNu; i++) {
	    outAddrs[i] = existingAddr[i];
	    outMask[i] = existingMask[i];
	    outMtu[i] = existingMtu[i];
	}
	return existNu;
    }
    return count;
}

int
afsconf_ParseNetInfoFile(afs_uint32 outAddrs[], afs_uint32 outMask[], afs_uint32 outMtu[],
			 int max, char reason[], const char *fileName)
{
    return ParseNetInfoFile_int(outAddrs, outMask, outMtu, max, reason, fileName, 0);
}

/*
 * Given two arrays of addresses, masks and mtus find the common ones
 * and return them in the first buffer. Return number of common
 * entries.
 */
static int
filterAddrs(afs_uint32 addr1[], afs_uint32 addr2[], afs_uint32 mask1[],
	    afs_uint32 mask2[], afs_uint32 mtu1[], afs_uint32 mtu2[], int n1,
	    int n2)
{
    afs_uint32 taddr[MAXIPADDRS];
    afs_uint32 tmask[MAXIPADDRS];
    afs_uint32 tmtu[MAXIPADDRS];
    int count = 0, i = 0, j = 0, found = 0;

    opr_Assert(addr1);
    opr_Assert(addr2);
    opr_Assert(mask1);
    opr_Assert(mask2);
    opr_Assert(mtu1);
    opr_Assert(mtu2);

    for (i = 0; i < n1; i++) {
	found = 0;
	for (j = 0; j < n2; j++) {
	    if (addr1[i] == addr2[j]) {
		found = 1;
		break;
	    }
	}

	/* Always mask loopback address */
	if (found && rx_IsLoopbackAddr(addr1[i]))
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
 * parse both NetInfo and NetRestrict files and return the final
 * set of IP addresses to use
 */
/* max - Entries in addrbuf, maskbuf and mtubuf */
int
afsconf_ParseNetFiles(afs_uint32 addrbuf[], afs_uint32 maskbuf[],
		      afs_uint32 mtubuf[], afs_uint32 max, char reason[],
		      const char *niFileName, const char *nrFileName)
{
    afs_uint32 addrbuf1[MAXIPADDRS], maskbuf1[MAXIPADDRS],
	mtubuf1[MAXIPADDRS];
    afs_uint32 addrbuf2[MAXIPADDRS], maskbuf2[MAXIPADDRS],
	mtubuf2[MAXIPADDRS];
    int nAddrs1 = 0;
    afs_uint32 nAddrs2 = 0;
    int code, i;

    nAddrs1 =
	afsconf_ParseNetInfoFile(addrbuf1, maskbuf1, mtubuf1, MAXIPADDRS,
				 reason, niFileName);
    code =
	parseNetRestrictFile_int(addrbuf2, maskbuf2, mtubuf2, MAXIPADDRS,
			     &nAddrs2, reason, nrFileName, niFileName);
    if ((nAddrs1 < 0) && (code)) {
	/* both failed */
	return -1;
    } else if ((nAddrs1 > 0) && (code)) {
	/* NetInfo succeeded and NetRestrict failed */
	for (i = 0; ((i < nAddrs1) && (i < max)); i++) {
	    addrbuf[i] = addrbuf1[i];
	    if (maskbuf) {
		maskbuf[i] = maskbuf1[i];
		mtubuf[i] = mtubuf1[i];
	    }
	}
	return i;
    } else if ((!code) && (nAddrs1 < 0)) {
	/* NetRestrict succeeded and NetInfo failed */
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

/**
 * Like afsconf_ParseNetFiles(), but returns struct rx_sockaddr[] instead
 * of a bare afs_uint32[] address list, so IPv6 addresses can be
 * represented too, plus a parallel prefixbuf[] giving each entry's prefix
 * length.
 *
 * The IPv4 half of this is exactly afsconf_ParseNetFiles()'s own,
 * unchanged result (interface enumeration, NetInfo inclusion, NetRestrict
 * exclusion, mask-based intersection - all of it), just converted to
 * struct rx_sockaddr entries; nothing about how a v4 address is selected
 * changes here. Every v4 entry reports a prefixbuf[] value of 32 (an
 * exact host address) - afsconf_ParseNetFiles() already resolves any v4
 * subnet mask down to a specific set of real interface addresses, so
 * there is no partial-prefix result left to represent by the time it
 * reaches here.
 *
 * IPv6 gets a deliberately much smaller answer, appended after the v4
 * addresses: only NetRestrict is consulted (there's no v6 NetInfo
 * concept here), and each entry is either an exact address (prefixbuf[]
 * of 128) or a real CIDR prefix (any /0 through /128) - see
 * extract_Addr6()/parseNetRestrictFileV6_int() above. If nrFileName
 * doesn't exist, can't be read, or has no v6-literal lines, zero v6
 * addresses are returned - not an error, and callers that care whether an
 * operator actually configured a v6 restriction (as opposed to "no
 * configuration at all") should treat "zero v6 addresses back" as exactly
 * that signal.
 *
 * @param[out] addrbuf
 *     Addresses found: IPv4 first (from afsconf_ParseNetFiles()), then
 *     any IPv6 entries from NetRestrict
 * @param[out] prefixbuf
 *     Prefix length for each entry in addrbuf[] (same indexing): 32 for
 *     every IPv4 entry, 128 for an IPv6 exact-match entry, or the
 *     parsed "/N" value for an IPv6 CIDR entry
 * @param[in] max
 *     Length of addrbuf[]/prefixbuf[]
 * @param[out] reason
 *     Reason (if any) for an afsconf_ParseNetFiles() parsing failure
 * @param[in] niFileName
 *     NetInfo file to parse (IPv4 only)
 * @param[in] nrFileName
 *     NetRestrict file to parse (IPv4 exclusion, plus IPv6 inclusion)
 *
 * @return
 *     The total number of addresses (IPv4 + IPv6) on success, or a
 *     negative value if afsconf_ParseNetFiles()'s own IPv4 pass fails
 *     fatally (matching its own return convention - a v6-only NetRestrict
 *     entry can never make the v4 half's own failure non-fatal, since the
 *     two are independent concerns).
 */
int
afsconf_ParseNetFilesSA(struct rx_sockaddr addrbuf[], afs_uint32 prefixbuf[],
			afs_uint32 max, char reason[], const char *niFileName,
			const char *nrFileName)
{
    afs_uint32 addrbuf4[MAXIPADDRS];
    afs_uint32 max4 = (max < MAXIPADDRS) ? max : MAXIPADDRS;
    afs_uint32 ntotal = 0;
    int code, i;

    code = afsconf_ParseNetFiles(addrbuf4, NULL, NULL, max4, reason,
				 niFileName, nrFileName);
    if (code < 0)
	return code;

    for (i = 0; (afs_uint32)i < (afs_uint32) code && ntotal < max; i++) {
	memset(&addrbuf[ntotal], 0, sizeof(addrbuf[ntotal]));
	rx_ipv4_to_sockaddr(addrbuf4[i], 0, 0, &addrbuf[ntotal]);
	prefixbuf[ntotal] = 32;
	ntotal++;
    }

#ifdef HAVE_IPV6
    if (ntotal < max) {
	int n6 = parseNetRestrictFileV6_int(&addrbuf[ntotal],
					    &prefixbuf[ntotal], max - ntotal,
					    nrFileName);
	if (n6 > 0)
	    ntotal += (afs_uint32) n6;
    }
#endif

    return (int) ntotal;
}
