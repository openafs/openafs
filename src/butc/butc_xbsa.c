/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifdef xbsa

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <afs/stds.h>
#include <stdio.h>
#if defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV)
#include <dlfcn.h>
#endif
#include <errno.h>
#include "butc_xbsa.h"
#include <afs/butx.h>
#include <afs/bubasics.h>

#include "error_macros.h"

extern int debugLevel;


char resourceType[20] = "LFS FILE SYSTEM";
#define GOODSTR(s) ((s)?(s):"<NULL>")

BSA_Int16(*XBSAInit) (BSA_UInt32 *, SecurityToken *, ObjectOwner *, char **);
BSA_Int16(*XBSABeginTxn) (BSA_UInt32);
BSA_Int16(*XBSAEndTxn) (BSA_UInt32, Vote);
BSA_Int16(*XBSATerminate) (BSA_UInt32);
BSA_Int16(*XBSAQueryObject) (BSA_UInt32, QueryDescriptor *,
			     ObjectDescriptor *);
BSA_Int16(*XBSAGetObject) (BSA_UInt32, ObjectDescriptor *, DataBlock *);
BSA_Int16(*XBSAEndData) (BSA_UInt32);
BSA_Int16(*XBSACreateObject) (BSA_UInt32, ObjectDescriptor *, DataBlock *);
BSA_Int16(*XBSADeleteObject) (BSA_UInt32, CopyType, ObjectName *, CopyId *);
BSA_Int16(*XBSAMarkObjectInactive) (BSA_UInt32, ObjectName *);
BSA_Int16(*XBSASendData) (BSA_UInt32, DataBlock *);
BSA_Int16(*XBSAGetData) (BSA_UInt32, DataBlock *);
BSA_Int16(*XBSAQueryApiVersion) (ApiVersion *);
BSA_Int16(*XBSAGetEnvironment) (BSA_UInt32, ObjectOwner *, char **);


xbsa_error(int rc, struct butx_transactionInfo *info)
{
    switch (rc) {
    case BSA_RC_AUTHENTICATION_FAILURE:
	ELog(0, "     XBSA: Authentication failure\n");
	break;
    case BSA_RC_INVALID_KEYWORD:
	ELog(0, "     XBSA: A specified keyword is invalid\n");
	break;
    case BSA_RC_TOKEN_EXPIRED:
	ELog(0, "     XBSA: The security token has expired\n");
	break;
    case ADSM_RC_PSWD_GEN:
	if (XBSA_GET_SERVER_TYPE(info->serverType) == XBSA_SERVER_TYPE_ADSM) {
	    ELog(0, "     XBSA: Password generation is not supported\n");
	}
	break;
    case BSA_RC_BAD_HANDLE:
	ELog(0, "     XBSA: The handle is invalid, %d\n", info->bsaHandle);
	break;
    case BSA_RC_NO_MATCH:
	ELog(0,
	     "     XBSA: There were no matches found for the specified object\n");
	break;
    case BSA_RC_MORE_DATA:
	ELog(0, "     XBSA: There were more matches found than expected\n");
	break;
    case BSA_RC_NULL_OBJNAME:
	ELog(0, "     XBSA: The object name is null\n");
	break;
    case BSA_RC_OBJNAME_TOO_LONG:
	ELog(0, "     XBSA: The object name was longer than expected\n");
	break;
    case BSA_RC_DESC_TOO_LONG:
	ELog(0,
	     "     XBSA: The description string was longer than expected\n");
	break;
    case BSA_RC_OBJINFO_TOO_LONG:
	ELog(0,
	     "     XBSA: The object info string was longer than expected\n");
	break;
    case BSA_RC_ABORT_ACTIVE_NOT_FOUND:
	ELog(0, "     XBSA: The specified object was not found\n");
	break;
    case BSA_RC_NULL_DATABLKPTR:
	ELog(0, "     XBSA: The dataBlockPtr is null\n");
	break;
    case BSA_RC_INVALID_VOTE:
	ELog(0, "     XBSA: The vote variable is invalid\n");
	break;
    }
}

/*
 * Hook into the correct XBSA shared library.
 * Set up the function pointers.
 * Get the library version.
 * XBSAQueryApiVersion
 */
afs_int32
xbsa_MountLibrary(struct butx_transactionInfo *info, afs_int32 serverType)
{
    void *dynlib;
    int rc;

    if (debugLevel > 98) {
	printf("\nxbsa_MountLibraray\n");
    }

    switch (serverType) {
    case XBSA_SERVER_TYPE_ADSM:
#if defined(AFS_AIX_ENV)
	dynlib =
	    dlopen("/usr/lib/libXApi.a(bsashr10.o)",
		   RTLD_NOW | RTLD_LOCAL | RTLD_MEMBER);
	if (dynlib == NULL) {
	    dynlib =
		dlopen("/usr/lib/libXApi.a(xbsa.o)",
		       RTLD_NOW | RTLD_LOCAL | RTLD_MEMBER);
	}
#elif defined(AFS_SUN5_ENV)
	dlopen ("/usr/lib/libCstd.so.1", RTLD_NOW | RTLD_GLOBAL);
	dynlib = dlopen("/usr/lib/libXApi.so", RTLD_NOW | RTLD_GLOBAL);
#else
	dynlib = NULL;
#endif
	break;
    default:
	ELog(0, "xbsa_MountLibrary: The serverType %d is not recognized\n",
	     serverType);
	return (BUTX_INVALIDSERVERTYPE);
	break;
    }

    if (dynlib == NULL) {
	ELog(0,
	     "xbsa_MountLibrary: The dlopen call to load the XBSA shared library failed\n");
	return (BUTX_NOLIBRARY);
    }

    memset(info, 0, sizeof(struct butx_transactionInfo));
    XBSA_SET_SERVER_TYPE(info->serverType, serverType);

#if defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV)
    XBSAInit = (BSA_Int16(*)
		(BSA_UInt32 *, SecurityToken *, ObjectOwner *,
		 char **))dlsym((void *)dynlib, "BSAInit");
    XBSABeginTxn =
	(BSA_Int16(*)(BSA_UInt32)) dlsym((void *)dynlib, "BSABeginTxn");
    XBSAEndTxn =
	(BSA_Int16(*)(BSA_UInt32, Vote)) dlsym((void *)dynlib, "BSAEndTxn");
    XBSATerminate =
	(BSA_Int16(*)(BSA_UInt32)) dlsym((void *)dynlib, "BSATerminate");
    XBSAQueryObject =
	(BSA_Int16(*)(BSA_UInt32, QueryDescriptor *, ObjectDescriptor *))
	dlsym((void *)dynlib, "BSAQueryObject");
    XBSAGetObject =
	(BSA_Int16(*)(BSA_UInt32, ObjectDescriptor *, DataBlock *))
	dlsym((void *)dynlib, "BSAGetObject");
    XBSAEndData =
	(BSA_Int16(*)(BSA_UInt32)) dlsym((void *)dynlib, "BSAEndData");
    XBSACreateObject =
	(BSA_Int16(*)(BSA_UInt32, ObjectDescriptor *, DataBlock *))
	dlsym((void *)dynlib, "BSACreateObject");
    XBSAMarkObjectInactive =
	(BSA_Int16(*)(BSA_UInt32, ObjectName *)) dlsym((void *)dynlib,
						       "BSAMarkObjectInactive");
    XBSADeleteObject =
	(BSA_Int16(*)(BSA_UInt32, CopyType, ObjectName *, CopyId *))
	dlsym((void *)dynlib, "BSADeleteObject");
    XBSASendData =
	(BSA_Int16(*)(BSA_UInt32, DataBlock *)) dlsym((void *)dynlib,
						      "BSASendData");
    XBSAGetData =
	(BSA_Int16(*)(BSA_UInt32, DataBlock *)) dlsym((void *)dynlib,
						      "BSAGetData");
    XBSAQueryApiVersion =
	(BSA_Int16(*)(ApiVersion *)) dlsym((void *)dynlib,
					   "BSAQueryApiVersion");
    XBSAGetEnvironment =
	(BSA_Int16(*)(BSA_UInt32, ObjectOwner *, char **))dlsym((void *)
								dynlib,
								"BSAGetEnvironment");

    if (!XBSAInit || !XBSABeginTxn || !XBSAEndTxn || !XBSATerminate
	|| !XBSAQueryObject || !XBSAGetObject || !XBSAEndData
	|| !XBSACreateObject || !XBSADeleteObject || !XBSAMarkObjectInactive
	|| !XBSASendData || !XBSAGetData || !XBSAQueryApiVersion
	|| !XBSAGetEnvironment) {
	ELog(0,
	     "xbsa_MountLibrary: The dlopen call to load the XBSA shared library failed\n");
	return (BUTX_NOLIBRARY);
    }

    XBSAQueryApiVersion(&(info->apiVersion));
#endif

    /*
     * Verify the API version
     */
    if ((info->apiVersion.version == XBSA_TS_VERSION)
	&& (info->apiVersion.release == XBSA_TS_RELEASE)) {
	/* XOPEN Techincal Standard Level! 
	 * We are coded to the Preliminary Spec!  Time to go boom!
	 */
	ELog(0,
	     "xbsa_MountLibrary: The XBSAQueryApiVersion call returned an incompatible version, %d %d %d\n",
	     info->apiVersion.version, info->apiVersion.release,
	     info->apiVersion.level);
	return (BUTX_INVALIDVERSION);
    }

    switch (XBSA_GET_SERVER_TYPE(info->serverType)) {
    case XBSA_SERVER_TYPE_ADSM:
	if ((info->apiVersion.version > XBSA_ADSM_NO_MULT_SERVER_VERSION)
	    || (info->apiVersion.version == XBSA_ADSM_NO_MULT_SERVER_VERSION
		&& info->apiVersion.release >
		XBSA_ADSM_NO_MULT_SERVER_RELEASE)
	    || (info->apiVersion.version == XBSA_ADSM_NO_MULT_SERVER_VERSION
		&& info->apiVersion.release ==
		XBSA_ADSM_NO_MULT_SERVER_RELEASE
		&& info->apiVersion.level > XBSA_ADSM_NO_MULT_SERVER_LEVEL)) {

	    /* This version contains the fixes to allow multiple servers */
	    info->serverType |= XBSA_SERVER_FLAG_MULTIPLE;
	} else {
	    /* This is an older version of ADSM prior to the fix to
	     * allow multiple servers.
	     */
	    info->serverType |= XBSA_SERVER_FLAG_NONE;
	}
	break;
    }

    return (XBSA_SUCCESS);

}

/*
 * Set up the connection to the XBSA Server.
 * BSAInit
 * BSAGetEnvironment
 */
afs_int32
xbsa_Initialize(struct butx_transactionInfo * info, char *bsaObjectOwner,
		char *appObjectOwner, char *secToken, char *serverName)
{
    char envStrs[XBSA_NUM_ENV_STRS][BSA_MAX_DESC];
    char *envP[XBSA_NUM_ENV_STRS + 1];
    char *ADSMMaxObject = "TSMMAXOBJ=";
    char *ADSMServer = "TSMSRVR=";
    char *tempStrPtr;
    int i;
    int rc;

    if (debugLevel > 98) {
	printf("\nxbsa_Initialize bsaObjectOwner='%s' appObjectOwner='%s' "
	       "secToken=xxxxxx servername='%s'\n", GOODSTR(bsaObjectOwner),
	       GOODSTR(appObjectOwner), GOODSTR(serverName));
    }

    if (info->bsaHandle != 0) {
	/* We already have a handle */
	ELog(0,
	     "xbsa_Initialize: The dlopen call to load the XBSA shared library failed\n");
	return (BUTX_ILLEGALINIT);
    }

    /*
     * The XBSAGetEnvironment function is supposed to return the
     * the serverName to use.  However, it is returning the tcpserveraddress
     * instead.  So, we can't count on this function to properly fill it
     * in.  So, until that get fixed.  The serverName will have to be filled
     * in by the callers of this function (butc).
     */

    /* Initialize the environment strings */
    for (i = 0; i < XBSA_NUM_ENV_STRS; i++)
	envP[i] = envStrs[i];
    envP[XBSA_NUM_ENV_STRS] = NULL;

    /* The environment variables are specific to the server type */
    switch (XBSA_GET_SERVER_TYPE(info->serverType)) {
    case XBSA_SERVER_TYPE_ADSM:
	if (serverName) {
	    if (strlen(serverName) >= BSA_MAX_DESC) {
		ELog(0,
		     "xbsa_Initialize: The serverName was not specified\n");
		return (BUTX_INVALIDSERVERNAME);
	    }
	    strcpy(info->serverName, serverName);
	    strcpy(envP[0], ADSMServer);
	    tempStrPtr = envP[0];
	    tempStrPtr = tempStrPtr + strlen(ADSMServer);
	    strcat(tempStrPtr, serverName);
	    envP[1] = NULL;
	    envP[0] = NULL;     /* Hack for TSM V5 */
	} else {
	    envP[0] = NULL;
	    ELog(0, "xbsa_Initialize: The serverName was not specified\n");
	    return (BUTX_INVALIDSERVERNAME);
	}
	break;
    default:
	ELog(0, "xbsa_Initialize: The serverType %d is not recognized\n",
	     XBSA_GET_SERVER_TYPE(info->serverType));
	return (BUTX_INVALIDSERVERTYPE);
	break;
    }

    if (bsaObjectOwner) {
	if (strlen(bsaObjectOwner) >= BSA_MAX_BSAOBJECT_OWNER) {
	    ELog(0,
		 "xbsa_Initialize: The bsaObjectOwner is too long; size = %d; name = %s\n",
		 strlen(bsaObjectOwner), bsaObjectOwner);
	    return (BUTX_INVALIDBSANAME);
	}
	strcpy(info->objOwner.bsaObjectOwner, bsaObjectOwner);
    } else {
	info->objOwner.bsaObjectOwner[0] = NULL;
    }

    if (appObjectOwner) {
	if (strlen(appObjectOwner) >= BSA_MAX_APPOBJECT_OWNER) {
	    ELog(0,
		 "xbsa_Initialize: The appObjectOwner is too long; size = %d; name = %s\n",
		 strlen(appObjectOwner), appObjectOwner);
	    return (BUTX_INVALIDAPPNAME);
	}
	strcpy(info->objOwner.appObjectOwner, appObjectOwner);
    } else {
	info->objOwner.appObjectOwner[0] = NULL;
    }

    if (secToken) {
	if (strlen(secToken) >= BSA_MAX_TOKEN_SIZE) {
	    ELog(0,
		 "xbsa_Initialize: The secToken is too long; size = %d; name = %s\n",
		 strlen(secToken), secToken);
	    return (BUTX_INVALIDSECTOKEN);
	}
	strcpy(info->secToken, secToken);
    } else {
	info->secToken[0] = NULL;
    }

    rc = (int)XBSAInit(&(info->bsaHandle), &(info->secToken),
		       &(info->objOwner), envP);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "xbsa_Initialize: The XBSAInit call failed with %d\n", rc);
	xbsa_error(rc, info);
	return (BUTX_INITFAIL);
    }

    /* Initialize the environment strings */
    for (i = 0; i < XBSA_NUM_ENV_STRS; i++)
	envP[i] = envStrs[i];
    envP[XBSA_NUM_ENV_STRS] = NULL;

    rc = (int)XBSAGetEnvironment(info->bsaHandle, &info->objOwner, envP);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0,
	     "xbsa_Initialize: The XBSAGetEnvironment call failed with %d\n",
	     rc);
	xbsa_error(rc, info);
	return (BUTX_GETENVFAIL);
    }

 info->maxObjects = 255; /* Hack for ADSM V5: unclear what this actually means... */

    switch (XBSA_GET_SERVER_TYPE(info->serverType)) {
    case XBSA_SERVER_TYPE_ADSM:
	for (i = 0; i < XBSA_NUM_ENV_STRS; i++) {
	    if (strncmp(envP[i], ADSMMaxObject, sizeof(ADSMMaxObject)) == 0) {
		tempStrPtr = envP[i];
		tempStrPtr = tempStrPtr + strlen(ADSMMaxObject);
		info->maxObjects = strtol(tempStrPtr, NULL, 10);
		if (info->maxObjects <= 0) {
		    ELog(0,
			 "xbsa_Initialize: The XBSAGetEnvironment call returned an invalid value for MAXOBJ %d\n",
			 info->maxObjects);
		    return (BUTX_GETENVFAIL);
		}
	    }
	}
	if (info->maxObjects == 0) {
	    ELog(0,
		 "xbsa_Initialize: The XBSAGetEnvironment call failed to return the MAXOBJ string\n");
	    return (BUTX_GETENVFAIL);
	}
	break;
    }

    return (XBSA_SUCCESS);
}

/*
 * Create a transaction
 * BSABeginTxn
 */
afs_int32
xbsa_BeginTrans(struct butx_transactionInfo * info)
{
    int rc;

    if (debugLevel > 98) {
	printf("\nxbsa_BeginTrans\n");
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0, "xbsa_BeginTrans: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    rc = (int)XBSABeginTxn(info->bsaHandle);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "xbsa_BeginTrans: The XBSABeginTxn call failed with %d\n",
	     rc);
	xbsa_error(rc, info);
	return (BUTX_BEGINTXNFAIL);
    }

    info->numObjects = 0;
    return (XBSA_SUCCESS);
}

/*
 * End the current transaction
 * BSAEndTxn
 */
afs_int32
xbsa_EndTrans(struct butx_transactionInfo * info)
{
    int rc;

    if (debugLevel > 98) {
	printf("\nxbsa_EndTrans\n");
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0, "xbsa_EndTrans: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    /* terminate the transaction */
    rc = (int)XBSAEndTxn(info->bsaHandle, BSAVote_COMMIT);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "xbsa_EndTrans: The XBSAEndTxn call failed with %d\n", rc);
	xbsa_error(rc, info);
	return (BUTX_ENDTXNFAIL);
    }
    return (XBSA_SUCCESS);
}

/*
 * Terminate the connection to the XBSA Server.
 * End the transaction.
 * BSATerminate
 */
afs_int32
xbsa_Finalize(struct butx_transactionInfo * info)
{
    int rc;

    if (debugLevel > 98) {
	printf("\nxbsa_Finalize\n");
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0, "xbsa_Finalize: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    /* terminate the session */
    rc = (int)XBSATerminate(info->bsaHandle);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "The XBSATerminate call failed with %d\n", rc);
	xbsa_error(rc, info);
	return (BUTX_TERMFAIL);
    }

    info->bsaHandle = 0;
    return (XBSA_SUCCESS);
}

/*
 * Query for the object we are looking for.
 * BSAQueryObject
 */
afs_int32
xbsa_QueryObject(struct butx_transactionInfo * info, char *objectSpaceName,
		 char *pathName)
{
    int rc;
    QueryDescriptor queryDescriptor;

    if (debugLevel > 98) {
	printf("\nxbsa_QueryObject objectSpaceName='%s' pathnName='%s'\n",
	       GOODSTR(objectSpaceName), GOODSTR(pathName));
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_QueryObject: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    /* Initialize the query for our dump name */

    if (objectSpaceName) {
	if (strlen(objectSpaceName) >= BSA_MAX_OSNAME) {
	    ELog(0,
		 "xbsa_QueryObject: The objectSpaceName is too long; size = %d; name = %s\n",
		 strlen(objectSpaceName), objectSpaceName);
	    return (BUTX_INVALIDOBJECTSPNAME);
	}
	strcpy(queryDescriptor.objName.objectSpaceName, objectSpaceName);
    } else {
	queryDescriptor.objName.objectSpaceName[0] = NULL;
    }

    if (pathName) {
	if (strlen(pathName) >= BSA_MAX_PATHNAME) {
	    ELog(0,
		 "xbsa_QueryObject: The pathName is too long; size = %d; name = %s\n",
		 strlen(pathName), pathName);
	    return (BUTX_INVALIDPATHNAME);
	}
	strcpy(queryDescriptor.objName.pathName, pathName);
    } else {
	queryDescriptor.objName.pathName[0] = NULL;
    }

    queryDescriptor.owner = info->objOwner;
    queryDescriptor.copyType = BSACopyType_BACKUP;
    strcpy(queryDescriptor.lGName, "");
    strcpy(queryDescriptor.cGName, "");
    strcpy(queryDescriptor.resourceType, "");
    queryDescriptor.objectType = BSAObjectType_FILE;
    queryDescriptor.status = BSAObjectStatus_ACTIVE;
    strcpy(queryDescriptor.desc, "");

    rc = (int)XBSAQueryObject(info->bsaHandle, &queryDescriptor,
			      &info->curObject);
    if (rc == BSA_RC_NO_MORE_DATA)
	rc = BSA_RC_SUCCESS;	/* This is actually a success! */
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "xbsa_QueryObject: The xBSAQueryObject call failed with %d\n",
	     rc);
	xbsa_error(rc, info);
	return (BUTX_QUERYFAIL);
    }
    return (XBSA_SUCCESS);
}

/*
 * Locate the correct object on the server and then make the call to
 * get the object descriptor so we can begin the transfer of data.
 * BSAGetObject
 */
afs_int32
xbsa_ReadObjectBegin(struct butx_transactionInfo * info, char *dataBuffer,
		     afs_int32 bufferSize, afs_int32 * count,
		     afs_int32 * endOfData)
{
    int rc;
    DataBlock dataBlock;

    if (debugLevel > 98) {
	printf("\nxbsa_ReadObjectBegin %d Bytes\n", bufferSize);
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_ReadObjectBegin: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    if ((bufferSize < 0) || (bufferSize > XBSAMAXBUFFER)) {
	ELog(0, "xbsa_ReadObjectBegin: The bufferSize %d is invalid\n",
	     bufferSize);
	return (BUTX_INVALIDBUFFERSIZE);
    }

    if (dataBuffer == NULL) {
	ELog(0, "xbsa_ReadObjectBegin: The dataBuffer is NULL\n");
	return (BUTX_INVALIDDATABUFFER);
    }

    dataBlock.bufferLen = (BSA_UInt16) bufferSize;
    dataBlock.numBytes = (BSA_UInt16) 0;
    dataBlock.bufferPtr = dataBuffer;
    *endOfData = 0;

    rc = (int)XBSAGetObject(info->bsaHandle, &info->curObject, &dataBlock);
    if ((rc != BSA_RC_MORE_DATA) && (rc != BSA_RC_NO_MORE_DATA)) {
	ELog(0,
	     "xbsa_ReadObjectBegin: The XBSAGetObject call failed with %d\n",
	     rc);
	xbsa_error(rc, info);
	return (BUTX_GETOBJFAIL);
    }
    *count = dataBlock.numBytes;
    if (rc == BSA_RC_NO_MORE_DATA)
	*endOfData = 1;
    return (XBSA_SUCCESS);
}


/*
 * Tell the XBSA Server that this is the end of Data for this object.
 * BSAEndData()
 */
afs_int32
xbsa_ReadObjectEnd(struct butx_transactionInfo * info)
{
    int rc;

    if (debugLevel > 98) {
	printf("\nxbsa_ReadObjectEnd\n");
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_ReadObjectEnd: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }
    rc = (int)XBSAEndData(info->bsaHandle);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "xbsa_ReadObjectEnd: XBSAEndData call failed with %d\n", rc);
	xbsa_error(rc, info);
	return (BUTX_ENDDATAFAIL);
    }
    return (XBSA_SUCCESS);
}


/*
 * Create the XBSA Backup Copy Object.
 * BSACreateObject
 */
afs_int32
xbsa_WriteObjectBegin(struct butx_transactionInfo * info,
		      char *objectSpaceName, char *pathName, char *lGName,
		      afs_hyper_t estimatedSize, char *objectDescription,
		      char *objectInfo)
{
    int rc;
    int length;
    DataBlock dataBlock;

    if (debugLevel > 98) {
	printf
	    ("\nxbsa_WriteObjectBegin objectSpacename='%s' pathName='%s' lGName='%s' "
	     "estimatesSize='0x%x%08x objectDescription='%s' objectInfo='%s'\n",
	     GOODSTR(objectSpaceName), GOODSTR(pathName), GOODSTR(lGName),
	     hgethi(estimatedSize), hgetlo(estimatedSize),
	     GOODSTR(objectDescription), GOODSTR(objectInfo));
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_WriteObjectBegin: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    if (objectSpaceName) {
	if (strlen(objectSpaceName) >= BSA_MAX_OSNAME) {
	    ELog(0,
		 "xbsa_WriteObjectBegin: The objectSpaceName is too long; size = %d; name = %s\n",
		 strlen(objectSpaceName), objectSpaceName);
	    return (BUTX_INVALIDOBJECTSPNAME);
	}
	strcpy(info->curObject.objName.objectSpaceName, objectSpaceName);
    } else {
	info->curObject.objName.objectSpaceName[0] = NULL;
    }

    if (pathName) {
	if (strlen(pathName) >= BSA_MAX_PATHNAME) {
	    ELog(0,
		 "xbsa_WriteObjectBegin: The pathName is too long; size = %d; name = %s\n",
		 strlen(pathName), pathName);
	    return (BUTX_INVALIDPATHNAME);
	}
	strcpy(info->curObject.objName.pathName, pathName);
    } else {
	info->curObject.objName.pathName[0] = NULL;
    }

    if (lGName) {
	if (strlen(lGName) >= BSA_MAX_LG_NAME) {
	    ELog(0,
		 "xbsa_WriteObjectBegin: The lGName is too long; size = %d; name = %s\n",
		 strlen(lGName), lGName);
	    return (BUTX_INVALIDLGNAME);
	}
	strcpy(info->curObject.lGName, lGName);
    } else {
	info->curObject.lGName[0] = NULL;
    }

    if (objectDescription) {
	if (((XBSA_GET_SERVER_TYPE(info->serverType) == XBSA_SERVER_TYPE_ADSM)
	     && (strlen(objectDescription) >= ADSM_MAX_DESC))
	    ||
	    ((XBSA_GET_SERVER_TYPE(info->serverType) != XBSA_SERVER_TYPE_ADSM)
	     && (strlen(objectDescription) >= BSA_MAX_DESC))) {
	    ELog(0,
		 "xbsa_WriteObjectBegin: The objectDescription is too long; size = %d; name = %s\n",
		 strlen(objectDescription), objectDescription);
	    return (BUTX_INVALIDOBJDESC);
	}
	strcpy(info->curObject.desc, objectDescription);
    } else {
	info->curObject.desc[0] = NULL;
    }

    if (objectInfo) {
	if (((XBSA_GET_SERVER_TYPE(info->serverType) == XBSA_SERVER_TYPE_ADSM)
	     && (strlen(objectInfo) >= ADSM_MAX_OBJINFO))
	    ||
	    ((XBSA_GET_SERVER_TYPE(info->serverType) != XBSA_SERVER_TYPE_ADSM)
	     && (strlen(objectInfo) >= BSA_MAX_OBJINFO))) {
	    ELog(0,
		 "xbsa_WriteObjectBegin: The objectInfo is too long; size = %d; name = %s\n",
		 strlen(objectInfo), objectInfo);
	    return (BUTX_INVALIDOBJINFO);
	}
	strcpy(info->curObject.objectInfo, objectInfo);
    } else {
	info->curObject.objectInfo[0] = NULL;
    }

    if (info->numObjects == info->maxObjects) {
	/* If we've used up Max Objects we must start a new transaction. */
	rc = (int)xbsa_EndTrans(info);
	if (rc != XBSA_SUCCESS) {
	    return (rc);
	}
	rc = (int)xbsa_BeginTrans(info);
	if (rc != XBSA_SUCCESS) {
	    return (rc);
	}
    }

    dataBlock.bufferLen = (BSA_UInt16) 0;
    dataBlock.numBytes = (BSA_UInt16) 0;
    dataBlock.bufferPtr = 0;

    info->curObject.Owner = info->objOwner;
    info->curObject.copyType = BSACopyType_BACKUP;
    info->curObject.size.left = hgethi(estimatedSize);
    info->curObject.size.right = hgetlo(estimatedSize);
    info->curObject.objectType = BSAObjectType_FILE;
    strcpy(info->curObject.resourceType, resourceType);

    rc = (int)XBSACreateObject(info->bsaHandle, &info->curObject, &dataBlock);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0,
	     "xbsa_WriteObjectBegin: The XBSACreateObject call failed with %d\n",
	     rc);
	xbsa_error(rc, info);
	return (BUTX_CREATEOBJFAIL);
    }

    info->numObjects++;

    return (XBSA_SUCCESS);
}

/*
 * Delete a backup object from the server
 * BSAMarkObjectInactive()
 * BSADeleteObject()
 */
afs_int32
xbsa_DeleteObject(struct butx_transactionInfo * info, char *objectSpaceName,
		  char *pathName)
{
    int rc;
    ObjectName objectName;

    if (debugLevel > 98) {
	printf("\nxbsa_DeleteObject objectSpacename='%s' pathName='%s'\n",
	       GOODSTR(objectSpaceName), GOODSTR(pathName));
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_DeleteObject: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    if (objectSpaceName) {
	if (strlen(objectSpaceName) >= BSA_MAX_OSNAME) {
	    ELog(0,
		 "xbsa_DeleteObject: The objectSpaceName is too long; size = %d; name = %s\n",
		 strlen(objectSpaceName), objectSpaceName);
	    return (BUTX_INVALIDOBJECTSPNAME);
	}
	strcpy(objectName.objectSpaceName, objectSpaceName);
    } else {
	objectName.objectSpaceName[0] = NULL;
    }

    if (pathName) {
	if (strlen(pathName) >= BSA_MAX_PATHNAME) {
	    ELog(0, "xbsa_DeleteObject: strlen(pathName), pathName\n",
		 strlen(pathName), pathName);
	    return (BUTX_INVALIDPATHNAME);
	}
	strcpy(objectName.pathName, pathName);
    } else {
	objectName.pathName[0] = NULL;
    }

    rc = (int)XBSAMarkObjectInactive(info->bsaHandle, &objectName);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0,
	     "xbsa_DeleteObject: XBSAMarkObjectInactive call failed with %d\n",
	     rc);
	xbsa_error(rc, info);
	return ((rc ==
		 BSA_RC_ABORT_ACTIVE_NOT_FOUND) ? BUTX_DELETENOVOL :
		BUTX_DELETEOBJFAIL);
    }

    return (XBSA_SUCCESS);
}

/*
 * Tell the XBSA Server that this is the end of Data for this object.
 * BSAEndData()
 */
afs_int32
xbsa_WriteObjectEnd(struct butx_transactionInfo * info)
{
    int rc;

    if (debugLevel > 98) {
	printf("\nxbsa_WriteObjectEnd\n");
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_WriteObjectEnd: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }
    rc = (int)XBSAEndData(info->bsaHandle);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "xbsa_WriteObjectEnd: XBSAEndData call failed with %d\n", rc);
	xbsa_error(rc, info);
	return (BUTX_ENDDATAFAIL);
    }

    return (XBSA_SUCCESS);
}


/*
 * Write the fileset data to the XBSA server
 * BSASendData
 */
afs_int32
xbsa_WriteObjectData(struct butx_transactionInfo * info, char *dataBuffer,
		     afs_int32 bufferSize, afs_int32 * count)
{
    int rc;
    DataBlock dataBlock;

    if (debugLevel > 98) {
	printf("\nxbsa_WriteObjectData %d Bytes\n", bufferSize);
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_WriteObjectData: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    if ((bufferSize < 0) || (bufferSize > XBSAMAXBUFFER)) {
	ELog(0, "xbsa_WriteObjectData: The bufferSize %d is invalid\n",
	     bufferSize);
	return (BUTX_INVALIDBUFFERSIZE);
    }

    if (dataBuffer == NULL) {
	ELog(0, "xbsa_WriteObjectData: The dataBuffer is NULL\n");
	return (BUTX_INVALIDDATABUFFER);
    }

    dataBlock.bufferPtr = dataBuffer;
    dataBlock.bufferLen = (BSA_UInt16) bufferSize;
    dataBlock.numBytes = (BSA_UInt16) 0;

    rc = (int)XBSASendData(info->bsaHandle, &dataBlock);
    if (rc != BSA_RC_SUCCESS) {
	ELog(0, "xbsa_WriteObjectData: XBSAEndData call failed with %d\n",
	     rc);
	xbsa_error(rc, info);
	return (BUTX_SENDDATAFAIL);
    }
    *count = dataBlock.numBytes;
    return (XBSA_SUCCESS);
}


/*
 * Read the fileset data from the XBSA server
 * BSAGetData
 */
afs_int32
xbsa_ReadObjectData(struct butx_transactionInfo * info, char *dataBuffer,
		    afs_int32 bufferSize, afs_int32 * count,
		    afs_int32 * endOfData)
{
    int rc;
    DataBlock dataBlock;

    if (debugLevel > 98) {
	printf("\nxbsa_ReadObjectData %d Bytes\n", bufferSize);
	printf
	    ("serverName='%s' ; bsaObjectOwner='%s' ; appObjectOwner='%s' ; sectoken=xxxxxx\n",
	     GOODSTR(info->serverName),
	     GOODSTR(info->objOwner.bsaObjectOwner),
	     GOODSTR(info->objOwner.appObjectOwner));
    }

    if (info->bsaHandle == 0) {
	/* We haven't initialized yet! */
	ELog(0,
	     "xbsa_ReadObjectData: No current handle, butx not initialized\n");
	return (BUTX_NOHANDLE);
    }

    if ((bufferSize < 0) || (bufferSize > XBSAMAXBUFFER)) {
	ELog(0, "xbsa_ReadObjectData: The bufferSize %d is invalid\n",
	     bufferSize);
	return (BUTX_INVALIDBUFFERSIZE);
    }

    if (dataBuffer == NULL) {
	ELog(0, "xbsa_ReadObjectData: The dataBuffer is NULL\n");
	return (BUTX_INVALIDDATABUFFER);
    }

    dataBlock.bufferLen = (BSA_UInt16) bufferSize;
    dataBlock.numBytes = (BSA_UInt16) 0;
    dataBlock.bufferPtr = dataBuffer;
    *endOfData = 0;

    rc = (int)XBSAGetData(info->bsaHandle, &dataBlock);
    if ((rc != BSA_RC_MORE_DATA) && (rc != BSA_RC_NO_MORE_DATA)) {
	ELog(0, "xbsa_ReadObjectData: XBSAGetData call failed with %d\n", rc);
	xbsa_error(rc, info);
	return (BUTX_GETDATAFAIL);
    }
    *count = dataBlock.numBytes;
    if (rc == BSA_RC_NO_MORE_DATA)
	*endOfData = 1;
    return (XBSA_SUCCESS);
}
#endif /*xbsa */
