/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Functions for accessing NT system configuration information. */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <iptypes.h>
#include <ipifcons.h>

#include "afsreg.h"
#include "syscfg.h"

static int IsLoopback(char * guid);

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
    PMIB_IPADDRTABLE pIpAddrTable = NULL;
    ULONG            dwSize;
    DWORD            code;
    DWORD            index;
    DWORD            validAddrs = 0;

    int maxCount = *count;
    int nConfig = 0;
    PIP_ADAPTER_ADDRESSES pAddresses, cAddress;
    PMIB_IPADDRTABLE pIpTbl;
    ULONG outBufLen = 0;
    DWORD dwRetVal = 0;
    int n = 0;
    DWORD i;

    HMODULE hIpHlp;
    DWORD (WINAPI *pGetAdaptersAddresses)(ULONG, DWORD, PVOID, 
                                          PIP_ADAPTER_ADDRESSES, PULONG) = 0;

    hIpHlp = LoadLibrary("iphlpapi");
    if (hIpHlp != NULL) {
        (FARPROC) pGetAdaptersAddresses = GetProcAddress(hIpHlp, "GetAdaptersAddresses");
        if (pGetAdaptersAddresses == NULL)
            FreeLibrary(hIpHlp);
    }

    if (pGetAdaptersAddresses == NULL)
        return syscfg_GetIFInfo_2000(count, addrs, masks, mtus, flags);

    /* first pass to get the required size of the IP table */
    pIpTbl = (PMIB_IPADDRTABLE) malloc(sizeof(MIB_IPADDRTABLE));
    outBufLen = sizeof(MIB_IPADDRTABLE);
    
    dwRetVal = GetIpAddrTable(pIpTbl, &outBufLen, FALSE);
    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        /* this should have failed with an insufficient buffer because we
           didn't give any space to place the IP addresses */
        free(pIpTbl);
        *count = 0;
        nConfig = -1;
        goto done;
    }
    
    /* second pass to get the actual data */
    free(pIpTbl);
    pIpTbl = (PMIB_IPADDRTABLE) malloc(outBufLen);
    
    dwRetVal = GetIpAddrTable(pIpTbl, &outBufLen, FALSE);
    if (dwRetVal != NO_ERROR) {
        free(pIpTbl);
        *count = 0;
        nConfig = -1;
        goto done;
    }
    
    pAddresses = (IP_ADAPTER_ADDRESSES*) malloc(sizeof(IP_ADAPTER_ADDRESSES));
    
    /* first call gets required buffer size */
    if (pGetAdaptersAddresses(AF_INET, 
                              0, 
                              NULL, 
                              pAddresses, 
                              &outBufLen) == ERROR_BUFFER_OVERFLOW) 
    {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES*) malloc(outBufLen);
    } else {
        free(pIpTbl);
        *count = 0;
        nConfig = -1;
        goto done;
    }
    
    /* second call to get the actual data */
    if ((dwRetVal = pGetAdaptersAddresses(AF_INET, 
                                          0, 
                                          NULL, 
                                          pAddresses, 
                                          &outBufLen)) == NO_ERROR) 
    {
        /* we have a list of addresses.  go through them and figure out
           the IP addresses */
        for (cAddress = pAddresses; cAddress; cAddress = cAddress->Next) {
            
            /* skip software loopback adapters */
            if (cAddress->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;
            
            /* also skip interfaces that are not up */
            if (cAddress->OperStatus != 1)
                continue;
            
            /* starting with the AdapterName, which is actually the adapter
               instance GUID, check if this is a MS loopback device */
            if (IsLoopback(cAddress->AdapterName))
                continue;
            
            /* ok. looks good.  Now fish out all the addresses from the
               address table corresponding to the interface, and add them
               to the list */
            for (i=0;i<pIpTbl->dwNumEntries;i++) {
                if (pIpTbl->table[i].dwIndex == cAddress->IfIndex)
                {
                    if (n < maxCount) {
                        addrs[n] = ntohl(pIpTbl->table[i].dwAddr);
                        masks[n] = ntohl(pIpTbl->table[i].dwMask);
                        mtus[n] = cAddress->Mtu;
                        flags[n] = 0;
                        n++;
                    }
                    nConfig++;
                }
            }
        }
        
        free(pAddresses);
        free(pIpTbl);
        
        *count = n;
    } else { 
        /* again. this is bad */
        free(pAddresses);
        free(pIpTbl);
        *count = 0;
        nConfig = -1;
    }

  done:
    FreeLibrary(hIpHlp);
    return nConfig;
}

static int IsLoopback(char * guid)
{
    int isloopback = FALSE;
 
    HKEY hkNet = NULL;
    HKEY hkDev = NULL;
    HKEY hkDevConn = NULL;
    HKEY hkEnum = NULL;
    HKEY hkAdapter = NULL;
    
    char pnpIns[MAX_PATH];
    char hwId[MAX_PATH];
    char service[MAX_PATH];
    
    DWORD size;
    
    /* Open the network adapters key */
    if (FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &hkNet)))
        goto _exit;
    
    /* open the guid key */
    if (FAILED(RegOpenKeyEx(hkNet, guid, 0, KEY_READ, &hkDev)))
        goto _exit;
    
    /* then the connection */
    if (FAILED(RegOpenKeyEx(hkDev, "Connection", 0, KEY_READ, &hkDevConn)))
        goto _exit;
    
    /* and find out the plug-n-play instance ID */
    size = MAX_PATH;
    if (FAILED(RegQueryValueEx(hkDevConn, "PnpInstanceID", NULL, NULL, pnpIns, &size)))
        goto _exit;
    
    /* now look in the device ENUM */
    if (FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Enum", 0, KEY_READ, &hkEnum)))
        goto _exit;
    
    /* for the instance that we found above */
    if (FAILED(RegOpenKeyEx(hkEnum, pnpIns, 0, KEY_READ, &hkAdapter)))
        goto _exit;
    
    /* and fetch the harware ID */
    size = MAX_PATH;
    if (FAILED(RegQueryValueEx(hkAdapter, "HardwareID", NULL, NULL, hwId, &size)))
        goto _exit;
    
    size = MAX_PATH;
    if (FAILED(RegQueryValueEx(hkAdapter, "Service", NULL, NULL, service, &size)))
        goto _exit;
    
    /* and see if it is the loopback adapter */
    if (!stricmp(hwId, "*msloop") || !stricmp(service, "msloop"))
        isloopback = TRUE;
    
  _exit:
    if (hkAdapter)
        RegCloseKey(hkAdapter);
    if (hkEnum)
        RegCloseKey(hkEnum);
    if (hkDevConn)
        RegCloseKey(hkDevConn);
    if (hkDev)
        RegCloseKey(hkDev);
    if (hkNet)
        RegCloseKey(hkNet);
 
    return isloopback;
}

static int GetInterfaceList(HKEY skey, char **list);
static char *GetNextInterface(char *iflist);
static int GetIP(HKEY skey, char *ifname, int *addr, int *mask);

int syscfg_GetIFInfo_2000(int *count, int *addrs, int *masks, int *mtus, int *flags)
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
	if (!IsLoopback(ifname) && GetIP(skey, ifname, &addrs[n], &masks[n]) == 0 && addrs[n] != 0) {
	    n++;
	} else {
            maxCount--;
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

    /* interface substrings are assumed to be of form \Device\<adapter name> 
     * \Tcpip\Parameters\Interfaces\<adapter name>
     */
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
    DWORD dwDHCP;
    DWORD dwLease;
    DWORD dwSize;

    len = strlen(ifname) + 1 + sizeof(AFSREG_IPSRV_ADAPTER_PARAM_SUBKEY);
    s = malloc(len);
    if (!s)
	return -1;

    sprintf(s, "%s\\%s", ifname, AFSREG_IPSRV_ADAPTER_PARAM_SUBKEY);

    status = RegOpenKeyAlt(skey, s, KEY_READ, 0, &key, NULL);
    free(s);

    if (status)
	return -1;

    dwSize = sizeof(DWORD);
    status = RegQueryValueEx(key, "EnableDHCP", NULL,
			     &valType, &dwDHCP, &dwSize);
    if (status || (valType != REG_DWORD))
        dwDHCP = 0;

    if (dwDHCP == 0) {
        status = RegQueryValueAlt(key, AFSREG_IPSRV_ADAPTER_PARAM_ADDR_VALUE,
                                  &valType, &ipStr, NULL);
        if (status || (valType != REG_SZ && valType != REG_MULTI_SZ)) {
            if (ipStr) free(ipStr);
            (void) RegCloseKey(key);
            return -1;
        }

	status = RegQueryValueAlt(key, AFSREG_IPSRV_ADAPTER_PARAM_MASK_VALUE,
				  &valType, &snMask, NULL);
	if (status || (valType != REG_SZ && valType != REG_MULTI_SZ)) {
	    if (snMask) free(snMask);
	    snMask = NULL;
	}
    } else {
	/* adapter configured via DHCP; address/mask in alternate values */
        dwSize = sizeof(DWORD);
        status = RegQueryValueEx(key, "Lease", NULL,
                                 &valType, &dwLease, &dwSize);
        if (status || (valType != REG_DWORD) || dwLease == 0) {
            (void) RegCloseKey(key);
            return -1;
        }

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

