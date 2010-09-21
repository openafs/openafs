/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BSA_XBSA_H_
#define _BSA_XBSA_H_

#ifdef __cplusplus
extern "C" {
#endif

#if (SYS_V > 3) || defined(BERK4_2) || defined(SUN4)
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifdef bool_t
#undef bool_t
#endif

/* include TSM Header Files. */
#include <dsmapitd.h>
#include <dsmapifp.h>
#include <dsmrc.h>

/* BSA_Boolean  */
typedef char BSA_Boolean;

/* BSA_Int16 */
typedef short BSA_Int16;

/* BSA_Int32 */
typedef long BSA_Int32;

/* BSA_Int64  */
typedef struct {    /* defined as two 32-bit integers */
    BSA_Int32   left;
    BSA_Int32   right;
} BSA_Int64;

/* BSA_UInt16  */
typedef unsigned short BSA_UInt16;

/* BSA_UInt32 */
typedef unsigned long BSA_UInt32;

/* BSA_UInt64  */
typedef struct {        /* defined as two unsigned 32-bit integers*/
    BSA_UInt32  left;
    BSA_UInt32  right;
} BSA_UInt64;

/*-------------------------------------------------------------------------+
|  ADSM specific constants and return codes                                |
+-------------------------------------------------------------------------*/
/* TSM library version, release, level
 */
#define BSA_API_VERSION     5
#define BSA_API_RELEASE     2
#define BSA_API_LEVEL       0

/* Constants used  */
#define ADSM_MAX_DESC               100     /* ADSM max Desc size             */
#define ADSM_MAX_OBJINFO            100     /* ADSM max object info size      */
#define ADSM_LOWEST_BOUND           0x0000  /* value for LowerBound min       */
#define ADSM_HIGHEST_BOUND          0xFFFF  /* value for UpperBound max       */
#define ADSM_ENV_STRS               3       /* number of env strings          */
#define ObjectDescriptorVersion     1       /* ver for ObjectDescriptor       */
#define UserDescriptorVersion       1       /* ver for UserDescriptor         */
#define BSAObjectType_DATABASE      4       /*  ObjectType for Databases      */

/* Return Codes Used
 */
#define BSA_RC_OK                           0x00
#define BSA_RC_SUCCESS                      0x00

#define ADSM_RC_ERROR                       0x60 /* see ADSM error log        */
#define ADSM_RC_INVALID_NODE                0x61 /* BSAObjOwner not match Init*/
#define ADSM_RC_INVALID_COPYTYPE            0x62 /* invalid copyType          */
#define ADSM_RC_INVALID_OBJTYPE             0x63 /* invalid objectType        */
#define ADSM_RC_INVALID_STATUS              0x64 /* invalid object status     */
#define ADSM_RC_INVALID_ST_VER              0x65 /* invalid structure version */
#define ADSM_RC_OWNER_TOO_LONG              0x66 /* owner too long            */
#define ADSM_RC_PSWD_TOO_LONG               0x67 /* pswd  too long            */
#define ADSM_RC_PSWD_GEN                    0x68 /* pswd access = generate    */

/* Constants used
 *
 * Maximum string lengths (lower bound), including trailing null
 */
#define BSA_LIST_ELEMENT_DELIMITER  null    /* Element delimiter in list      */
#define BSA_MAX_ADMIN_NAME          64      /* Administrator name             */
#define BSA_MAX_APPOBJECT_OWNER     64      /* Max end-object owner length    */
#define BSA_MAX_BSAOBJECT_OWNER     64      /* Max BSA object owner length    */
#define BSA_MAX_CG_DEST             31      /* Copy group destination         */
#define BSA_MAX_CG_NAME             31      /* Max copy group name length     */
#define BSA_MAX_COPYGROUPS          16      /* Max number of copy groups      */
                                            /* which can be specified in a    */
                                            /* lifecycle group                */
#define BSA_MAX_DESC                256     /* Description field              */
#define BSA_MAX_ENCODINGMETHOD      31      /* Max encoding method length     */
#define BSA_MAX_EVENTINFO           256     /* Max event info size            */
#define BSA_MAX_FILTERRULESET       8192    /* Max filter rule set size       */
#define BSA_MAX_OSNAME              1024    /* Max Objectspace name length    */
#define BSA_MAX_LG_NAME             31      /* Life cycle group name          */
#define BSA_MAX_LIFECYCLEGROUPS     64      /* Max number of life cycle       */
                                            /* groups in a policy set         */
#define BSA_MAX_OBJINFO             512     /* Max object info size           */
#define BSA_MAX_PATHNAME            1024    /* Max path name length           */
#define BSA_MAX_POLICYDOMAINS       256     /* Max number of specific policy  */
                                            /* domains an administrator may   */
                                            /* be responsible for             */
#define BSA_MAX_POLICYDOMAIN_NAME   31      /* Policy domain name             */
#define BSA_MAX_POLICYSETS          16      /* Max number of policy sets      */
                                            /* in a domain                    */
#define BSA_MAX_POLICYSET_NAME      31      /* Policy set name                */
#define BSA_MAX_RESOURCETYPE        31      /* Max resource mgr name length   */
#define BSA_MAX_TOKEN_SIZE          64      /* Max size of a security token   */
#define BSA_PUBLIC              "BSA_ANY"   /* Default string                 */

/* Return Codes Used
 *
 * Return Code descriptions are given in Section 4.3.
 */
#define BSA_RC_ABORT_ACTIVE_NOT_FOUND       0x02
#define BSA_RC_ABORT_SYSTEM_ERROR           0x03
#define BSA_RC_AUTHENTICATION_FAILURE       0x04
#define BSA_RC_BAD_CALL_SEQUENCE            0x05
#define BSA_RC_BAD_HANDLE                   0x06
#define BSA_RC_BUFFER_TOO_SMALL             0x07
#define BSA_RC_DESC_TOO_LONG                0x08
#define BSA_RC_OBJECTSPACE_TOO_LONG         0x09
#define BSA_RC_INVALID_TOKEN                0x0a
#define BSA_RC_INVALID_VOTE                 0x0b
#define BSA_RC_INVALID_KEYWORD              0x0c
#define BSA_RC_MATCH_EXISTS                 0x0d
#define BSA_RC_MORE_DATA                    0x0e
#define BSA_RC_MORE_RULES                   0x0f
#define BSA_RC_NEWTOKEN_REQD                0x10
#define BSA_RC_NO_MATCH                     0x11
#define BSA_RC_NO_MORE_DATA                 0x12
#define BSA_RC_NO_RESOURCES                 0x13
#define BSA_RC_NULL_DATABLKPTR              0x14
#define BSA_RC_NULL_OBJNAME                 0x15
#define BSA_RC_NULL_POINTER                 0x16
#define BSA_RC_NULL_RULEID                  0x17
#define BSA_RC_OBJECT_NAME_TOO_LONG         0x18
#define BSA_RC_OBJECT_NOT_EMPTY             0x19
#define BSA_RC_OBJECT_NOT_FOUND             0x1a
#define BSA_RC_OBJINFO_TOO_LONG             0x1b
#define BSA_RC_OBJNAME_TOO_LONG             0x1c
#define BSA_RC_OPERATION_NOT_AUTHORIZED     0x1d
#define BSA_RC_OLDTOKEN_REQD                0x1e
#define BSA_RC_TOKEN_EXPIRED                0x1f
#define BSA_RC_TXN_ABORTED                  0x20
#define BSA_RC_UNMATCHED_QUOTE              0x21
#define BSA_RC_USER_OWNS_OBJECTS            0x22

/* AppObjectOwner
 */
typedef char AppObjectOwner[BSA_MAX_APPOBJECT_OWNER];

/* BSAObjectOwner
 */
typedef char BSAObjectOwner[BSA_MAX_BSAOBJECT_OWNER];

/* CopyGpDest
 */
typedef char CopyGpDest[BSA_MAX_CG_DEST];

/* CopyGpName
 */
typedef char CopyGpName[BSA_MAX_CG_NAME];

/* CopyMode
 *
 * Constant     Value     Explanation
 * --------     -----     -----------
 * INCREMENTAL  1         Specifies that the Backup Services should make a
 *                        copy only if the application object has been
 *                        modified since the last time this copy group was
 *                        used to create the object's copy.
 * ABSOLUTE     2         Specifies that the Backup Services should make a
 *                        copy even if the application object has not been
 *                        modified since the last time this copy group was
 *                        used to create the object's copy.
 */
typedef enum {
    BSACopyMode_INCREMENTAL = 1,
    BSACopyMode_ABSOLUTE = 2
} CopyMode;

/* CopySerialization
 *
 * Constant     Value     Explanation
 * --------     -----     -----------
 * STATIC       1         Specifies that the Backup Services must create a
 *                        consistent (unmodified during the operation) copy of
 *                        the object.  If the application is unable to create a
 *                        consistent copy then it should skip the operation
 *                        (creating backup or archive copy) for the object.
 * SHAREDSTATIC 2         Specifies that the Backup Services must create a
 *                        consistent copy of the object.  It can retry the
 *                        operation a number of times (application dependent).
 *                        If the Backup Services is unable to create a
 *                        consistent copy even after retries, then it should
 *                        skip creating a backup or archive copy of the
 *                        object.
 * SHAREDDYNAMIC 3        Specifies that the Backup Services must create a
 *                        copy of the object.  It can retry the operation a
 *                        number of times in an attempt to create a consistent
 *                        copy; however, if it fails to make a consistent
 *                        copy, a copy must still be made, ignoring the fact
 *                        that the copy may have been modified during the
 *                        operation.  Such copies are useful for log file
 *                        objects which are being continuously modified.
 * DYNAMIC      4         Specifies that the Backup Services must create a
 *                        copy of the obbject even if the source object is
 *                        modified during the operation.  No retries should be
 *                        attempted to create a consistent copy.
 */
typedef enum {
    BSACopySerialization_STATIC = 1,
    BSACopySerialization_SHAREDSTATIC = 2,
    BSACopySerialization_SHAREDDYNAMIC = 3,
    BSACopySerialization_DYNAMIC = 4
} CopySerialization;

/* CopyType
 *
 * Constant     Value     Explanation
 * --------     -----     -----------
 * ANY          1         Used for matching any copy type (e.g. "backup" or
 *                        "archive" in the copy type field of structures for
 *                        selecting query results).
 * ARCHIVE      2         Specifies that the copy type should be "archive".
 *                        When used in the copy type field of the CopyGroup,
 *                        it identifies the copy data as of type
 *                        ArchiveCopyData, which is used to create archive
 *                        copies.
 * BACKUP       3         Specifies that the copy type should be "backup".
 *                        When used in the copy type field of the CopyGroup,
 *                        it identifies the copy data as of type
 *                        BackupCopyData, which is used to create backup
 *                        copies.
 */
typedef enum {
    BSACopyType_ANY = 1,
    BSACopyType_ARCHIVE = 2,
    BSACopyType_BACKUP = 3
} CopyType;

/* Description
 */
typedef char Description[BSA_MAX_DESC];

/* DomainName
 */
typedef char * DomainName[BSA_MAX_POLICYDOMAIN_NAME];

/* EventInfo
 */
typedef char EventInfo[BSA_MAX_EVENTINFO];

/* LGName
 */
typedef char LGName[BSA_MAX_LG_NAME];

/* ObjectInfo
 */
typedef char ObjectInfo[BSA_MAX_OBJINFO];

/* ObjectName
 */
typedef struct {
    char    objectSpaceName[BSA_MAX_OSNAME]; /* Highest-level name qualifier */
    char    pathName[BSA_MAX_PATHNAME];      /* Object name within           */
                                             /* objectspace                  */
} ObjectName;

/* ObjectOwner
 */
typedef struct {
    BSAObjectOwner  bsaObjectOwner;    /* BSA Owner name - this is the name  */
                                       /* that Backup Services authenticates */
    AppObjectOwner  appObjectOwner;    /* End-owner name, this is the name   */
                                       /* defined by the application         */
} ObjectOwner;

/* ObjectSize
 */
typedef BSA_UInt64 ObjectSize;         /* Unsigned 64-bit integer */

/* ObjectStatus
 *
 * Constant     Value     Explanation
 * --------     -----     -----------
 * ANY          1         Provides a wild card function.  Can only be used in
 *                        queries.
 * ACTIVE       2         Indicates that this is the most recent backup copy
 *                        of an object.
 * INACTIVE     3         Indicates that this is not the most recent backup
 *                        copy, or that the object itself no longer exists.
 */
typedef enum {
    BSAObjectStatus_ANY = 1,
    BSAObjectStatus_ACTIVE = 2,
    BSAObjectStatus_INACTIVE = 3
} ObjectStatus;

/* ObjectType
 *
 * Constant     Value     Explanation
 * --------     -----     -----------
 * any          1         Used for matching any object type (e.g. "file" or
 *                        "directory") value in the object type field of
 *                        structures for selecting query results.
 * file         2         Used by the application to indicate that the type of
 *                        application object is a "file" or single object.
 * directory    3         Used by the application to indicate that the type of
 *                        application object is a "directory" or container of
 *                        objects.
 */

typedef enum {
    BSAObjectType_ANY  = 1,
    BSAObjectType_FILE    = 2,
    BSAObjectType_DIRECTORY = 3
 } ObjectType;

/* Operation
 *
 * Constant     Value     Explanation
 * --------     -----     -----------
 * archive      1         Used to indicate that a scheduled operation is of
 *                        type "archive".
 * backup       2         Used to indicate that a scheduled operation is of
 *                        type "backup".
 */
typedef enum {
    BSAOperation_ARCHIVE = 1,
    BSAOperation_BACKUP  = 2
} Operation;

/* Period
 *
 * Use of the Period structure in a Schedule for an event:
 * 1. The Schedule structure specifies 3 timing elements:
 *    a. "firstStartTime" - a timestamp showing the "earliest"
 *        possible time the event could take place
 *    b. "day" - the day of the week the event should occur
 *    c. "frequency" - the period between successive events
 * 2. To determine the day the event should occur (this does
 *    not change the time of the event in the day):
 *    a. Determine the requested day from "firstStartTime".
 *    b. If "day" does not equal XBSA_DAYOFWEEK_ANY
 *       i. Compare "day" to the day of the week corresponding
 *          to "firstStartTime" determined in 2a above.
 *       ii.If the days match, then use "firstStartTime" else
 *          use the first "day" *following* the day shown
 *          in "firstStartTime" as the day of the event.
 * 3. If the PeriodWhich field in the Period structure is other
 *    than XBSA_PERIOD_UNDEFINED, then successive events are
 *    determined using the value of "periodData".
 *    a. For seconds and days, the appropriate seconds or days
 *       are added to the "firstStartTime" and step 2 to correct
 *       for the day of the week (if needed) is applied again.
 *    b. If a monthly period is specified, then the appropriate
 *       number of months are added by incrementing the month index is
 *       made (for example, a one month increment from February to
 *       March).  If the monthly date is not valid (for example, January
 *       30 --> February 30) then the last day of the desired month is
 *       used (example January 30 --> February 28).  Then step 2 is
 *       followed to adjust for the requested day (which might move the
 *       event time into the following month).
 */
typedef enum {
    BSAPeriod_SECONDS = 1,
    BSAPeriod_DAYS = 2,
    BSAPeriod_MONTHS = 3,
    BSAPeriod_UNDEFINED = -9,
    BSAPeriod_DAILY = 4, BSAPeriod_WEEKLY = 5, BSAPeriod_MONTHLY = 6,
    BSAPeriod_QUARTERLY = 7, BSAPeriod_ANNUALLY = 8
} PeriodWhich;

typedef struct {
    PeriodWhich     which;
    union {
        time_t      nSeconds;
        BSA_Int16   nDays;
        BSA_Int16   nMonths;
    } periodData;
} Period;

/* ResourceType
 */
typedef char ResourceType[BSA_MAX_RESOURCETYPE];

/* RuleId
 */
typedef BSA_UInt64 RuleId;

/* Scheduleid
 */
typedef BSA_UInt64 ScheduleId;

/* AccessRight
 *
 * Constant     Value     Explanation
 * --------     -----     -----------
 * GET          1         Access right for getting an object from Backup
 *                        Services, also includes access right for querying
 *                        (getting attributes) an object from Backup Services.
 * QUERY        2         Access right for querying (getting attributes) an
 *                        object from Backup Services.
 */
typedef enum {
    BSAAccessRight_GET  = 1,
    BSAAcessRight_QUERY = 2
} AccessRight;

/* AccessRule
 */
typedef struct {
    RuleId          ruleId;         /* Provided by Backup Services            */
    ObjectName      objName;        /* Object name to be given access         */
    ObjectOwner     objectOwner;    /* BSA object owner and Applicaton object */
                                    /* owner to be given access               */
    AccessRight     rights;         /* The access rights to be given          */
} AccessRule;

/* ApiVersion
 */
typedef struct {
    BSA_UInt16      version;        /* Version of this API                    */
    BSA_UInt16      release;        /* Release of this API                    */
    BSA_UInt16      level;          /* Level of this API                      */
} ApiVersion;

/* ArchiveCopyData
 */
typedef struct {
    CopyGpName          cGName;     /* Copy group name                        */
    BSA_UInt16          freq;       /* Archive frequency                      */
    CopySerialization   copySer;    /* Copy serialization code                */
    CopyMode            copyMode;   /* Copy mode                              */
    CopyGpDest          destName;   /* Copy destination name                  */
    BSA_UInt16          retVersion; /* Retention time for the version         */
} ArchiveCopyData;

/* BackupCopyData
 */
typedef struct {
    CopyGpName          cGName;     /* Copy group name                        */
    BSA_UInt16          freq;       /* Backup frequency                       */
    CopySerialization   copySer;    /* Copy serialization code                */
    CopyMode            copyMode;   /* Copy mode: 1=modified, 2=absolute      */
    CopyGpDest          destName;   /* Copy destination name                  */
    BSA_UInt16          verDataEx;  /* Versions (number of versions           */
                                    /* retained)                              */
    BSA_UInt16          verDataDel; /* Versions (data deleted)                */
    Period              retXtraVer; /* Retain extra versions                  */
    Period              retOnlyVer; /* Retain only versions                   */
} BackupCopyData;

/* CopyGroup
 */
typedef struct {
    CopyType    copyType;       /* Type of copy group: archive, backup, etc   */
    union {
        ArchiveCopyData archive;
        BackupCopyData  backup;
    } copyData;
} CopyGroup;

/* CopyId
 */
typedef BSA_UInt64      CopyId;

/* DataBlock
 */
typedef struct {
    BSA_UInt16  bufferLen;
    BSA_UInt16  numBytes;       /* Actual number of bytes read from */
                                /* or written to the buffer, or the */
                                /* minimum number of bytes needed   */
    char *      bufferPtr;
} DataBlock;

/* DayOfWeek
 */
typedef enum {
    BSADayOfWeek_Monday = 1,    BSADayOfWeek_Tuesday = 2,
    BSADayOfWeek_Wednesday = 3, BSADayOfWeek_Thursday = 4,
    BSADayOfWeek_Friday = 5,    BSADayOfWeek_Saturday = 6,
    BSADayOfWeek_Sunday = 7
} DayOfWeek;

/* Environment
 */
typedef struct {
    char * envVariables; /* Identifies the Backup Services instance and other */
                         /* implementation-dependent variables such as        */
                         /* communication ports, etc.  Each variable is a     */
                         /* (keyword, value) pair separated by a space.       */
                         /* If a value contains spaces, it must be            */
                         /* enclosed in single or double quotes.              */
} BSAEnvironment;

/* Event
 */
typedef struct {
    BSA_UInt32      eventId;    /* This is an internal (to Backup Services) id*/
    ObjectOwner     objectOwner;/* Identifies the owner of the event          */
    struct tm       time;       /* Identifies the time the action is to start */
    Operation       action;     /* Identifies the action (backup, archive)    */
    ObjectName      objName;    /* Identifies objects to be acted on          */
    ResourceType    resourceType;/* Identifies the resource manager for the   */
                                 /* event                                     */
    EventInfo       eventInfo;  /* User- and resource-manager-specific info   */
} BSAEvent;

/* MethodName
 */
typedef char EncodingMethod[BSA_MAX_ENCODINGMETHOD];

/* ObjectDescriptor
 */
typedef struct {
    BSA_UInt32      version;        /* Version number for this structure      */
    ObjectOwner     Owner;          /* Owner of the object                    */
    ObjectName      objName;        /* Object name                            */
    struct tm       createTime;     /* Supplied by Backup Services            */
    CopyType        copyType;       /* Copy type: archive or backup           */
    CopyId          copyId;         /* Supplied by Backup Services            */
    BSA_UInt64      restoreOrder;   /* Supplied by Backup Services            */
    LGName          lGName;         /* Associated Lifecycle Group name        */
    CopyGpName      cGName;         /* Copy group within the lifecycle group  */
    ObjectSize      size;           /* Object size may be up to 63 bits       */
    ResourceType    resourceType;   /* e.g. UNIX file system                  */
    ObjectType      objectType;     /* e.g. file, directory, etc.             */
    ObjectStatus    status;         /* Active/inactive, supplied by           */
                                    /* Backup Services                        */
    EncodingMethod * encodingList;  /* List of encoding Methods used, in      */
                                    /* application-defined order,             */
                                    /* terminated with a null entry           */
    Description     desc;           /* Descriptive label for the object       */
    ObjectInfo      objectInfo;     /* Application information                */
} ObjectDescriptor;

/* QueryDescriptor
 */
typedef struct {
    ObjectOwner     owner;          /* Owner of the object                    */
    ObjectName      objName;        /* Object name                            */
    struct tm       createTimeLB;   /* Lower bound on create time             */
    struct tm       createTimeUB;   /* Upper bound on create time             */
    struct tm       expireTimeLB;   /* Lower bound on expiration time         */
    struct tm       expireTimeUB;   /* Upper bound on expiration time         */
    CopyType        copyType;       /* Copy type: archive or backup           */
    LGName          lGName;         /* Associated Lifecycle Group name        */
    CopyGpName      cGName;         /* Copy group within the lifecycle group  */
    ResourceType    resourceType;   /* e.g. UNIX file system                  */
    ObjectType      objectType;     /* e.g. file, directory, etc.             */
    ObjectStatus    status;         /* Active/inactive, supplied by Backup    */
                                    /* Services                               */
    Description     desc;           /* Descriptive label for the object       */
} QueryDescriptor;                  /* "*" is interpreted as a meta character */
                                    /* wild card.                             */
                                    /* Any undefined value is interpreted as  */
                                    /* an error.                              */

/* Schedule
 */
typedef struct {
    ScheduleId      schedId;        /* Provided by Backup Services            */
    ObjectOwner     objectOwner;    /* Specifies the owner of the schedule    */
    Operation       operation;      /* Specifies the action to be taken       */
    struct tm       firstStartTime; /* Specifies the first time the action    */
                                    /* is to take place                       */
    DayOfWeek       day;            /* Specifies the day of week for the event*/
    Period          frequency;      /* Specifies the frequency, e.g. daily    */
    ObjectName      objectName;     /* Identifies objects to be acted on      */
    ResourceType    resourceType;   /* Identifies the resource manager for the*/
                                    /* schedule                               */
    EventInfo       scheduleInfo;   /* Resource manager specific information  */
} Schedule;

/* Security Token
 */
typedef char SecurityToken[BSA_MAX_TOKEN_SIZE];

/* StreamHandle
 */
typedef int StreamHandle;

/* UserDescriptor
 */
typedef struct t_UserDescriptor {
    BSA_UInt16      version;        /* Version num of this structure          */
    BSAObjectOwner  bsaObjectOwner; /* BSA Object owner name              */
    DomainName      domainName;     /* Policy domain for the user         */
    Description     desc;           /* User information                   */
} UserDescriptor;

/* Vote
 */
typedef enum {
    BSAVote_COMMIT = 1,
    BSAVote_ABORT  = 2
} Vote;

/* Function Prototypes for Data Movement API Subset
 * Note that int and long have been replaced with typedefs
 * from custom.h.
 */

extern BSA_Int16
BSABeginTxn
(      long              bsaHandle
);

extern BSA_Int16
BSAChangeToken
(      long              bsaHandle,
       SecurityToken    *oldTokenPtr,
       SecurityToken    *newTokenPtr
);

extern BSA_Int16
BSACreateObject
(      long              bsaHandle,
       ObjectDescriptor *objectDescriptorPtr,
       DataBlock        *dataBlockPtr
);

extern BSA_Int16
BSACreateObjectF
(      long              bsaHandle,
       ObjectDescriptor *objectDescriptorPtr,
       StreamHandle     *streamPtr
);

extern BSA_Int16
BSADeleteObject
(      long              bsaHandle,
       CopyType          copyType,
       ObjectName       *objectName,
       CopyId           *copyId
);

extern BSA_Int16
BSAEndData
(      long              bsaHandle
);

extern BSA_Int16
BSAEndTxn
(      long              bsaHandle,
       Vote              vote
);

extern BSA_Int16
BSAGetData
(      long              bsaHandle,
       DataBlock        *dataBlockPtr
);

extern BSA_Int16
BSAGetEnvironment
(      long              bsaHandle,
       ObjectOwner      *objectOwnerPtr,
       char            **environmentPtr
);

extern BSA_Int16
BSAGetNextQueryObject
(      long              bsaHandle,
       ObjectDescriptor *objectDescriptorPtr
);

extern BSA_Int16
BSAGetObject
(      long              bsaHandle,
       ObjectDescriptor *objectDescriptorPtr,
       DataBlock        *dataBlockPtr
);

extern BSA_Int16
BSAGetObjectF
(      long              bsaHandle,
       ObjectDescriptor *objectDescriptorPtr,
       StreamHandle     *streamPtr
);

extern BSA_Int16
BSAInit
(      long             *bsaHandleP,
       SecurityToken    *tokenPtr,
       ObjectOwner      *objectOwnerPtr,
       char            **environmentPtr
);

extern BSA_Int16
BSAMarkObjectInactive
(      long              bsaHandle,
       ObjectName       *objectNamePtr
);

extern void
BSAQueryApiVersion
(      ApiVersion       *apiVersionPtr
);

extern BSA_Int16
BSAQueryObject
(      long              bsaHandle,
       QueryDescriptor  *queryDescriptorPtr,
       ObjectDescriptor *objectDescriptorPtr
);

extern BSA_Int16
BSASendData
(      long              bsaHandle,
       DataBlock        *dataBlockPtr
);

extern BSA_Int16
BSASetEnvironment
(      long              bsaHandle,
       char            **environmentPtr
);

extern BSA_Int16
BSATerminate
(      long              bsaHandle
);

extern BSA_Int16
BSAResolveLifecycleGroup(
    long              BSAHandle,
    ObjectDescriptor *BSAobjDescP
);

#ifdef __cplusplus
}
#endif

typedef struct
{
  ApiSessInfo      dsmSessInfo;
  BSAObjectOwner   bsaObjectOwner; /* node value on BSAInit */
  dsUint16_t       sessFlags;      /* misc flags            */
  dsUint8_t        oper;        /* send or recv operation */
  CopyType         copyType;    /* Backup or Archive type */
  char             *nextQryP;    /* ptr to respArchive or respBackup */
} xGlobal;

/*  flag masks for sessFlags    */
/*  to set on use   sessFlags = (sessFlags | FL_IN_BSA_TXN) */
/*  to set off use  sessFlags = (sessFlags ^ FL_IN_BSA_TXN) */
/*  to test on use  if (sessFlags & FL_IN_BSA_TXN) */
#define  FL_IN_BSA_TXN     0x0001  /* within a BSA type txn            */
#define  FL_IN_DSM_TXN     0x0002  /* within a actual ADSM txn         */
#define  FL_IN_BSA_QRY     0x0004  /* within a query txn               */
#define  FL_PSWD_EXPIRE    0x0008  /* pswd has expired                 */
#define  FL_RC_WILL_ABORT  0x0010  /* recvd 157 on SendObj or SendData */
#define  FL_END_DATA_DONE  0x0020  /* Get error already did EndData    */

#define OPER_NONE            0    /* values for oper field  */
#define OPER_SEND_START      1
#define OPER_SEND_ISSUED     2
#define OPER_RECV_START      3
#define OPER_RECV_ISSUED     4

/*  structure def for objInfo  - max len = 255 */
typedef struct
{
  ResourceType     resourceType;                       /*  31 bytes */
  ObjectSize       size;                               /*   8 bytes */
  char             partDesc[ADSM_MAX_DESC];            /* 100 bytes */
  char             partObjInfo[ADSM_MAX_OBJINFO];      /* 100 bytes */
} XAPIObjInfo;

/* ******* G l o b a l s  ********  */

/* xGlobal xopenGbl; */

extern BSA_Int16 xlateRC(
    long      BSAHandle,
    BSA_Int16 dsmRC,
    BSA_Int16 *bsaRCP);

extern BSA_Int16 xparsePath(
    long     BSAHandle,
    char          *pathname,
    char          *hl,
    char          *ll);

extern xGlobal        xopenGbl;
extern char           traceStr[];
extern char           traceStr1[];
extern char           traceStr2[];

#define dsMalloc(a)   malloc(a)
#define dsFree(a)     free(a)

extern void StrUpper(char *s);
/* Min() and Max() macros */
#ifndef min
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif

/* Combined file name w/ line number */
#define TrFL __FILE__, __LINE__

/*=== MACRO for exit tracing ===*/
#define XOPENRETURN(BSAHandle,functionName,bsaRC,fileName,lineNumber) {sprintf(traceStr2,"%s EXIT: rc = >%d<.",functionName, bsaRC); ourTrace(BSAHandle,fileName,lineNumber,traceStr2); return(bsaRC);}

/* Define license error log file name */
#define TSMLicLog "tsmlic.log"

#define custMin_retcode  96
#define custMax_retcode  104

/*=======================================================
 Function to places a trace message in the API tracefile,
 the message includes the xopen file name and line number.
 ======================================================*/
void ourTrace(long           BSAHandle,
              char          *fileName,
              int            lineNumber,
              char          *traceStr2);

/*================================================
 Function to log events with call to dsmLogEventEx
 ===============================================*/
void ourLogEvent_Ex(dsUint32_t handle, dsmLogType type, char* msg,
                 char *appMsg, dsmLogSeverity severity);

/*=================================================================
 Function to retrieve a message by routing to dsmRCMsg or stdXOpenMsgMap.
 ===============================================================*/
char* ourRCMsg(dsInt16_t dsmRC, char *msg);

/*====================================================
 Function to build fully qualified license file name.
 void build_lfname(char *lfname
 ========================================================*/
extern void build_lfname(char *lfname);

/*=== Function to deliver standard XOpen API message above API range. */
void stdXOpenMsgMap(dsInt16_t, char*);


#endif /* xbsa */
