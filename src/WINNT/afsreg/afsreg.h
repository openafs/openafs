/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSREG_H_
#define AFSREG_H_

/* Registry keys and values accessed by AFS server processes and
 * the AFS software installation and configuration utilities.
 */

#define AFSREG_NULL_KEY  ((HKEY)0)

/* Do not change AFSREG_{CLT,SVR}_SW_NAME unless the installation
 * name in the installers is also changed to match
 */
#define AFSREG_SVR_SVC_NAME  TEXT("TransarcAFSServer")
#define AFSREG_SVR_SW_NAME   TEXT("AFS Server")

#define AFSREG_CLT_SVC_NAME  TEXT("TransarcAFSDaemon")
#define AFSREG_CLT_SW_NAME   	  TEXT("AFS Client")
#define AFSREG_CLT_TOOLS_SW_NAME  TEXT("AFS Client 32-Bit Binaries")

/* ---- NT system configuration information ---- */

/* TCP/IP registry keys and values of interest:
 *
 * HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services
 *
 *     Tcpip\Linkage
 *         Bind:REG_MULTI_SZ:<interface adapter list>
 *
 *     <adapter name>\Parameters\Tcpip
 *         IPAddress:REG_MULTI_SZ:<list of dotted decimal IP addresses>
 *         SubnetMask:REG_MULTI_SZ:<list of dotted decimal IP address masks>
 *         DhcpIPAddress:REG_SZ:<dotted decimal IP address>
 *         DhcpSubnetMask:REG_SZ:<dotted decimal IP address mask>
 */

#define AFSREG_IPSRV_KEY \
TEXT("HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Services")

/* Adapter list subkey  and subkey values */
#define AFSREG_IPSRV_IFACELIST_SUBKEY         TEXT("Tcpip\\Linkage")
#define AFSREG_IPSRV_IFACELIST_BIND_VALUE     TEXT("Bind")

/* Per-adapter subkey and subkey values */
#define AFSREG_IPSRV_ADAPTER_PARAM_SUBKEY          TEXT("Parameters\\Tcpip")
#define AFSREG_IPSRV_ADAPTER_PARAM_ADDR_VALUE      TEXT("IPAddress")
#define AFSREG_IPSRV_ADAPTER_PARAM_MASK_VALUE      TEXT("SubnetMask")
#define AFSREG_IPSRV_ADAPTER_PARAM_DHCPADDR_VALUE  TEXT("DhcpIPAddress")
#define AFSREG_IPSRV_ADAPTER_PARAM_DHCPMASK_VALUE  TEXT("DhcpSubnetMask")

/*
 * Event logging registry keys and values of interest:
 *
 * HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog
 *
 *     Application\AFSREG_SVR_APPLOG_SUBKEY
 *         EventMessageFile:REG_EXPAND_SZ:<AFS event message file path>
 *         TypesSupported:REG_DWORD:<EVENTLOG_ERROR_TYPE |
 *                                   EVENTLOG_WARNING_TYPE |
 *                                   EVENTLOG_INFORMATION_TYPE>
 */

#define AFSREG_APPLOG_SUBKEY \
    TEXT("System\\CurrentControlSet\\Services\\EventLog\\Application")
#define AFSREG_APPLOG_KEY TEXT("HKEY_LOCAL_MACHINE\\") AFSREG_APPLOG_SUBKEY

/* AFS event source subkey and subkey values -- client and server services */
#define AFSREG_SVR_APPLOG_SUBKEY         AFSREG_SVR_SW_NAME
#define AFSREG_CLT_APPLOG_SUBKEY         AFSREG_CLT_SW_NAME
#define AFSREG_APPLOG_MSGFILE_VALUE  TEXT("EventMessageFile")
#define AFSREG_APPLOG_MSGTYPE_VALUE  TEXT("TypesSupported")




/* ---- AFS software configuration information ---- */

/* HKEY_LOCAL_MACHINE\SOFTWARE\TransarcCorporation\AFSREG_SVR_SW_NAME
 *
 *     CurrentVersion
 *         PathName:REG_SZ:<AFS installation directory>
 *         MajorVersion:REG_DWORD:<major version number>
 *         MinorVersion:REG_DWORD:<minor version number>
 *         PatchLevel:REG_DWORD:<patch level>
 */

#define AFSREG_SVR_SW_SUBKEY \
    TEXT("Software\\TransarcCorporation\\") AFSREG_SVR_SW_NAME
#define AFSREG_SVR_SW_KEY TEXT("HKEY_LOCAL_MACHINE\\") AFSREG_SVR_SW_SUBKEY

#define AFSREG_SVR_SW_VERSION_KEY  AFSREG_SVR_SW_KEY TEXT("\\CurrentVersion")
#define AFSREG_SVR_SW_VERSION_SUBKEY  AFSREG_SVR_SW_SUBKEY TEXT("\\CurrentVersion")

/* AFSREG_SVR_SW_VERSION_KEY values */
#define AFSREG_SVR_SW_VERSION_DIR_VALUE      TEXT("PathName")
#define AFSREG_SVR_SW_VERSION_MAJOR_VALUE    TEXT("MajorVersion")
#define AFSREG_SVR_SW_VERSION_MINOR_VALUE    TEXT("MinorVersion")
#define AFSREG_SVR_SW_VERSION_PATCH_VALUE    TEXT("PatchLevel")


/* HKEY_LOCAL_MACHINE\SOFTWARE\TransarcCorporation\AFSREG_CLT_SW_NAME
 *
 *     CurrentVersion
 *         PathName:REG_SZ:<AFS installation directory>
 *         MajorVersion:REG_DWORD:<major version number>
 *         MinorVersion:REG_DWORD:<minor version number>
 *         PatchLevel:REG_DWORD:<patch level>
 */

#define AFSREG_CLT_SW_SUBKEY \
    TEXT("Software\\TransarcCorporation\\") AFSREG_CLT_SW_NAME
#define AFSREG_CLT_SW_KEY TEXT("HKEY_LOCAL_MACHINE\\") AFSREG_CLT_SW_SUBKEY

#define AFSREG_CLT_SW_VERSION_KEY  AFSREG_CLT_SW_KEY TEXT("\\CurrentVersion")
#define AFSREG_CLT_SW_VERSION_SUBKEY  AFSREG_CLT_SW_SUBKEY TEXT("\\CurrentVersion")

#define AFSREG_CLT_TOOLS_SW_SUBKEY \
    TEXT("Software\\TransarcCorporation\\") AFSREG_CLT_TOOLS_SW_NAME
#define AFSREG_CLT_TOOLS_SW_KEY TEXT("HKEY_LOCAL_MACHINE\\") AFSREG_CLT_TOOLS_SW_SUBKEY

#define AFSREG_CLT_TOOLS_SW_VERSION_KEY  AFSREG_CLT_TOOLS_SW_KEY TEXT("\\CurrentVersion")
#define AFSREG_CLT_TOOLS_SW_VERSION_SUBKEY  AFSREG_CLT_TOOLS_SW_SUBKEY TEXT("\\CurrentVersion")

/* AFSREG_CLT_SW_VERSION_KEY values */
#define AFSREG_CLT_SW_VERSION_DIR_VALUE      TEXT("PathName")
#define AFSREG_CLT_SW_VERSION_MAJOR_VALUE    TEXT("MajorVersion")
#define AFSREG_CLT_SW_VERSION_MINOR_VALUE    TEXT("MinorVersion")
#define AFSREG_CLT_SW_VERSION_PATCH_VALUE    TEXT("PatchLevel")



/* ---- AFS service configuration information ---- */

/* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\AFSREG_SVR_SVC_NAME
 *
 *     DisplayName:REG_SZ:<service display name>
 *     ImagePath:REG_EXPAND_SZ:<service binary path>
 *
 *     Afstab
 *         <vice partition name>
 *             DeviceName:REG_SZ:<device hosting device partition>
 */

#define AFSREG_SVR_SVC_DISPLAYNAME_DATA  TEXT("OpenAFS Server")
#define AFSREG_SVR_SVC_IMAGENAME_DATA TEXT("bosctlsvc.exe")

#define AFSREG_SVR_SVC_SUBKEY TEXT("System\\CurrentControlSet\\Services\\") AFSREG_SVR_SVC_NAME
#define AFSREG_SVR_SVC_KEY TEXT("HKEY_LOCAL_MACHINE\\") AFSREG_SVR_SVC_SUBKEY

#define AFSREG_SVR_SVC_AFSTAB_KEY   AFSREG_SVR_SVC_KEY TEXT("\\Afstab")

/* AFSREG_SVR_SVC_AFSTAB_KEY partition subkey values */
#define AFSREG_SVR_SVC_AFSTAB_DEVNAME_VALUE      TEXT("DeviceName")


/* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\AFSREG_CLT_SVC_NAME
 *
 *     DisplayName:REG_SZ:<service display name>
 *     ImagePath:REG_EXPAND_SZ:<service binary path>
 *
 *     Parameters
 *         Cell:REG_SZ:<client cell>
 */

#define AFSREG_CLT_SVC_DISPLAYNAME_DATA  TEXT("OpenAFS Client")
#define AFSREG_CLT_SVC_IMAGENAME_DATA TEXT("afsd_service.exe")

#define AFSREG_CLT_SVC_SUBKEY TEXT("System\\CurrentControlSet\\Services\\") AFSREG_CLT_SVC_NAME
#define AFSREG_CLT_SVC_KEY TEXT("HKEY_LOCAL_MACHINE\\") AFSREG_CLT_SVC_SUBKEY

#define AFSREG_CLT_SVC_PARAM_KEY   AFSREG_CLT_SVC_KEY TEXT("\\Parameters")
#define AFSREG_CLT_SVC_PARAM_SUBKEY   AFSREG_CLT_SVC_SUBKEY TEXT("\\Parameters")
#define AFSREG_CLT_SVC_PROVIDER_KEY AFSREG_CLT_SVC_KEY TEXT("\\NetworkProvider")
#define AFSREG_CLT_SVC_PROVIDER_SUBKEY AFSREG_CLT_SVC_SUBKEY TEXT("\\NetworkProvider")

/* AFSREG_CLT_SVC_PARAM_KEY values */
#define AFSREG_CLT_SVC_PARAM_CELL_VALUE      TEXT("Cell")

#define AFSREG_CLT_OPENAFS_SUBKEY TEXT("Software\\OpenAFS\\Client")
#define AFSREG_CLT_OPENAFS_KEY TEXT("HKEY_LOCAL_MACHINE\\") AFSREG_CLT_OPENAFS_SUBKEY
#define AFSREG_CLT_OPENAFS_CELLSERVDB_DIR_VALUE  TEXT("CellServDBDir")

#define AFSREG_USER_OPENAFS_SUBKEY TEXT("Software\\OpenAFS\\Client")
#define AFSREG_USER_OPENAFS_KEY TEXT("HKEY_CURRENT_USER") AFSREG_USER_OPENAFS_SUBKEY


/* Extended (alternative) versions of registry access functions */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    REGENTRY_KEY,
    REGENTRY_VALUE
} regentry_t;

extern long
RegOpenKeyAlt(HKEY key,
	      const char *subKeyName,
	      DWORD mode,
	      int create,
	      HKEY *resultKeyP,
	      DWORD *resultKeyDispP);

extern long
RegQueryValueAlt(HKEY key,
		 const char *valueName,
		 DWORD *dataTypeP,
		 void **dataPP,
		 DWORD *dataSizeP);

extern long
RegEnumKeyAlt(HKEY key,
	      char **subkeyNames);

extern long
RegDeleteKeyAlt(HKEY key,
		const char *subKeyName);

extern long
RegDeleteEntryAlt(const char *entryName,
		  regentry_t entryType);

extern long
RegDupKeyAlt(const char *sourceKey,
	     const char *targetKey);

extern int
IsWow64(void);

#ifdef __cplusplus
};
#endif

#endif /* AFSREG_H_ */
