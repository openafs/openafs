/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Functions for accessing NT system configuration information. */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#include "afsreg.h"
#include "syscfg.h"

static int GetInterfaceList(HKEY skey, char **list);
static char *GetNextInterface(char *iflist);
static int GetIP(HKEY skey, char *ifname, int *addr, int *mask);


/* syscfg_GetIFInfo
 *
 * Retrieve IP addresses, subnet masks, MTU sizes, and flags for all
 * configured interfaces.
 *
 * Arguments:
 * IN/OUT:
 *	count - in is max size of arrays. out is number of elements used.
 *
 * OUT:
 *	addrs - array of configured IP addresses, in host order.
 *	masks - array of subnet masks, in host order.
 *	mtus  - array of MTU sizes.
 *      flags - array of flags.
 *
 * Return Value:
 *	Total number of configured interfaces (>= count) or -1 on error.
 */

int syscfg_GetIFInfo(int *count, int *addrs, int *masks, int *mtus, int *flags)
{
    int maxCount = *count;
    char *IFListBase = NULL;
    char *IFList, *ifname;
    HKEY skey;
    int i, n, nConfig;

    if (RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_IPSRV_KEY,
		      KEY_READ, 0, &skey, NULL))
	return -1;

    if ((nConfig = GetInterfaceList(skey, &IFListBase)) < 0) {
	(void) RegCloseKey(skey);
	return -1;
    }

    IFList = IFListBase;
    n = 0;

    while ((n < maxCount) && (ifname = GetNextInterface(IFList))) {
	if (GetIP(skey, ifname, &addrs[n], &masks[n]) == 0) {
	    n++ ;
	}
	IFList = ifname;
    }

    /* And until we get mtu's and flags */
    for (i = 0; i < n; i++) {
	mtus[i] = 1500;
	flags[i] = 0;
    }

    (void) RegCloseKey(skey);
    free(IFListBase);

    *count = n;
    return nConfig;
}


/* GetInterfaceList
 *
 * Get interface list; list is represented as a multistring.
 * Returns number of elements in interface list or -1 on error.
 */
static int GetInterfaceList(HKEY skey, char **list)
{
    HKEY key;
    long status;
    char *str = NULL;
    int size;
    DWORD valType;

    if (RegOpenKeyAlt(skey, AFSREG_IPSRV_IFACELIST_SUBKEY,
		      KEY_READ, 0, &key, NULL))
	return -1;

    status = RegQueryValueAlt(key, AFSREG_IPSRV_IFACELIST_BIND_VALUE,
			      &valType, &str, NULL);
    (void) RegCloseKey(key);
    if (status || (valType != REG_MULTI_SZ))
	return -1;

    /* Count strings in multistring. */
    size = 0;

    if (*str != '\0') {
	int i;

	for (i = 1; ; i++) {
	    if (str[i] == '\0') {
		/* hit end of string */
		size++;
		i++;
		if (str[i] == '\0') {
		    /* hit end of multistring */
		    break;
		}
	    }
	}
    }
    *list = str;
    return size;
}


/* GetNextInterface
 *
 * Parse interface list.  In first call to GetNextInterface(), iflist is
 * the list returned by GetInterfaceList(); in successive calls, iflist is
 * the pointer returned by the previous call to GetNextInterface().
 *
 * Returns pointer to next adapter name, or NULL if done.
 */

static char *GetNextInterface(char *iflist)
{
    char *ifname;

    /* interface substrings are assumed to be of form \Device\<adapter name> */
    ifname = strrchr(iflist, '\\');

    if (!ifname) {
	/* subsequent (not first) call; advance to next interface substring */
	iflist += strlen(iflist) + 1;
	/* iflist now points to next interface or end-of-multistring char */
	ifname = strrchr(iflist, '\\');
    }

    if (ifname) {
	/* advance to first character of adapter name */
	ifname++;
    }

    return ifname;
}


/* GetIP
 *
 * Get IP address associated with interface (adapter name).
 * Returns 0 on success and -1 on error.
 */

static int GetIP(HKEY skey, char *ifname, int *addr, int *mask)
{
    HKEY key;
    char *s;
    long status;
    int len;
    char *ipStr = NULL;
    char *snMask = NULL;
    DWORD valType;

    len = strlen(ifname) + 1 + sizeof(AFSREG_IPSRV_ADAPTER_PARAM_SUBKEY);
    s = malloc(len);
    if (!s)
	return -1;

    sprintf(s, "%s\\%s", ifname, AFSREG_IPSRV_ADAPTER_PARAM_SUBKEY);

    status = RegOpenKeyAlt(skey, s, KEY_READ, 0, &key, NULL);
    free(s);

    if (status)
	return -1;

    status = RegQueryValueAlt(key, AFSREG_IPSRV_ADAPTER_PARAM_ADDR_VALUE,
			      &valType, &ipStr, NULL);
    if (status || (valType != REG_SZ && valType != REG_MULTI_SZ)) {
	if (ipStr) free(ipStr);
	(void) RegCloseKey(key);
	return -1;
    }

    if (*ipStr != '0') {
	status = RegQueryValueAlt(key, AFSREG_IPSRV_ADAPTER_PARAM_MASK_VALUE,
				  &valType, &snMask, NULL);
	if (status || (valType != REG_SZ && valType != REG_MULTI_SZ)) {
	    if (snMask) free(snMask);
	    snMask = NULL;
	}
    } else {
	/* adapter configured via DHCP; address/mask in alternate values */
	free(ipStr);
	ipStr = NULL;

	status = RegQueryValueAlt(key,
				  AFSREG_IPSRV_ADAPTER_PARAM_DHCPADDR_VALUE,
				  &valType, &ipStr, NULL);

	if (status || (valType != REG_SZ && valType != REG_MULTI_SZ)) {
	    if (ipStr) free(ipStr);
	    (void) RegCloseKey(key);
	    return -1;
	}

	status = RegQueryValueAlt(key,
				  AFSREG_IPSRV_ADAPTER_PARAM_DHCPMASK_VALUE,
				  &valType, &snMask, NULL);

	if (status || (valType != REG_SZ && valType != REG_MULTI_SZ)) {
	    if (snMask) free(snMask);
	    snMask = NULL;
	}
    }

    /* convert ip and subnet. */
    *addr = (int)inet_addr(ipStr);
    *addr = ntohl(*addr);
    free(ipStr);

    if (snMask) {
        *mask = (int)inet_addr(snMask);
	*mask = ntohl(*mask);
	free(snMask);
    } else {
        *mask = 0;
    }

    (void) RegCloseKey(key);

    return 0;
}
