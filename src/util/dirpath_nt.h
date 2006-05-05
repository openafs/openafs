/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _DIRPATH_H
#define _DIRPATH_H

/* Dirpath package: Rationale and Usage
 *
 * With the port of AFS to Windows NT, it becomes necessary to support
 * storing AFS system files (binaries, logs, etc.) in a user-specified
 * installation directory.  This breaks from the traditional notion of
 * all AFS system files being stored under /usr/afs or /usr/vice.
 *
 * The core concept is that there is a dynamically determined installation
 * directory that is the prefix to a well-known AFS tree.  The structure
 * of this well-known AFS tree remains unchanged.  For example, AFS server
 * binaries reside in <install dir>/usr/afs/bin, server configuration files
 * reside in <install dir>/usr/afs/etc, etcetera.  This scheme allows the
 * flexibility required by NT, without requiring file-location changes
 * on Unix (for which <install dir> can simply be null).
 *
 * Thus file paths can no longer be hard-coded; rather, all file paths are
 * specified via the macros provided by this package.
 *
 * Utilizing a dynamically determined installation directory forces the
 * notion of local versus canonical (wire-format) paths.  A local path is
 * fully qualified (with a syntax determined by the native filesystem type)
 * whereas a canonical path specifies location only within the well-known AFS
 * tree.  Supporting the notion of canonical paths allows the same path to
 * be sent to servers on different machines, having different installation
 * directories, with the desired result.
 *
 * For example, 'bos create <mach> kaserver simple /usr/afs/bin/kasever' will
 * work both on a Unix machine, where the kaserver executable actually resides
 * in /usr/afs/bin, and on a NT machine, where the kaserver may reside in
 * C:/Program Files/Transarc/AFS Server/usr/afs/bin.
 *
 * Conversion functions are provided that translate canonical (wire-format)
 * paths to fully qualified local paths; see the documentation in dirpath.c
 * for details.  Note that these conversion functions also accomodate fully
 * qualified paths on the wire, for exceptional cases where this feature
 * may be required.  Again, these conversion functions have been implemented
 * such that no file-location changes are required on Unix.
 *
 * The path macros provided here are divided into local (AFSDIR_*) and
 * canonical (AFSDIR_CANONICAL_*).  The canonical macros MUST be used by
 * commands that send a non-user-specified path to a server (i.e., over
 * the wire).  The local macros MUST be used in all other cases.
 */



#include <afs/param.h>

#ifdef AFS_NT40_ENV
#include <windef.h>
#endif
#include <limits.h>

/* Max dir path size for afs install dirs */
#ifdef AFS_NT40_ENV
#define AFSDIR_PATH_MAX MAX_PATH
#else /* unices */
#define AFSDIR_PATH_MAX    _POSIX_PATH_MAX
#endif


/* ---------------------  Exported functions ---------------------- */


#define AFSDIR_CLIENT_PATHS_OK 0x1	/* client paths initialized correctly */
#define AFSDIR_SERVER_PATHS_OK 0x2	/* server paths initialized correctly */
extern unsigned int initAFSDirPath(void);

extern int
  ConstructLocalPath(const char *cpath, const char *relativeTo,
		     char **fullPathBufp);
extern int
  ConstructLocalBinPath(const char *cpath, char **fullPathBufp);
extern int
  ConstructLocalLogPath(const char *cpath, char **fullPathBufp);



/* -----------------  Directory/file name macros ------------------ */

/* afs installation dir names */
#define AFSDIR_ETC_DIR   "/etc"
#define AFSDIR_BIN_DIR   "/bin"
#define AFSDIR_SERVER_ETC_DIR   "/usr/afs/etc"
#define AFSDIR_SERVER_BIN_DIR   "/usr/afs/bin"
#define AFSDIR_CLIENT_ETC_DIR   "/usr/afs/etc"
#define AFSDIR_CORES_DIR "/usr/afs/etc/cores"
#define AFSDIR_DB_DIR    "/usr/afs/etc/db"
#define AFSDIR_LOGS_DIR  "/usr/afs/etc/logs"
#define AFSDIR_LOCAL_DIR "/usr/afs/local"
#define AFSDIR_BACKUP_DIR "/usr/afs/backup"
#define AFSDIR_MIGR_DIR  "/usr/afs/local/migrate"
#define AFSDIR_BIN_FILE_DIR  "/usr/afs/local/migrate/bin_files"
#define AFSDIR_BOSCONFIG_DIR "/usr/afs/local"
#define AFSDIR_BOSSERVER_DIR "/usr/afs/bin"

/* file names */
#define AFSDIR_THISCELL_FILE    "ThisCell"
#define AFSDIR_CELLSERVDB_FILE  "CellServDB"
#define AFSDIR_CELLALIAS_FILE   "CellAlias"
#define AFSDIR_KEY_FILE         "KeyFile"
#define AFSDIR_ULIST_FILE       "UserList"
#define AFSDIR_NOAUTH_FILE      "NoAuth"
#define AFSDIR_BUDBLOG_FILE     "BackupLog"
#define AFSDIR_TAPECONFIG_FILE  "tapeconfig"
#define AFSDIR_KALOGDB_FILE    	"AuthLog"
#define AFSDIR_KALOG_FILE       "AuthLog"
#define AFSDIR_KADB_FILE        "kaserver"
#define AFSDIR_NTPD_FILE        "ntpd"
#define AFSDIR_PRDB_FILE        "prdb"
#define AFSDIR_PTLOG_FILE       "PtLog"
#define AFSDIR_KCONF_FILE       "krb.conf"
#define AFSDIR_VLDB_FILE        "vldb"
#define AFSDIR_VLOG_FILE        "VLLog"
#define AFSDIR_CORE_FILE        "core"
#define AFSDIR_SLVGLOG_FILE     "SalvageLog"
#define AFSDIR_SALSRVLOG_FILE   "SalsrvLog"
#define AFSDIR_SALVAGER_FILE    "salvager"
#define AFSDIR_SALSRV_FILE      "salvageserver"
#define AFSDIR_SLVGLOCK_FILE    "salvage.lock"
#define AFSDIR_BOZCONF_FILE     "BosConfig"
#define AFSDIR_BOZCONFNEW_FILE  "BosConfig.new"
#define AFSDIR_BOZINIT_FILE     "BozoInit"
#define AFSDIR_BOZLOG_FILE      "BosLog"
#define AFSDIR_BOSVR_FILE       "bosserver"
#define AFSDIR_VOLSERLOG_FILE   "VolserLog"
#define AFSDIR_AUDIT_FILE       "Audit"
#define AFSDIR_KRB_EXCL_FILE    "krb.excl"

#define AFSDIR_ROOTVOL_FILE     "RootVolume"
#define AFSDIR_HOSTDUMP_FILE    "hosts.dump"
#define AFSDIR_CLNTDUMP_FILE    "client.dump"
#define AFSDIR_CBKDUMP_FILE     "callback.dump"
#define AFSDIR_OLDSYSID_FILE    "sysid.old"
#define AFSDIR_SYSID_FILE       "sysid"
#define AFSDIR_FILELOG_FILE     "FileLog"
#define AFSDIR_MIGRATE_LOGNAME  "wtlog."

#define AFSDIR_FSSTATE_FILE     "fsstate.dat"

#ifdef COMMENT
#define AFSDIR_CELLSERVDB_FILE_NTCLIENT  "afsdcell.ini"
#else
#define AFSDIR_CELLSERVDB_FILE_NTCLIENT  AFSDIR_CELLSERVDB_FILE
#endif

#define AFSDIR_NETINFO_FILE     "NetInfo"
#define AFSDIR_NETRESTRICT_FILE "NetRestrict"

#define AFSDIR_LOCALRESIDENCY_FILE "LocalResidency"
#define AFSDIR_WEIGHTINGCONST_FILE "Weight.LocalConstants"
#define AFSDIR_THRESHOLDCONST_FILE "Thershold.LocalConstants"

/* -------------- Canonical (wire-format) path macros -------------- */

/* Each of the following is a canonical form of the corresponding
 * local (AFSDIR_*) path macro.
 */

#define AFSDIR_CANONICAL_USR_DIRPATH            "/usr"
#define AFSDIR_CANONICAL_SERVER_AFS_DIRPATH     "/usr/afs"
#define AFSDIR_CANONICAL_CLIENT_VICE_DIRPATH    "/usr/vice"
#ifdef AFS_DARWIN_ENV
#define AFSDIR_ALTERNATE_CLIENT_VICE_DIRPATH    "/var/db/openafs"
#endif

#define AFSDIR_CANONICAL_SERVER_BIN_DIRPATH \
AFSDIR_CANONICAL_SERVER_AFS_DIRPATH "/" AFSDIR_BIN_DIR

#define AFSDIR_CANONICAL_SERVER_ETC_DIRPATH \
AFSDIR_CANONICAL_SERVER_AFS_DIRPATH "/" AFSDIR_ETC_DIR

#define AFSDIR_CANONICAL_SERVER_LOGS_DIRPATH \
AFSDIR_LOGS_DIR
/* AFSDIR_CANONICAL_SERVER_AFS_DIRPATH "/" AFSDIR_LOGS_DIR */

#define AFSDIR_CANONICAL_SERVER_LOCAL_DIRPATH \
AFSDIR_LOCAL_DIR
/* AFSDIR_CANONICAL_SERVER_AFS_DIRPATH "/" AFSDIR_LOCAL_DIR */

#define AFSDIR_CANONICAL_SERVER_SALVAGER_FILEPATH \
AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/" AFSDIR_SALVAGER_FILE

#define AFSDIR_CANONICAL_SERVER_SALSRV_FILEPATH \
AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/" AFSDIR_SALSRV_FILE

#define AFSDIR_CANONICAL_SERVER_SLVGLOG_FILEPATH \
AFSDIR_CANONICAL_SERVER_LOGS_DIRPATH "/" AFSDIR_SLVGLOG_FILE

#define AFSDIR_CANONICAL_SERVER_SALSRVLOG_FILEPATH \
AFSDIR_CANONICAL_SERVER_LOGS_DIRPATH "/" AFSDIR_SALSRVLOG_FILE


/* ---------------------  Local path macros ---------------------- */

/* Note: On NT, these should be used only after calling initAFSDirPath().
 *       On Unix, the paths are implicitly initialized.
 */

/* enums for indexes into the pathname array */
typedef enum afsdir_id {
    AFSDIR_USR_DIRPATH_ID,
    AFSDIR_SERVER_AFS_DIRPATH_ID,
    AFSDIR_SERVER_ETC_DIRPATH_ID,
    AFSDIR_SERVER_BIN_DIRPATH_ID,
    AFSDIR_SERVER_CORES_DIRPATH_ID,
    AFSDIR_SERVER_DB_DIRPATH_ID,
    AFSDIR_SERVER_LOGS_DIRPATH_ID,
    AFSDIR_SERVER_LOCAL_DIRPATH_ID,
    AFSDIR_SERVER_BACKUP_DIRPATH_ID,
    AFSDIR_CLIENT_VICE_DIRPATH_ID,
    AFSDIR_CLIENT_ETC_DIRPATH_ID,
    AFSDIR_SERVER_THISCELL_FILEPATH_ID,
    AFSDIR_SERVER_CELLSERVDB_FILEPATH_ID,
    AFSDIR_SERVER_NOAUTH_FILEPATH_ID,
    AFSDIR_SERVER_KEY_FILEPATH_ID,
    AFSDIR_SERVER_ULIST_FILEPATH_ID,
    AFSDIR_SERVER_BUDBLOG_FILEPATH_ID,
    AFSDIR_SERVER_TAPECONFIG_FILEPATH_ID,
    AFSDIR_SERVER_KALOGDB_FILEPATH_ID,
    AFSDIR_SERVER_KALOG_FILEPATH_ID,
    AFSDIR_SERVER_KADB_FILEPATH_ID,
    AFSDIR_SERVER_NTPD_FILEPATH_ID,
    AFSDIR_SERVER_PRDB_FILEPATH_ID,
    AFSDIR_SERVER_PTLOG_FILEPATH_ID,
    AFSDIR_SERVER_KCONF_FILEPATH_ID,
    AFSDIR_SERVER_VLDB_FILEPATH_ID,
    AFSDIR_SERVER_VLOG_FILEPATH_ID,
    AFSDIR_SERVER_CORELOG_FILEPATH_ID,
    AFSDIR_SERVER_SLVGLOG_FILEPATH_ID,
    AFSDIR_SERVER_SALVAGER_FILEPATH_ID,
    AFSDIR_SERVER_BOZCONF_FILEPATH_ID,
    AFSDIR_SERVER_BOZCONFNEW_FILEPATH_ID,
    AFSDIR_SERVER_BOZINIT_FILEPATH_ID,
    AFSDIR_SERVER_BOZLOG_FILEPATH_ID,
    AFSDIR_SERVER_BOSVR_FILEPATH_ID,
    AFSDIR_SERVER_SLVGLOCK_FILEPATH_ID,
    AFSDIR_SERVER_VOLSERLOG_FILEPATH_ID,
    AFSDIR_SERVER_ROOTVOL_FILEPATH_ID,
    AFSDIR_SERVER_HOSTDUMP_FILEPATH_ID,
    AFSDIR_SERVER_CLNTDUMP_FILEPATH_ID,
    AFSDIR_SERVER_CBKDUMP_FILEPATH_ID,
    AFSDIR_SERVER_OLDSYSID_FILEPATH_ID,
    AFSDIR_SERVER_SYSID_FILEPATH_ID,
    AFSDIR_SERVER_FILELOG_FILEPATH_ID,
    AFSDIR_SERVER_AUDIT_FILEPATH_ID,
    AFSDIR_CLIENT_THISCELL_FILEPATH_ID,
    AFSDIR_CLIENT_CELLSERVDB_FILEPATH_ID,
    AFSDIR_CLIENT_NETINFO_FILEPATH_ID,
    AFSDIR_CLIENT_NETRESTRICT_FILEPATH_ID,
    AFSDIR_SERVER_NETINFO_FILEPATH_ID,
    AFSDIR_SERVER_NETRESTRICT_FILEPATH_ID,
    AFSDIR_SERVER_WEIGHTING_CONSTANTS_FILEPATH_ID,
    AFSDIR_SERVER_THRESHOLD_CONSTANTS_FILEPATH_ID,
    AFSDIR_SERVER_MIGRATE_DIRPATH_ID,
    AFSDIR_SERVER_MIGRATELOG_FILEPATH_ID,
    AFSDIR_SERVER_BIN_FILE_DIRPATH_ID,
    AFSDIR_CLIENT_CELLALIAS_FILEPATH_ID,
    AFSDIR_SERVER_KRB_EXCL_FILEPATH_ID,
    AFSDIR_SERVER_SALSRV_FILEPATH_ID,
    AFSDIR_SERVER_SALSRVLOG_FILEPATH_ID,
    AFSDIR_SERVER_FSSTATE_FILEPATH_ID,
    AFSDIR_PATHSTRING_MAX
} afsdir_id_t;

/* getDirPath() returns a pointer to a string from an internal array of path strings 
 */
const char *getDirPath(afsdir_id_t string_id);

/* Top level usr dir */
#define AFSDIR_USR_DIRPATH getDirPath(AFSDIR_USR_DIRPATH_ID)

/* server subdir paths */
#define AFSDIR_SERVER_AFS_DIRPATH getDirPath(AFSDIR_SERVER_AFS_DIRPATH_ID)
#define AFSDIR_SERVER_ETC_DIRPATH getDirPath(AFSDIR_SERVER_ETC_DIRPATH_ID)
#define AFSDIR_SERVER_BIN_DIRPATH getDirPath(AFSDIR_SERVER_BIN_DIRPATH_ID)
#define AFSDIR_SERVER_CORES_DIRPATH getDirPath(AFSDIR_SERVER_CORES_DIRPATH_ID)
#define AFSDIR_SERVER_DB_DIRPATH getDirPath(AFSDIR_SERVER_DB_DIRPATH_ID)
#define AFSDIR_SERVER_LOGS_DIRPATH getDirPath(AFSDIR_SERVER_LOGS_DIRPATH_ID)
#define AFSDIR_SERVER_LOCAL_DIRPATH getDirPath(AFSDIR_SERVER_LOCAL_DIRPATH_ID)
#define AFSDIR_SERVER_BACKUP_DIRPATH getDirPath(AFSDIR_SERVER_BACKUP_DIRPATH_ID)
#define AFSDIR_SERVER_LOCAL_DIRPATH getDirPath(AFSDIR_SERVER_LOCAL_DIRPATH_ID)
#define AFSDIR_SERVER_MIGRATE_DIRPATH getDirPath(AFSDIR_SERVER_MIGRATE_DIRPATH_ID)
#define AFSDIR_SERVER_MIGRATE_DIRPATH getDirPath(AFSDIR_SERVER_MIGRATE_DIRPATH_ID)

/* client subdir paths */
#define AFSDIR_CLIENT_VICE_DIRPATH getDirPath(AFSDIR_CLIENT_VICE_DIRPATH_ID)
#define AFSDIR_CLIENT_ETC_DIRPATH getDirPath(AFSDIR_CLIENT_ETC_DIRPATH_ID)
#define AFSDIR_SERVER_BIN_FILE_DIRPATH getDirPath(AFSDIR_SERVER_BIN_FILE_DIRPATH_ID)

/* server file paths */
#define AFSDIR_SERVER_THISCELL_FILEPATH getDirPath(AFSDIR_SERVER_THISCELL_FILEPATH_ID)
#define AFSDIR_SERVER_CELLSERVDB_FILEPATH getDirPath(AFSDIR_SERVER_CELLSERVDB_FILEPATH_ID)
#define AFSDIR_SERVER_NOAUTH_FILEPATH getDirPath(AFSDIR_SERVER_NOAUTH_FILEPATH_ID)
#define AFSDIR_SERVER_KEY_FILEPATH getDirPath(AFSDIR_SERVER_KEY_FILEPATH_ID)
#define AFSDIR_SERVER_ULIST_FILEPATH getDirPath(AFSDIR_SERVER_ULIST_FILEPATH_ID)
#define AFSDIR_SERVER_BUDBLOG_FILEPATH getDirPath(AFSDIR_SERVER_BUDBLOG_FILEPATH_ID)
#define AFSDIR_SERVER_TAPECONFIG_FILEPATH getDirPath(AFSDIR_SERVER_TAPECONFIG_FILEPATH_ID)
#define AFSDIR_SERVER_KALOGDB_FILEPATH getDirPath(AFSDIR_SERVER_KALOGDB_FILEPATH_ID)
#define AFSDIR_SERVER_KALOG_FILEPATH getDirPath(AFSDIR_SERVER_KALOG_FILEPATH_ID)
#define AFSDIR_SERVER_KADB_FILEPATH getDirPath(AFSDIR_SERVER_KADB_FILEPATH_ID)
#define AFSDIR_SERVER_NTPD_FILEPATH getDirPath(AFSDIR_SERVER_NTPD_FILEPATH_ID)
#define AFSDIR_SERVER_PRDB_FILEPATH getDirPath(AFSDIR_SERVER_PRDB_FILEPATH_ID)
#define AFSDIR_SERVER_PTLOG_FILEPATH getDirPath(AFSDIR_SERVER_PTLOG_FILEPATH_ID)
#define AFSDIR_SERVER_KCONF_FILEPATH getDirPath(AFSDIR_SERVER_KCONF_FILEPATH_ID)
#define AFSDIR_SERVER_VLDB_FILEPATH getDirPath(AFSDIR_SERVER_VLDB_FILEPATH_ID)
#define AFSDIR_SERVER_VLOG_FILEPATH getDirPath(AFSDIR_SERVER_VLOG_FILEPATH_ID)
#define AFSDIR_SERVER_CORELOG_FILEPATH getDirPath(AFSDIR_SERVER_CORELOG_FILEPATH_ID)
#define AFSDIR_SERVER_SLVGLOG_FILEPATH getDirPath(AFSDIR_SERVER_SLVGLOG_FILEPATH_ID)
#define AFSDIR_SERVER_SALSRVLOG_FILEPATH getDirPath(AFSDIR_SERVER_SALSRVLOG_FILEPATH_ID)
#define AFSDIR_SERVER_SALVAGER_FILEPATH getDirPath(AFSDIR_SERVER_SALVAGER_FILEPATH_ID)
#define AFSDIR_SERVER_SALSRV_FILEPATH getDirPath(AFSDIR_SERVER_SALSRV_FILEPATH_ID)
#define AFSDIR_SERVER_BOZCONF_FILEPATH getDirPath(AFSDIR_SERVER_BOZCONF_FILEPATH_ID)
#define AFSDIR_SERVER_BOZCONFNEW_FILEPATH getDirPath(AFSDIR_SERVER_BOZCONFNEW_FILEPATH_ID)
#define AFSDIR_SERVER_BOZINIT_FILEPATH getDirPath(AFSDIR_SERVER_BOZINIT_FILEPATH_ID)
#define AFSDIR_SERVER_BOZLOG_FILEPATH getDirPath(AFSDIR_SERVER_BOZLOG_FILEPATH_ID)
#define AFSDIR_SERVER_BOSVR_FILEPATH getDirPath(AFSDIR_SERVER_BOSVR_FILEPATH_ID)
#define AFSDIR_SERVER_SLVGLOCK_FILEPATH getDirPath(AFSDIR_SERVER_SLVGLOCK_FILEPATH_ID)
#define AFSDIR_SERVER_VOLSERLOG_FILEPATH getDirPath(AFSDIR_SERVER_VOLSERLOG_FILEPATH_ID)
#define AFSDIR_SERVER_ROOTVOL_FILEPATH getDirPath(AFSDIR_SERVER_ROOTVOL_FILEPATH_ID)
#define AFSDIR_SERVER_HOSTDUMP_FILEPATH getDirPath(AFSDIR_SERVER_HOSTDUMP_FILEPATH_ID)
#define AFSDIR_SERVER_CLNTDUMP_FILEPATH getDirPath(AFSDIR_SERVER_CLNTDUMP_FILEPATH_ID)
#define AFSDIR_SERVER_CBKDUMP_FILEPATH getDirPath(AFSDIR_SERVER_CBKDUMP_FILEPATH_ID)
#define AFSDIR_SERVER_OLDSYSID_FILEPATH getDirPath(AFSDIR_SERVER_OLDSYSID_FILEPATH_ID)
#define AFSDIR_SERVER_SYSID_FILEPATH getDirPath(AFSDIR_SERVER_SYSID_FILEPATH_ID)
#define AFSDIR_SERVER_FILELOG_FILEPATH getDirPath(AFSDIR_SERVER_FILELOG_FILEPATH_ID)
#define AFSDIR_SERVER_AUDIT_FILEPATH getDirPath(AFSDIR_SERVER_AUDIT_FILEPATH_ID)
#define AFSDIR_SERVER_NETINFO_FILEPATH getDirPath(AFSDIR_SERVER_NETINFO_FILEPATH_ID)
#define AFSDIR_SERVER_NETRESTRICT_FILEPATH getDirPath(AFSDIR_SERVER_NETRESTRICT_FILEPATH_ID)
#define AFSDIR_SERVER_WEIGHTING_CONSTANTS_FILEPATH getDirPath(AFSDIR_SERVER_WEIGHTING_CONSTANTS_FILEPATH_ID)
#define AFSDIR_SERVER_THRESHOLD_CONSTANTS_FILEPATH getDirPath(AFSDIR_SERVER_THRESHOLD_CONSTANTS_FILEPATH_ID)
#define AFSDIR_SERVER_MIGRATELOG_FILEPATH getDirPath(AFSDIR_SERVER_MIGRATELOG_FILEPATH_ID)
#define AFSDIR_SERVER_KRB_EXCL_FILEPATH getDirPath(AFSDIR_SERVER_KRB_EXCL_FILEPATH_ID)
#define AFSDIR_SERVER_FSSTATE_FILEPATH getDirPath(AFSDIR_SERVER_FSSTATE_FILEPATH_ID)

/* client file paths */
#define AFSDIR_CLIENT_THISCELL_FILEPATH getDirPath(AFSDIR_CLIENT_THISCELL_FILEPATH_ID)
#define AFSDIR_CLIENT_CELLSERVDB_FILEPATH getDirPath(AFSDIR_CLIENT_CELLSERVDB_FILEPATH_ID)
#define AFSDIR_CLIENT_NETINFO_FILEPATH getDirPath(AFSDIR_CLIENT_NETINFO_FILEPATH_ID)
#define AFSDIR_CLIENT_NETRESTRICT_FILEPATH getDirPath(AFSDIR_CLIENT_NETRESTRICT_FILEPATH_ID)

#endif /* _DIRPATH_H */
