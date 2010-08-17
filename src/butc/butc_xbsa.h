/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_BUTC_XBSA_H
#define OPENAFS_BUTC_XBSA_H

/* The following serverTypes are currently supported by this interface */
#define XBSA_SERVER_TYPE_NONE       0x00	/* no server, use tape drive         */
#define XBSA_SERVER_TYPE_UNKNOWN    0x01	/* server, don't know which type yet */
#define XBSA_SERVER_TYPE_ADSM       0x02	/* server type is ADSM               */
#define XBSA_SERVER_TYPE_MASK       0xFF	/* for (butx_transactionInfo *)->serverType (byte 1) */

#ifdef xbsa
#define CONF_XBSA (xbsaType != XBSA_SERVER_TYPE_NONE)	/*test if butc is XBSA */
#else
#define CONF_XBSA 0
#endif

#ifdef xbsa
#define XBSA_SET_SERVER_TYPE(server, type) ((server) = ((type) & XBSA_SERVER_TYPE_MASK))
#define XBSA_GET_SERVER_TYPE(server)       ((server) & XBSA_SERVER_TYPE_MASK)

/* The following serverType flags are currently supported by this interface */
#define XBSA_SERVER_FLAG_NONE      0x0000	/* don't allow multiple server connections */
#define XBSA_SERVER_FLAG_MULTIPLE  0x0100	/* allow multiple server connections */
#define XBSA_SERVER_FLAG_MASK      0xFF00	/* for (butx_transactionInfo *)->serverType (byte 2) */
#define XBSA_SET_SERVER_FLAG(server, flag)    ((server) |= ((flag) & XBSA_SERVER_FLAG_MASK))
#define XBSA_GET_SERVER_FLAG(server)          ((server) & XBSA_SERVER_FLAG_MASK)
#define XBSA_IS_SERVER_FLAG_SET(server, flag) (XBSA_GET_SERVER_FLAG(server) & flag)

#define XBSAMINBUFFER  1024	/* minimum size is 1KB */
#define XBSADFLTBUFFER 16384	/* default size is 16KB */
#define XBSAMAXBUFFER  65535	/* maximum size in 64KB-1; has to fit in 16bit integer */

#ifdef NEW_XBSA
#include "afsxbsa.h"
#else
#include <xbsa.h>
#endif

#define XBSA_NUM_ENV_STRS ADSM_ENV_STRS
#define XBSA_MAX_OSNAME   BSA_MAX_OSNAME
#define XBSA_MAX_PATHNAME BSA_MAX_PATHNAME

#define XBSA_SUCCESS 0

/* The following defines the ADSM version level prior to the addition
 * of support for multiple servers.
 */
#define XBSA_ADSM_NO_MULT_SERVER_VERSION 3
#define XBSA_ADSM_NO_MULT_SERVER_RELEASE 7
#define XBSA_ADSM_NO_MULT_SERVER_LEVEL   1

/* The following defines the XBSA Technical Standard Level */
#define XBSA_TS_VERSION 1
#define XBSA_TS_RELEASE 1

/*
 * The butx_transactionInfo structure defines the connection to an
 * XBSA server.  The fields in this structure should only be modified
 * by the routines in file_xbsa.c.
 *
 * The values in here are specific to the transaction.
 * Values specific to the objects should be passed separately.
 * The spec says bsaHandle should be a long but ADSM has it as a ulong!
 */
struct butx_transactionInfo {
    ApiVersion apiVersion;
    u_long bsaHandle;
    afs_int32 serverType;	/* Type and flags           */
    afs_int32 maxObjects;	/* max objects/transaction  */
    afs_int32 numObjects;	/* objects in current trans */
    char serverName[BSA_MAX_DESC];
    SecurityToken secToken;
    ObjectOwner objOwner;
    ObjectDescriptor curObject;
};

extern afs_int32 xbsa_MountLibrary(struct butx_transactionInfo *info,
				   afs_int32 serverType);

extern afs_int32 xbsa_Initialize(struct butx_transactionInfo *info,
				 char *bsaObjectOwner, char *appObjectOwner,
				 char *secToken, char *serverName);

extern afs_int32 xbsa_Finalize(struct butx_transactionInfo *info);

extern afs_int32 xbsa_BeginTrans(struct butx_transactionInfo *info);

extern afs_int32 xbsa_EndTrans(struct butx_transactionInfo *info);

extern afs_int32 xbsa_QueryObject(struct butx_transactionInfo *info,
				  char *objectSpaceName, char *pathName);

extern afs_int32 xbsa_ReadObjectBegin(struct butx_transactionInfo *info,
				      char *dataBuffer, afs_int32 bufferSize,
				      afs_int32 * count,
				      afs_int32 * endOfData);

extern afs_int32 xbsa_ReadObjectEnd(struct butx_transactionInfo *info);

extern afs_int32 xbsa_WriteObjectBegin(struct butx_transactionInfo *info,
				       char *objectSpaceName, char *pathName,
				       char *lGName,
				       afs_hyper_t estimatedSize,
				       char *objectDescription,
				       char *objectInfo);

extern afs_int32 xbsa_WriteObjectEnd(struct butx_transactionInfo *info);

extern afs_int32 xbsa_WriteObjectData(struct butx_transactionInfo *info,
				      char *dataBuffer, afs_int32 bufferSize,
				      afs_int32 * count);

extern afs_int32 xbsa_ReadObjectData(struct butx_transactionInfo *info,
				     char *dataBuffer, afs_int32 bufferSize,
				     afs_int32 * count,
				     afs_int32 * endOfData);

extern afs_int32 xbsa_DeleteObject(struct butx_transactionInfo *info,
				   char *objectSpaceName, char *pathName);

#endif /*xbsa */


/* XBSA Global Parameters */

#ifdef XBSA_TCMAIN
#define XBSA_EXT
#else
#define XBSA_EXT extern
#endif

XBSA_EXT afs_int32 xbsaType;
#ifdef xbsa
XBSA_EXT struct butx_transactionInfo butxInfo;

#define rpc_c_protect_level_default 0
XBSA_EXT afs_uint32 dumpRestAuthnLevel;
XBSA_EXT char *xbsaObjectOwner;
XBSA_EXT char *appObjectOwner;
XBSA_EXT char *adsmServerName;
XBSA_EXT char *xbsaSecToken;
XBSA_EXT char *xbsalGName;
#endif /*xbsa*/
#endif /* OPENAFS_BUTC_XBSA_H */
