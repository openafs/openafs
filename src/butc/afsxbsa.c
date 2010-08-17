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

#include <sys/types.h>
#include <afs/stds.h>
#include <stdio.h>


#if defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_LINUX26_ENV)
#include <dlfcn.h>
#endif

#include <errno.h>
#include "butc_xbsa.h"
#include <afs/butx.h>

/* Global Definations */
#define APPLTYPE "afs-butc"
#define TDP_LIBNAME "libApiDS.a"
#define FIRST_HANDLE 1
#define NOHANDLE 0
#define XAPI_FSINFO "FS for XOpen API"
#define DIR_DELIMITER '/'
#define STR_DIR_DELIMITER '/'

xGlobal xopenGbl;
char traceStr[DSM_MAX_RC_MSG_LENGTH+1];
char traceStr2[(DSM_MAX_RC_MSG_LENGTH+1) - 30];
char ourMsg[DSM_MAX_RC_MSG_LENGTH + 1];
static int dsm_init = 0 ;

/* >>>  TSM function pointers. */
dsInt16_t (* AFSdsmBeginQuery)( dsUint32_t dsmHandle, dsmQueryType queryType, dsmQueryBuff *queryBuffer);
dsInt16_t (* AFSdsmGetNextQObj)( dsUint32_t dsmHandle, DataBlk *dataBlkPtr) ;
dsInt16_t (* AFSdsmEndQuery)( dsUint32_t dsmHandle);
dsInt16_t (* AFSdsmRCMsg)( dsUint32_t dsmHandle, dsInt16_t dsmRC, char *msg);
dsInt16_t (* AFSdsmLogEventEx)( dsUint32_t dsmHandle, dsmLogExIn_t *dsmLogExInP, dsmLogExOut_t *dsmLogExOutP);
dsInt16_t (* AFSdsmTrace)( dsUint32_t dsmHandle, char * string );
dsInt16_t (* AFSdsmTerminate)( dsUint32_t dsmHandle);
dsInt16_t (* AFSdsmSendData)( dsUint32_t dsmHandle, DataBlk *dataBlkPtri);
dsInt16_t (* AFSdsmBeginTxn)( dsUint32_t dsmHandle);
dsInt16_t (* AFSdsmDeleteObj)( dsUint32_t dsmHandle, dsmDelType delType, dsmDelInfo delInfo);
dsInt16_t (* AFSdsmEndTxn)( dsUint32_t dsmHandle, dsUint8_t vote, dsUint16_t *reason);
void (* AFSdsmQueryApiVersion)( dsmApiVersion *apiVersionP);
dsInt16_t (* AFSdsmInit)( dsUint32_t *dsmHandle, dsmApiVersion *dsmApiVersionP, char *clientNodeNameP, char *clientOwnerNameP, char *clientPasswordP, char *applicationType, char *configfile, char *options);
dsInt16_t (* AFSdsmQuerySessInfo)( dsUint32_t dsmHandle, ApiSessInfo *SessInfoP);
dsInt16_t (* AFSdsmBeginGetData)(dsUint32_t dsmHandle, dsBool_t mountWait, dsmGetType getType, dsmGetList *dsmGetObjListP);
dsInt16_t (* AFSdsmGetObj)( dsUint32_t dsmHandle, ObjID *objIdP, DataBlk *dataBlkPtr);
dsInt16_t (* AFSdsmEndGetObj)( dsUint32_t dsmHandle);
dsInt16_t (* AFSdsmEndGetData)( dsUint32_t dsmHandle);
dsInt16_t (* AFSdsmGetData)( dsUint32_t dsmHandle, DataBlk *dataBlkPtr);
dsInt16_t (* AFSdsmEndSendObj)( dsUint32_t dsmHandle);
dsInt16_t (* AFSdsmRegisterFS)( dsUint32_t dsmHandle, regFSData *regFilespaceP);
dsInt16_t (* AFSdsmBindMC)( dsUint32_t dsmHandle, dsmObjName *objNameP, dsmSendType sendType, mcBindKey *mcBindKeyP);
dsInt16_t (* AFSdsmSendObj)( dsUint32_t dsmHandle, dsmSendType sendType, void *sendBuff, dsmObjName *objNameP, ObjAttr *objAttrPtr, DataBlk *dataBlkPtr);
dsInt16_t (* AFSdsmChangePW)( dsUint32_t dsmHandle, char *oldPW, char *newPW);
#if 0
dsInt16_t (* AFSdsmCleanUp)( dsBool_t mtFlag);
dsInt16_t (* AFSdsmDeleteAccess)( dsUint32_t dsmHandle, dsUint32_t ruleNum);
dsInt16_t (* AFSdsmDeleteFS)( dsUint32_t dsmHandle, char *fsName, dsUint8_t repository);
dsInt16_t (* AFSdsmEndGetDataEx)( dsmEndGetDataExIn_t *dsmEndGetDataExInP, dsmEndGetDataExOut_t *dsmEndGetDataExOutP);
dsInt16_t (* AFSdsmEndSendObjEx)( dsmEndSendObjExIn_t *dsmEndSendObjExInP, dsmEndSendObjExOut_t *dsmEndSendObjExOutP);
dsInt16_t (* AFSdsmEndTxnEx)( dsmEndTxnExIn_t *dsmEndTxnExInP, dsmEndTxnExOut_t *dsmEndTxnExOutP);
dsInt16_t (* AFSdsmGroupHandler)( dsmGroupHandlerIn_t *dsmGroupHandlerInP, dsmGroupHandlerOut_t  *dsmGroupHandlerOutP);
dsInt16_t (* AFSdsmInitEx)( dsUint32_t *dsmHandleP, dsmInitExIn_t *dsmInitExInP, dsmInitExOut_t *dsmInitExOutP);
dsInt16_t (* AFSdsmLogEvent)( dsUint32_t dsmHandle, logInfo *lopInfoP);
dsInt16_t (* AFSdsmQueryAccess)( dsUint32_t dsmHandle, qryRespAccessData **accessListP, dsUint16_t *numberOfRules);
void      (* AFSdsmQueryApiVersionEx)( dsmApiVersionEx *apiVersionP);
dsInt16_t (* AFSdsmQueryCliOptions)( optStruct *optstructP);
dsInt16_t (* AFSdsmQuerySessOptions)( dsUint32_t dsmHandle, optStruct *optstructP);
dsInt16_t (* AFSdsmRenameObj)( dsmRenameIn_t *dsmRenameInP, dsmRenameOut_t *dsmRenameOutP);
dsInt16_t (* AFSdsmSetAccess)( dsUint32_t dsmHandle, dsmAccessType accessType, dsmObjName *objNameP, char *node, char *owner);
dsInt16_t (* AFSdsmSetUp)( dsBool_t mtFlag, envSetUp *envSetUpPi);
dsInt16_t (* AFSdsmUpdateFS)( dsUint32_t dsmHandle, char *fs, dsmFSUpd *fsUpdP, dsUint32_t fsUpdAct);
dsInt16_t (* AFSdsmUpdateObj)( dsUint32_t dsmHandle, dsmSendType sendType, void *sendBuff, dsmObjName *objNameP, ObjAttr *objAttrPtr, dsUint32_t objUpdAct);
#endif
/* <<< TSM function pointers. */

typedef struct s_delList {
  struct s_delList *next;
  ObjID           objId;
} delList;

static dsInt16_t buildList(
    dsUint32_t     dsmHandle,
    dsmObjName     *objNameP,
    delList        **llHeadPP,
    delList        **llTailPP)
{
   dsInt16_t            rc;
   qryArchiveData       queryBuffer;       /* for query Archive*/
   qryRespArchiveData   qaDataArea;
   DataBlk              qDataBlkArea;
   char                 descrStar[] = "*";
   delList              *lnew = NULL, *ll = NULL;

   queryBuffer.stVersion = qryArchiveDataVersion ;
   queryBuffer.objName = objNameP;
   queryBuffer.owner = xopenGbl.dsmSessInfo.owner;
   queryBuffer.insDateLowerBound.year = DATE_MINUS_INFINITE;
   queryBuffer.insDateUpperBound.year = DATE_PLUS_INFINITE;
   queryBuffer.expDateLowerBound.year = DATE_MINUS_INFINITE;
   queryBuffer.expDateUpperBound.year = DATE_PLUS_INFINITE;
   queryBuffer.descr = descrStar;

   if ((rc=AFSdsmBeginQuery(dsmHandle, qtArchive,
                        (void *)&queryBuffer )) != DSM_RC_OK)
   {
      XOPENRETURN(dsmHandle,"buildList(AFSdsmBeginQuery)",rc,__FILE__,__LINE__);
   }
   qaDataArea.stVersion = qryRespArchiveDataVersion;
   qDataBlkArea.stVersion = DataBlkVersion ;
   qDataBlkArea.bufferPtr = (char *)&qaDataArea;
   qDataBlkArea.bufferLen = sizeof(qryRespArchiveData);
   while ((rc = AFSdsmGetNextQObj(dsmHandle, &qDataBlkArea)) == DSM_RC_MORE_DATA)
   {
     if (!(lnew = (delList *)dsMalloc(sizeof(delList))))
        XOPENRETURN(dsmHandle,"buildList(AFSdsmGetNextQObj)",
                    DSM_RC_NO_MEMORY,__FILE__,__LINE__);

     if (!*llHeadPP)
     {
        *llHeadPP = lnew;
        *llTailPP = lnew;
     }
     else
     {
        ll = *llTailPP;
        ll->next = lnew;
        *llTailPP = lnew;
     }

     lnew->next = NULL;
     lnew->objId.hi = qaDataArea.objId.hi;
     lnew->objId.lo = qaDataArea.objId.lo;
   }

   if (rc != DSM_RC_FINISHED)
   {
      AFSdsmEndQuery(dsmHandle);
      XOPENRETURN(dsmHandle,"buildList(AFSdsmGetNextQObj)",
                  rc,__FILE__,__LINE__);
   }

   if ((rc = AFSdsmEndQuery(dsmHandle)) != DSM_RC_OK)
   {
      sprintf(traceStr2, "buildList: AFSdsmEndQuery rc = %d", rc);
      ourTrace(dsmHandle,TrFL,traceStr2);
   }
   XOPENRETURN(dsmHandle,"buildList",rc,__FILE__,__LINE__);
}

static dsInt16_t  freeList(
    delList    **llHeadPP,
    delList    **llTailPP)
{

   delList *ll;
   ll = *llHeadPP;
   while (ll)
   {
      *llHeadPP = ll->next;
      dsFree(ll);
      ll = *llHeadPP;
   }
   *llHeadPP = NULL;
   *llTailPP = NULL;
   XOPENRETURN(0,"freeList",0,__FILE__,__LINE__);
}

void ourTrace(long           BSAHandle,
              char          *fileName,
              int            lineNumber,
              char          *traceStr2)
{

   sprintf(traceStr,"%s (%d) ",fileName, lineNumber);

   if (traceStr2 != NULL && *traceStr2 != '\0')
      strcat(traceStr, traceStr2);

   AFSdsmTrace(BSAHandle, traceStr);
   return;
}

void ourLogEvent_Ex(dsUint32_t dsmHandle, dsmLogType type, char *message,
                    char *appMsg, dsmLogSeverity severity)
{
   dsmLogExIn_t dsmLogIn;
   dsmLogExOut_t dsmLogOut;
   dsInt16_t    rc = 0;
   memset(&dsmLogOut, '\0', sizeof(dsmLogExOut_t));

   if (dsmHandle)
   {
      dsmLogIn.stVersion = dsmLogExInVersion;
      dsmLogIn.severity = severity;
      dsmLogIn.logType = type;

      strcpy(dsmLogIn.appMsgID, appMsg);
      dsmLogIn.message = message;
      rc = AFSdsmLogEventEx(dsmHandle, &dsmLogIn, &dsmLogOut);
   }
}

char* ourRCMsg(dsInt16_t ourRC, char *msgPrefix)
{
   char         appStr[BSA_MAX_DESC];
   char         *chP;
   int          bytesToCp;

   memset(&ourMsg, 0x00, DSM_MAX_RC_MSG_LENGTH + 1);
   if(msgPrefix != NULL)
   {
       /*================================================================
        call stdXOpenMsgMap if return code values are within the TSM
        return code range (96 - 104). Reformat the message prefix
        (ANS0660 - ANS0668).
        ================================================================*/
        if ((ourRC >= custMin_retcode) && (ourRC <= custMax_retcode))
        {
             stdXOpenMsgMap(ourRC, ourMsg);
             /* set standard XOpen message prefix range: 660 - 668 */
             ourRC = ((ourRC * 10) / 16) + (ourRC % 16) + 600;
             sprintf(msgPrefix, "ANSO%d", ourRC);
        }
        else
          /*===============================================================
           call dsmRCMsg if return code values other then the above ranges.
           Get message prefix from the return message of dsmRCMsg.
           ===============================================================*/
        {
              AFSdsmRCMsg(NOHANDLE, ourRC, ourMsg);
             /*=========================================================
              search for the first ' ' and copy from beginning of string
              until before the two charecter before ' '.
              e.g. To get the code value from the messages of
                   "ANS1038S Invalid option specified", we only want to
                   get strings "ANS1038".
              ========================================================*/
              chP = (char *) strchr(ourMsg, ' ');
              bytesToCp = strlen(ourMsg) - (strlen(chP) + 1);
              strncpy(appStr, ourMsg, bytesToCp);
              sprintf(msgPrefix, "%s", appStr);
        }
   }
return ourMsg;
} /* ourRCMsg() */

void StrUpper(char *s)
{
   while (*s != '\0')
   {
      if (isalpha((int)(unsigned char)*s))
         *s = (char)toupper((int)(unsigned char)*s);
      s++;

   } /* end while */
} /* StrUpper() */

BSA_Int16 xparsePath(
    long           BSAHandle,
    char          *pathname,
    char          *hl,
    char          *ll)
{
   /*=== code taken from dscparse.c ParseDestOperand function ===*/

   dsInt16_t    opLen;
   dsInt16_t    x;

   *hl = *ll = '\0';

   strcpy(hl, pathname);   /* use hl as working area */
   if ((opLen = strlen(hl)) > 0)
   {
      /*=== Find the ll portion of the name ===*/
      #ifdef MBCS
      {
         char *llstart;
         llstart = strrchr(hl,DIR_DELIMITER);
         if (llstart == NULL)
            x = 0;
         else
            x = llstart - hl;
      }
      #else
         for (x = opLen-1; x>0 && hl[x]!=DIR_DELIMITER; x--);
      #endif

         /*=== If there is no leading delimiter then add one ===*/
         if (hl[x] != DIR_DELIMITER)
            strcpy(ll, STR_DIR_DELIMITER);

         strncat(ll, hl+x, opLen-x);

         hl[x] = '\0';          /* Shorten hl by length of ll */
   }
   return 0;
}

BSA_Int16 xlateRC(
    long           BSAHandle,
    BSA_Int16      dsmRC,
    BSA_Int16      *bsaRCP)

{
   switch (dsmRC)
   {
      case DSM_RC_OK:
         *bsaRCP=BSA_RC_OK;                              break;

      case DSM_RC_ABORT_ACTIVE_NOT_FOUND:
         *bsaRCP=BSA_RC_ABORT_ACTIVE_NOT_FOUND;          break;

      case DSM_RC_ABORT_SYSTEM_ERROR:
         *bsaRCP=BSA_RC_ABORT_SYSTEM_ERROR;              break;

      case DSM_RC_ABORT_BAD_VERIFIER:
      case DSM_RC_AUTH_FAILURE:
      case DSM_RC_REJECT_ID_UNKNOWN:
      case DSM_RC_REJECT_DUPLICATE_ID:
         *bsaRCP=BSA_RC_AUTHENTICATION_FAILURE;          break;

      case DSM_RC_BAD_CALL_SEQUENCE:
         *bsaRCP=BSA_RC_BAD_CALL_SEQUENCE;               break;

      case DSM_RC_INVALID_DS_HANDLE:
         *bsaRCP=BSA_RC_BAD_HANDLE;                      break;

      case DSM_RC_BUFF_TOO_SMALL:
         *bsaRCP=BSA_RC_BUFFER_TOO_SMALL;                break;

      case DSM_RC_DESC_TOOLONG:
         *bsaRCP=BSA_RC_DESC_TOO_LONG;                   break;

      case DSM_RC_FILESPACE_TOOLONG:
         *bsaRCP=BSA_RC_OBJECTSPACE_TOO_LONG;            break;

      /* some other condition here ? */
      case DSM_RC_PASSWD_TOOLONG:
         *bsaRCP=BSA_RC_INVALID_TOKEN;                   break;

      case DSM_RC_INVALID_VOTE:
         *bsaRCP=BSA_RC_INVALID_VOTE;                    break;

      case DSM_RC_INVALID_OPT:
         *bsaRCP=BSA_RC_INVALID_KEYWORD;                 break;

      /*  ? what conditions cause this - object already exists ?
      case DSM_RC_?:
         *bsaRCP=BSA_RC_MATCH_EXISTS;                    break;
      */

      case DSM_RC_MORE_DATA:
         *bsaRCP=BSA_RC_MORE_DATA;                       break;

      /* not supported - for QueryAccessRule
      case :
         *bsaRCP=BSA_RC_MORE_RULES;                      break;
      */

      case DSM_RC_NEWPW_REQD:
         *bsaRCP=BSA_RC_NEWTOKEN_REQD;                   break;

      case DSM_RC_ABORT_NO_MATCH:
      case DSM_RC_FILE_SPACE_NOT_FOUND:
      /*=== rc for query ===*/
         *bsaRCP=BSA_RC_NO_MATCH;                        break;
      /*=== if this return code comes on Delete, use OBJECT_NOT_FOUND
            - see xopndel.c ===*/

      case DSM_RC_FINISHED:
         *bsaRCP=BSA_RC_NO_MORE_DATA;                    break;

      case DSM_RC_ABORT_NO_LOG_SPACE:
      case DSM_RC_ABORT_NO_DB_SPACE:
      case DSM_RC_ABORT_NO_MEMORY:
      case DSM_RC_ABORT_NO_REPOSIT_SPACE:
      case DSM_RC_REJECT_NO_RESOURCES:
      case DSM_RC_REJECT_NO_MEMORY:
      case DSM_RC_REJECT_NO_DB_SPACE:
      case DSM_RC_REJECT_NO_LOG_SPACE:
         *bsaRCP=BSA_RC_NO_RESOURCES;                    break;

      case DSM_RC_NULL_DATABLKPTR:
         *bsaRCP=BSA_RC_NULL_DATABLKPTR;                 break;

      case DSM_RC_NULL_OBJNAME:
         *bsaRCP=BSA_RC_NULL_OBJNAME;                    break;

      case DSM_RC_NULL_BUFPTR:
         *bsaRCP=BSA_RC_NULL_POINTER;                    break;

      /* not supported - for DeleteAccessRule
      case :
         *bsaRCP=BSA_RC_NULL_RULEID;                     break;
      */

      case DSM_RC_HL_TOOLONG:
      case DSM_RC_LL_TOOLONG:
         *bsaRCP=BSA_RC_OBJECT_NAME_TOO_LONG;            break;

      /* not supported - for DeletePolicyDomain
      case :
         *bsaRCP=BSA_RC_OBJECT_NOT_EMPTY;                break;
      */

      /* same as NO_MATCH for DeleteObject
      case :
         *bsaRCP=BSA_RC_OBJECT_NOT_FOUND;                break;
      */

      case DSM_RC_OBJINFO_TOOLONG:
         *bsaRCP=BSA_RC_OBJINFO_TOO_LONG;                break;

      /* same as BSA_RC_OBJECT_NAME_TOO_LONG
      case :
         *bsaRCP=BSA_RC_OBJNAME_TOO_LONG;                break;
      */

      /* not supported - for CreatePolicySet, etc
      case :
         *bsaRCP=BSA_RC_OPERATION_NOT_AUTHORIZED;        break;
      */

      case DSM_RC_OLDPW_REQD:
         *bsaRCP=BSA_RC_OLDTOKEN_REQD;                   break;

      case DSM_RC_REJECT_VERIFIER_EXPIRED:
         *bsaRCP=BSA_RC_TOKEN_EXPIRED;                   break;

      case DSM_RC_WILL_ABORT:
      case DSM_RC_CHECK_REASON_CODE:
         *bsaRCP=BSA_RC_TXN_ABORTED;                     break;

      case DSM_RC_UNMATCHED_QUOTE:
         *bsaRCP=BSA_RC_UNMATCHED_QUOTE;                 break;

      /* not supported - for DeleteUser
      case :
         *bsaRCP=BSA_RC_USER_OWNS_OBJECT;                break;
      */

      default:
      {
         /*=========================================================
          No suitable match, so print message in log, if we still
          have a handle.  AFSdsmInit calls ApiCleanUp in certain error
          situations.  We lose the handle in those cases so check
          for it.
          =========================================================*/

         char         rcMsg[DSM_MAX_RC_MSG_LENGTH] ;
         char         errPrefix[DSM_MAX_RC_MSG_LENGTH + 1];
         char         ourMessage[DSM_MAX_RC_MSG_LENGTH + 1];
         dsUint32_t   dsmHandle;

         memset(errPrefix,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
         memset(ourMessage,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);

         if (BSAHandle)
         {
            dsmHandle = BSAHandle;
            AFSdsmRCMsg(dsmHandle,dsmRC,rcMsg) ;
            strcat(rcMsg, "\n");
            sprintf(traceStr2,
                   "xlateRC - %s", rcMsg);
            ourTrace(BSAHandle,TrFL, traceStr2);
            strcpy(ourMessage, ourRCMsg(dsmRC, errPrefix));
            ourLogEvent_Ex(BSAHandle, logLocal, ourMessage, errPrefix, logSevError);
         }

         *bsaRCP = ADSM_RC_ERROR;
      }
   }

   /*=== trace only if we have a valid handle ===*/
   if (dsmRC && BSAHandle)
   {
      sprintf(traceStr2, "xlateRC: TSM rc >%d< , BSA rc >%d<", dsmRC, *bsaRCP);
      ourTrace(BSAHandle, TrFL, traceStr2);
   }
   return DSM_RC_OK ;
}

void stdXOpenMsgMap(dsInt16_t  rc,  char *msg)
{
   switch(rc)
   {
      case ADSM_RC_ERROR:
           strcpy(msg, "TSM rc error, see ADSM error log.");
      case ADSM_RC_INVALID_NODE:
           strcpy(msg, "BSAObjectOwner doesn't match value session Init.");
           break;

      case ADSM_RC_INVALID_COPYTYPE:
           strcpy(msg, "Invalid copy type.");
           break;

      case ADSM_RC_INVALID_OBJTYPE:
           strcpy(msg, "Invalid object type.");
           break;

      case ADSM_RC_INVALID_STATUS:
           strcpy(msg, "Invalid Object Status.");
           break;

      case ADSM_RC_INVALID_ST_VER:
           strcpy(msg, "Invalid object descriptor structure version.");
           break;

      case ADSM_RC_OWNER_TOO_LONG:
           strcpy(msg, "Object owner name too long.");
           break;

      case ADSM_RC_PSWD_TOO_LONG:
           strcpy(msg, "Client password too long.");
           break;

     case ADSM_RC_PSWD_GEN:
          strcpy(msg, "Password input required .");
          break;

     default:
           strcpy(msg, "No message available");
           break;
   }
}

BSA_Int16 fillArchiveResp(
long                BSAHandle,
ObjectDescriptor   *BSAobjDescP,
qryRespArchiveData *respArchiveP
)
{
   XAPIObjInfo   *xapiObjInfoP;

   strcpy(BSAobjDescP->Owner.appObjectOwner, respArchiveP->owner);
   strcpy(BSAobjDescP->objName.objectSpaceName, respArchiveP->objName.fs);

   /*=== concatenate hl and ll for pathName ===*/
   strcpy(BSAobjDescP->objName.pathName, respArchiveP->objName.hl);
   strcat(BSAobjDescP->objName.pathName, respArchiveP->objName.ll);

   BSAobjDescP->createTime.tm_year = respArchiveP->insDate.year-1900;
   BSAobjDescP->createTime.tm_mon  = respArchiveP->insDate.month-1;
   BSAobjDescP->createTime.tm_mday = respArchiveP->insDate.day  ;
   BSAobjDescP->createTime.tm_hour = respArchiveP->insDate.hour ;
   BSAobjDescP->createTime.tm_min  = respArchiveP->insDate.minute ;
   BSAobjDescP->createTime.tm_sec  = respArchiveP->insDate.second ;

   BSAobjDescP->copyId.left  = respArchiveP->objId.hi;
   BSAobjDescP->copyId.right = respArchiveP->objId.lo;

   BSAobjDescP->restoreOrder.left  = respArchiveP->restoreOrderExt.lo_hi;
   BSAobjDescP->restoreOrder.right = respArchiveP->restoreOrderExt.lo_lo;

   strcpy(BSAobjDescP->lGName, respArchiveP->mcName) ;

   xapiObjInfoP = (XAPIObjInfo *)&(respArchiveP->objInfo);
   BSAobjDescP->size.left = xapiObjInfoP->size.left;
   BSAobjDescP->size.right= xapiObjInfoP->size.right;
   strcpy(BSAobjDescP->resourceType, xapiObjInfoP->resourceType);
   strcpy(BSAobjDescP->desc, xapiObjInfoP->partDesc);
   strcpy(BSAobjDescP->objectInfo, xapiObjInfoP->partObjInfo);

   if (respArchiveP->objName.objType == DSM_OBJ_DIRECTORY)
      BSAobjDescP->objectType = BSAObjectType_DIRECTORY;
   else
      BSAobjDescP->objectType = BSAObjectType_FILE;
      /*=== future - type DATABASE ?? ===*/

      BSAobjDescP->status = BSAObjectStatus_ACTIVE;
      XOPENRETURN(BSAHandle, "fillArchiveResp",
                  BSA_RC_SUCCESS,__FILE__,__LINE__);
}

BSA_Int16 fillBackupResp(
long               BSAHandle,
ObjectDescriptor  *BSAobjDescP,
qryRespBackupData *respBackupP
)
{
   XAPIObjInfo   *xapiObjInfoP;

   strcpy(BSAobjDescP->Owner.appObjectOwner, respBackupP->owner);
   strcpy(BSAobjDescP->objName.objectSpaceName, respBackupP->objName.fs);

   /*=== concatenate hl and ll for pathName ===*/
   strcpy(BSAobjDescP->objName.pathName, respBackupP->objName.hl);
   strcat(BSAobjDescP->objName.pathName, respBackupP->objName.ll);

   BSAobjDescP->createTime.tm_year = respBackupP->insDate.year-1900;
   BSAobjDescP->createTime.tm_mon  = respBackupP->insDate.month-1;
   BSAobjDescP->createTime.tm_mday = respBackupP->insDate.day  ;
   BSAobjDescP->createTime.tm_hour = respBackupP->insDate.hour ;
   BSAobjDescP->createTime.tm_min  = respBackupP->insDate.minute ;
   BSAobjDescP->createTime.tm_sec  = respBackupP->insDate.second ;

   BSAobjDescP->copyId.left  = respBackupP->objId.hi;
   BSAobjDescP->copyId.right = respBackupP->objId.lo;

   BSAobjDescP->restoreOrder.left  = respBackupP->restoreOrderExt.lo_hi;
   BSAobjDescP->restoreOrder.right = respBackupP->restoreOrderExt.lo_lo;

   strcpy(BSAobjDescP->lGName, respBackupP->mcName) ;

   xapiObjInfoP = (XAPIObjInfo *)&(respBackupP->objInfo);
   BSAobjDescP->size.left = xapiObjInfoP->size.left;
   BSAobjDescP->size.right= xapiObjInfoP->size.right;
   strcpy(BSAobjDescP->resourceType, xapiObjInfoP->resourceType);
   strcpy(BSAobjDescP->desc, xapiObjInfoP->partDesc);
   strcpy(BSAobjDescP->objectInfo, xapiObjInfoP->partObjInfo);

   if (respBackupP->objName.objType == DSM_OBJ_DIRECTORY)
      BSAobjDescP->objectType = BSAObjectType_DIRECTORY;
   else
      BSAobjDescP->objectType = BSAObjectType_FILE;
      /*=== future - type DATABASE ?? ===*/

   if (respBackupP->objState == DSM_ACTIVE)
      BSAobjDescP->status = BSAObjectStatus_ACTIVE;
   else
      BSAobjDescP->status = BSAObjectStatus_INACTIVE;
      /*=== ?? check for any other ===*/

   XOPENRETURN(BSAHandle, "fillRBackupResp",
               BSA_RC_SUCCESS,__FILE__,__LINE__);
}


afs_int32 dsm_MountLibrary()
{
void * dynlib = NULL ;

#ifdef DEBUG_BUTC
    printf("dsm_MountLibrary : inside function.  \n");
#endif
#if defined(AFS_AIX_ENV)
        dynlib = dlopen("/usr/lib/libApiDS.a(dsmapish.o)", RTLD_NOW | RTLD_LOCAL | RTLD_MEMBER);
#elif defined (AFS_AMD64_LINUX26_ENV)
	dynlib = dlopen("/usr/lib64/libApiTSM64.so", RTLD_NOW | RTLD_LOCAL);
#elif defined(AFS_SUN5_ENV) || defined(AFS_LINUX26_ENV)
        dynlib = dlopen("/usr/lib/libApiDS.so", RTLD_NOW | RTLD_LOCAL);
#else
        dynlib = NULL;
#endif

    	if (dynlib == NULL) {
        	ELog(0,"dsm_MountLibrary: The dlopen call to load the libApiDS shared library failed\n");
        	return(BUTX_NOLIBRARY);
    	}

#ifdef DEBUG_BUTC
    	printf("dsm_MountLibrary : SUCCESS to Open the libApiDS shared library. \n");
#endif
#if defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_LINUX26_ENV)
	AFSdsmBeginQuery = (dsInt16_t (*)( dsUint32_t dsmHandle, dsmQueryType queryType, dsmQueryBuff *queryBuffer)) dlsym((void *)dynlib, "dsmBeginQuery");
	AFSdsmGetNextQObj = (dsInt16_t (*)( dsUint32_t dsmHandle, DataBlk *dataBlkPtr))dlsym((void *)dynlib, "dsmGetNextQObj") ;
	AFSdsmEndQuery = (dsInt16_t (*)( dsUint32_t dsmHandle))dlsym((void *)dynlib, "dsmEndQuery");
	AFSdsmRCMsg = (dsInt16_t (*)( dsUint32_t dsmHandle, dsInt16_t dsmRC, char *msg))dlsym((void *)dynlib, "dsmRCMsg");
	AFSdsmLogEventEx = (dsInt16_t (*)( dsUint32_t dsmHandle, dsmLogExIn_t *dsmLogExInP, dsmLogExOut_t *dsmLogExOutP))dlsym((void *)dynlib, "dsmLogEventEx");
	AFSdsmTrace = (dsInt16_t (*)( dsUint32_t dsmHandle, char * string ))dlsym((void *)dynlib, "dsmTrace");
	AFSdsmTerminate = (dsInt16_t (*)( dsUint32_t dsmHandle))dlsym((void *)dynlib, "dsmTerminate");
	AFSdsmSendData = (dsInt16_t (*)( dsUint32_t dsmHandle, DataBlk *dataBlkPtri))dlsym((void *)dynlib, "dsmSendData");
	AFSdsmBeginTxn = (dsInt16_t (*)( dsUint32_t dsmHandle))dlsym((void *)dynlib, "dsmBeginTxn");
	AFSdsmDeleteObj = (dsInt16_t (*)( dsUint32_t dsmHandle, dsmDelType delType, dsmDelInfo delInfo))dlsym((void *)dynlib, "dsmDeleteObj");
	AFSdsmEndTxn = (dsInt16_t (*)( dsUint32_t dsmHandle, dsUint8_t vote, dsUint16_t *reason))dlsym((void *)dynlib, "dsmEndTxn");
	AFSdsmQueryApiVersion = (void (*)( dsmApiVersion *apiVersionP))dlsym((void *)dynlib, "dsmQueryApiVersion");
	AFSdsmInit = (dsInt16_t (*)( dsUint32_t *dsmHandle, dsmApiVersion *dsmApiVersionP, char *clientNodeNameP, char *clientOwnerNameP, char *clientPasswordP, char *applicationType, char *configfile, char *options))dlsym((void *)dynlib, "dsmInit");
	AFSdsmQuerySessInfo = (dsInt16_t (*)( dsUint32_t dsmHandle, ApiSessInfo *SessInfoP))dlsym((void *)dynlib, "dsmQuerySessInfo");
	AFSdsmBeginGetData = (dsInt16_t (*)(dsUint32_t dsmHandle, dsBool_t mountWait, dsmGetType getType, dsmGetList *dsmGetObjListP))dlsym((void *)dynlib, "dsmBeginGetData");
	AFSdsmGetObj = (dsInt16_t (*)( dsUint32_t dsmHandle, ObjID *objIdP, DataBlk *dataBlkPtr))dlsym((void *)dynlib, "dsmGetObj");
	AFSdsmEndGetObj = (dsInt16_t (*)( dsUint32_t dsmHandle))dlsym((void *)dynlib, "dsmEndGetObj");
	AFSdsmEndGetData = (dsInt16_t (*)( dsUint32_t dsmHandle))dlsym((void *)dynlib, "dsmEndGetData");
	AFSdsmGetData = (dsInt16_t (*)( dsUint32_t dsmHandle, DataBlk *dataBlkPtr))dlsym((void *)dynlib, "dsmGetData");
	AFSdsmEndSendObj = (dsInt16_t (*)( dsUint32_t dsmHandle))dlsym((void *)dynlib, "dsmEndSendObj");
	AFSdsmRegisterFS = (dsInt16_t (*)( dsUint32_t dsmHandle, regFSData *regFilespaceP))dlsym((void *)dynlib, "dsmRegisterFS");
	AFSdsmBindMC = (dsInt16_t (*)( dsUint32_t dsmHandle, dsmObjName *objNameP, dsmSendType sendType, mcBindKey *mcBindKeyP))dlsym((void *)dynlib, "dsmBindMC");
	AFSdsmSendObj = (dsInt16_t (*)( dsUint32_t dsmHandle, dsmSendType sendType, void *sendBuff, dsmObjName *objNameP, ObjAttr *objAttrPtr, DataBlk *dataBlkPtr))dlsym((void *)dynlib, "dsmSendObj");
	AFSdsmChangePW = (dsInt16_t (*)( dsUint32_t dsmHandle, char *oldPW, char *newPW))dlsym((void *)dynlib, "dsmChangePW");


	if (	!AFSdsmBeginQuery || !AFSdsmGetNextQObj || !AFSdsmEndQuery ||
		!AFSdsmRCMsg || !AFSdsmLogEventEx || !AFSdsmTrace ||
		!AFSdsmTerminate || !AFSdsmEndGetObj || !AFSdsmSendData ||
		!AFSdsmBeginTxn || !AFSdsmDeleteObj || !AFSdsmEndGetData ||
        	!AFSdsmEndTxn || !AFSdsmQueryApiVersion || !AFSdsmInit ||
		!AFSdsmGetData || !AFSdsmQuerySessInfo || !AFSdsmBeginGetData ||
		!AFSdsmGetObj || !AFSdsmEndSendObj || !AFSdsmRegisterFS ||
		!AFSdsmBindMC || !AFSdsmSendObj || !AFSdsmChangePW )
	{
        	ELog(0,"dsm_MountLibrary: The dlopen call to load the TSM shared library failed\n");
        	return(BUTX_NOLIBRARY);
    	}
#ifdef DEBUG_BUTC
    		printf("dsm_MountLibrary : All TSM function pointers initialized. \n");
#endif

#endif
	dsm_init = 1 ;
#ifdef DEBUG_BUTC
    printf("dsm_MountLibrary : leaving function. \n");
#endif
	return 0 ;

}

BSA_Int16 BSAInit( long     *BSAHandleP,
                   SecurityToken  *clientPasswordP,
                   ObjectOwner    *objectOwnerP,
                   char          **envP )
{
   dsInt16_t        rc = 0;
   dsInt16_t        saverc = 0;
   BSA_Int16        bsaRC = 0;
   dsUint32_t       dsmHandle;   /* use diff field when talking to base
                                   code in case either format changes */
   dsmApiVersion    xapiHdrVer;  /* Xopen API build version           */
   dsmApiVersion    apiLibVer;  /* Xopen API Libary version          */

   dsUint32_t       xapiVersion, apiVersion;
   envSetUp         dsmEnvSetUp;

   /*==================================================================
     Today the BSA constants have the same values as the DSM constants.
     If this changes, use strncpy.
     =================================================================*/

   char            rcmsg[DSM_MAX_RC_MSG_LENGTH + 1];
   char            dsmNode[DSM_MAX_NODE_LENGTH];
   char            dsmOwner[DSM_MAX_OWNER_LENGTH];
   char            dsmPswd[DSM_MAX_VERIFIER_LENGTH];
   char           *dsmNodeP;
   char           *dsmOwnerP;
   char           *dsmPswdP;
   char            options[DSM_MAX_RC_MSG_LENGTH];
   char           *optionP = NULL;
   char            TDPLibName[BSA_MAX_DESC];
   char            TDPILibName[BSA_MAX_DESC];
   int             i=0;
   char            errPrefix[DSM_MAX_RC_MSG_LENGTH + 1];
   char            ourMessage[DSM_MAX_RC_MSG_LENGTH + 1];
   char            rcMsg[DSM_MAX_RC_MSG_LENGTH + 1];

   dsmHandle = 0;
  *BSAHandleP = 0;

#ifdef DEBUG_BUTC
   printf("BSAInit : inside function. \n");
#endif
   memset(errPrefix,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(ourMessage,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(rcMsg,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   /*==================================================================
    Trace won't work before AFSdsmInit, moved to after call to AFSdsmInit
    The XOpen library is built by statically linking with the API and
    client common modules.  So, the version checking is unnecessary.
    We'll fill in the value anyway for consistency.
    ==================================================================*/
   memset(&xapiHdrVer,0x00,sizeof(dsmApiVersion));
   memset(&apiLibVer,0x00,sizeof(dsmApiVersion));
   memset(&dsmEnvSetUp,0x00,sizeof(envSetUp));

   AFSdsmQueryApiVersion(&apiLibVer);

   xapiHdrVer.version = DSM_API_VERSION;  /* Set the applications    */
   xapiHdrVer.release = DSM_API_RELEASE;  /* compile time version.   */
   xapiHdrVer.level   = DSM_API_LEVEL;    /* version                 */

   xapiVersion = (1000 * DSM_API_VERSION) + (100 * DSM_API_RELEASE)
                  + DSM_API_LEVEL;

   apiVersion  = (1000 * apiLibVer.version) + (100 * apiLibVer.release)
                  + apiLibVer.level;

   /* check for compatibility problems */
   if (apiVersion < xapiVersion)
   	return ADSM_RC_ERROR;

   /*==== Init global area ===*/

   memset(&xopenGbl,0x00, sizeof(xGlobal));  /* Zero out block.        */

   memset(dsmNode,0x00,DSM_MAX_NODE_LENGTH);
   memset(dsmOwner,0x00,DSM_MAX_OWNER_LENGTH);
   memset(dsmPswd,0x00,DSM_MAX_VERIFIER_LENGTH);
   memset(options,0x00,DSM_MAX_RC_MSG_LENGTH);
   memset(TDPLibName,0x00,BSA_MAX_DESC);
   memset(TDPILibName,0x00,BSA_MAX_DESC);

   memset(rcmsg,         '\0', DSM_MAX_RC_MSG_LENGTH + 1);


   /*=== Validate the inputs ===*/

   if (objectOwnerP)
   {
      if (strlen(objectOwnerP->bsaObjectOwner) > BSA_MAX_BSAOBJECT_OWNER)
      {
         bsaRC = ADSM_RC_OWNER_TOO_LONG;
         return(bsaRC);
      }

      if (strlen(objectOwnerP->appObjectOwner) > BSA_MAX_APPOBJECT_OWNER)
      {
         bsaRC = ADSM_RC_OWNER_TOO_LONG;
         return(bsaRC);
      }

      if (objectOwnerP->bsaObjectOwner[0] == '\0') /* empty string */
         dsmNodeP = NULL;
      else
      {
         strcpy(dsmNode, objectOwnerP->bsaObjectOwner);
         dsmNodeP = dsmNode;
      }

      if (objectOwnerP->appObjectOwner[0] == '\0') /* empty string  */
         dsmOwnerP = NULL;
      else
      {
         strcpy(dsmOwner, objectOwnerP->appObjectOwner);
         dsmOwnerP = dsmOwner;
      }
   }
   else
   {
      dsmNodeP = NULL;
      dsmOwnerP = NULL;
   }

   if (clientPasswordP != NULL)
   {
      if (strlen((const char *)clientPasswordP) > BSA_MAX_TOKEN_SIZE)
      {
         bsaRC = ADSM_RC_PSWD_TOO_LONG;
         return(bsaRC);
      }

      strcpy(dsmPswd, (const char *)clientPasswordP);
      dsmPswdP = dsmPswd;
   }
   else
      dsmPswdP = NULL;
      {
          while ((envP[i] != NULL) && (envP[i] != '\0'))
          {
             strcat(options, "-");
             strcat(options, envP[i]);
             if ((envP[i+1] != NULL) && (envP[i+1] != '\0'))
                strcat(options, " ");
                i++;
          }

          if (options[0] == '\0')   /* empty string */
             optionP = NULL;
          else
             optionP = options;

          rc = AFSdsmInit(&dsmHandle,   /* On return contains session handle.   */
                       &xapiHdrVer,   /* Version of the API we are using.     */
                       dsmNodeP,      /* node name                            */
                       dsmOwnerP,     /* owner name                           */
                       dsmPswdP,      /* password                             */
                       APPLTYPE,      /* Name of our node type.               */
                       NULL,          /* no API config file name              */
                       optionP);      /* option string                        */
     }

     if (rc == DSM_RC_OK)
     {
        /*=== now that AFSdsmInit issued, we can trace ===*/
        sprintf(traceStr2,
                "BSAInit: original bsaOwner=%s, appOwner=%s, token=%s, Appl Type=%s, options=%s.",
                 dsmNode, dsmOwner, dsmPswd, APPLTYPE, options);
        ourTrace(dsmHandle, TrFL, traceStr2);
        {
              strcpy(TDPLibName, TDP_LIBNAME);
              sprintf(traceStr2,
                     "BSAInit: Using TSM Native API library : %s", TDPLibName);
              ourTrace(dsmHandle, TrFL, traceStr2);
        }
     }


     /*=================================================================
     If the password expired, continue initialization so the session
     can continue.

     Save the 'password expired' rc away, so we can return it to the
     caller.

     The application will be responsible for issuing a BSAChangeToken()
     to update the password.
     =================================================================*/
     if (rc == DSM_RC_REJECT_VERIFIER_EXPIRED)
     {
        /*=== don't return yet - init global so session can continue ===*/

         saverc = rc;
         rc = DSM_RC_OK;
     }
     else
        if ((rc == DSM_RC_NO_OWNER_REQD) ||
            (rc == DSM_RC_NO_NODE_REQD))       /* pswd=generate */
        {
            AFSdsmTerminate(dsmHandle);
            bsaRC = ADSM_RC_PSWD_GEN;
            return (bsaRC);
        }


   /*==================================================================
     If we ran into any problems so far, rc will be non-zero or 'true'.

     This is not true for password expired, since we reset rc to zero
     in that case.  We'll have to check for that later so we return
     the 'password expired' indication to the caller.
     ==================================================================*/

   if (rc)
   {
      AFSdsmRCMsg(dsmHandle, rc, rcmsg);
      sprintf(traceStr2, "BSAInit(AFSdsmInit): Messages - %s ", rcmsg);
      ourTrace(dsmHandle, TrFL, traceStr2);
      strcpy(ourMessage, ourRCMsg(rc, errPrefix));
      ourLogEvent_Ex(dsmHandle, logLocal, ourMessage, errPrefix, logSevError);
      xlateRC(*BSAHandleP, rc, &bsaRC); /* BSAHandle still 0, but not used */
      AFSdsmTerminate(dsmHandle);          /* clean up memory blocks          */
      return (bsaRC);
   }


   {
      char msg[256];                    /* format string and watch for NULLs */

      strcpy(msg, "BSAInit for node=");

      if (objectOwnerP)
      {
         if (dsmNodeP)
            strcat(msg, dsmNodeP);
         else
            strcat(msg, "<empty>");
      }
      else
         strcat(msg, "<NULL>");

      strcat(msg, ", owner=");

      if (objectOwnerP)
      {
         if (dsmOwnerP)
            strcat(msg, dsmOwnerP);
         else
            strcat(msg, "<empty>");
      }
      else
         strcat(msg, "<NULL>");

      strcat(msg, ".\n");

      sprintf(traceStr, msg);
   }

   *BSAHandleP = (long)dsmHandle;


   /*=================================================================
    For password expired processing the application is responsible for
    issuing BSAChangeToken() to update the password, when they see the
    'expired password' return code.

    The BSAChangeToken() command will issue the AFSdsmQuerySessInfo()
    call, so don't issue it here.
    =================================================================*/

   if (!saverc)               /* don't call this if we got pswd expire */
   {
      /*=== now query session info to populate the global structure ===*/

      xopenGbl.dsmSessInfo.stVersion = ApiSessInfoVersion;
      rc = AFSdsmQuerySessInfo(dsmHandle,                /* Session handle */
                            &xopenGbl.dsmSessInfo);   /* Output struct  */

      if (rc)
      {
         sprintf(traceStr2, "BSAInit(AFSdsmQuerySessInfo): error rc = %d", rc);
         ourTrace(dsmHandle, TrFL, traceStr2);
         AFSdsmTerminate(dsmHandle);
         *BSAHandleP = 0;
      }
      else
      {
         sprintf(traceStr2, "BSAInit: Actual node=%s, actual DSM owner=%s, servername=%s.",
                 xopenGbl.dsmSessInfo.id, xopenGbl.dsmSessInfo.owner,
                 xopenGbl.dsmSessInfo.adsmServerName);
         ourTrace(dsmHandle, TrFL, traceStr2);
      }
   }
   else
   {
      /*============================================================
        Indicate in the session flags that we encountered a password
        expiration condition.  The application will know that this
        happened via the return code.
        ===========================================================*/

      xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_PSWD_EXPIRE);
   }

   /*=== Save bsaObjectOwner (node name) value passed on Init ===*/

   if (dsmNodeP)
      strcpy(xopenGbl.bsaObjectOwner, dsmNodeP);

     /*================================================================
      Check to see if we saved away the 'expired password' return code.
      if so, return it to the caller.
      ================================================================*/

   if (saverc)
      rc = saverc;

   xlateRC(*BSAHandleP, rc, &bsaRC);
#ifdef DEBUG_BUTC
   	printf("BSAInit : Leaving Function. \n");
#endif
   return (bsaRC);
}

BSA_Int16 BSATerminate(
    long              BSAHandle
)
{
   dsInt16_t      rc = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   dsmHandle = BSAHandle;
   sprintf(traceStr2, "BSATerminate ENTRY: BSAHandle is %d.",
                      BSAHandle);
   ourTrace(dsmHandle, TrFL, traceStr2);

   rc = AFSdsmTerminate(dsmHandle);
   dsmHandle = 0;
   xlateRC(dsmHandle, rc, &bsaRC);

   return (bsaRC);

}

BSA_Int16 BSAChangeToken(
   long           BSAHandle,
   SecurityToken *oldTokenP,
   SecurityToken *newTokenP
)
{
   dsInt16_t      rc = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   dsmHandle = BSAHandle;

   sprintf(traceStr2, "BSAChangeToken ENTRY: BSAHandle:%d old:>%s< new:>%s<",
           BSAHandle,oldTokenP,newTokenP);
   ourTrace(dsmHandle, TrFL, traceStr2);

   rc = AFSdsmChangePW(dsmHandle, (char *)oldTokenP, (char *)newTokenP);

   if (rc)
   {
      xlateRC(BSAHandle, rc, &bsaRC);
      XOPENRETURN(BSAHandle,"BSAChangeToken(AFSdsmChangePW)",
                  bsaRC,__FILE__,__LINE__);
   }

   xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_PSWD_EXPIRE); /* set off */

    /*=== now query session info to populate the global structure ===*/

   xopenGbl.dsmSessInfo.stVersion = ApiSessInfoVersion;
   rc = AFSdsmQuerySessInfo(dsmHandle,                /* Our session handle  */
                         &xopenGbl.dsmSessInfo);   /* Output structure.   */
   if (rc)
   {
      /*=== appl should call Terminate ===*/
      sprintf(traceStr2, "BSAChangeToken(AFSdsmQuerySessInfo): error rc = %d", rc);
      ourTrace(BSAHandle, TrFL, traceStr2);
   }
   xlateRC(BSAHandle, rc, &bsaRC);
   XOPENRETURN(BSAHandle,"BSAChangeToken",
               bsaRC,__FILE__,__LINE__);
}

BSA_Int16 BSASetEnvironment(
    long     BSAHandle,
    char           **envP
)
{
   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   sprintf(traceStr2, "BSASetEnvironment ENTRY: BSAHandle:%d envP:>%p< ",
           BSAHandle,envP);
   ourTrace(BSAHandle, TrFL, traceStr2);
   XOPENRETURN(BSAHandle,"BSASetEnvironment",
               BSA_RC_BAD_CALL_SEQUENCE,__FILE__,__LINE__);
}

BSA_Int16 BSAGetEnvironment(
   long           BSAHandle,
   ObjectOwner   *objOwnerP,
   char         **envP
)
{
   dsInt16_t   rc = 0;
   BSA_Int16   bsaRC = 0;
   dsUint32_t  dsmHandle;
   char        envString[ADSM_ENV_STRS][BSA_MAX_DESC];
   char        maxObjStr[6];  /* conversion field. value range is 16-256 */
   dsUint32_t  maxObjNum;
   dsInt16_t   i, j;

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   for (i=0; i<ADSM_ENV_STRS; i++)
   {
      memset(envString[i], 0x00, BSA_MAX_DESC);
   }
   sprintf(traceStr2, "BSAGetEnvironment ENTRY: BSAHandle:%d ObjOwner:'%s' appOwner:'%s' envP:>%p<.",
           BSAHandle,
           objOwnerP->bsaObjectOwner,
           objOwnerP->appObjectOwner,
           envP);
   ourTrace(BSAHandle, TrFL, traceStr2);

   dsmHandle = BSAHandle;

   /*===============================================================
    check if BSAInit has been done for now we have only one possible
    handle value - change in future
    ==============================================================*/
   if ((dsmHandle != FIRST_HANDLE) ||
      (xopenGbl.dsmSessInfo.stVersion != ApiSessInfoVersion))
       XOPENRETURN(BSAHandle,"BSAGetEnvironment",
                   BSA_RC_BAD_CALL_SEQUENCE, __FILE__,__LINE__);

   if (objOwnerP)
   {
      strcpy(objOwnerP->bsaObjectOwner, xopenGbl.dsmSessInfo.id);
      strcpy(objOwnerP->appObjectOwner, xopenGbl.dsmSessInfo.owner);
   }
   else
      XOPENRETURN(BSAHandle,"BSAGetEnvironment",
                  BSA_RC_NULL_POINTER, __FILE__,__LINE__);

   rc = AFSdsmQuerySessInfo(dsmHandle,                   /* Session Handle */
                         &xopenGbl.dsmSessInfo);       /* Output struct  */

   if (rc)
      {
         sprintf(traceStr2, "BSAGetEnvironment(AFSdsmQuerySessInfo): error rc = %d", rc);
         ourTrace(dsmHandle, TrFL, traceStr2);
         AFSdsmTerminate(dsmHandle);
        /* *BSAHandleP = 0; */
      }
      else
      {
         sprintf(traceStr2, "BSAGetEnvironment: Actual node=%s, actual DSM owner=%s, ServerName=%s.",
                 xopenGbl.dsmSessInfo.id, xopenGbl.dsmSessInfo.owner, xopenGbl.dsmSessInfo.adsmServerName);
         ourTrace(dsmHandle, TrFL, traceStr2);
      }

   strcpy(envString[0],"TSMSRVR=");
   strcat(envString[0],xopenGbl.dsmSessInfo.serverHost);

   strcpy(envString[1],"TSMMAXOBJ=");
   maxObjNum = xopenGbl.dsmSessInfo.maxObjPerTxn;  /* convert to 32 bit */
   sprintf(maxObjStr,"%lu", maxObjNum );
   strcat(envString[1], maxObjStr);

   strcpy(envString[2], "TSMSRVRSTANZA=");
   strcat(envString[2], xopenGbl.dsmSessInfo.adsmServerName);

   sprintf(traceStr2, "BSAGetEnvironment Value : TSM Server '%s', TSM Max Object '%s',TSM Server STANZA '%s'",
         envString[0], envString[1], envString[2]);
   ourTrace(BSAHandle, TrFL, traceStr2);

   for (i=0; i< ADSM_ENV_STRS; i++) {
     if ( *envP == NULL ) {  /* watch for NULL pointers */
       /* Allocating memory for *envP.*/
       *envP = (char *) malloc(sizeof(char) * BSA_MAX_DESC +1);

       /* copy the content of envString[i] to *envP. */
        strcpy(*envP,envString[i]);
        envP++;
     }
   }

   xlateRC(BSAHandle, rc, &bsaRC);
   XOPENRETURN(BSAHandle,"BSAGetEnvironment",
               bsaRC,__FILE__,__LINE__)
}

BSA_Int16 BSABeginTxn(
    long             BSAHandle
)
{
   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   sprintf(traceStr2, "BSABeginTxn ENTRY: BSAHandle:%d", BSAHandle);
   ourTrace(BSAHandle, TrFL, traceStr2);
  /*========================================================
   don't actually issue BeginTxn yet, because we will do our
   own for Get and Query
   ========================================================*/

   xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_IN_BSA_TXN); /* set on */
   XOPENRETURN(BSAHandle, "BSABeginTxn",
               BSA_RC_SUCCESS,__FILE__,__LINE__);
}

BSA_Int16 BSAEndTxn(
    long           BSAHandle,
    Vote           vote
)
{
   dsInt16_t      rc = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;
   dsUint8_t      dsmVote;
   dsUint16_t     reason ;
   char           rsMsg[DSM_MAX_RC_MSG_LENGTH + 1];
   char           errPrefix[DSM_MAX_RC_MSG_LENGTH + 1];
   char           ourMessage[DSM_MAX_RC_MSG_LENGTH + 1];

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   memset(errPrefix,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(rsMsg,        '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(ourMessage,    '\0', DSM_MAX_RC_MSG_LENGTH + 1);

   sprintf(traceStr2, "BSAEndTxn ENTRY: BSAHandle:%d Vote:>%d<", BSAHandle, vote);
   ourTrace(BSAHandle, TrFL, traceStr2);

   dsmHandle = BSAHandle;

   if (xopenGbl.sessFlags & FL_IN_DSM_TXN)
   {
      if (vote == BSAVote_COMMIT)
         dsmVote = DSM_VOTE_COMMIT;
      else
         if (vote == BSAVote_ABORT)
            dsmVote = DSM_VOTE_ABORT;
         else
         {
            sprintf(traceStr2, "BSAEndTxn: invalid vote (%d)", vote);
            ourTrace(BSAHandle,TrFL, traceStr2);
            bsaRC = BSA_RC_INVALID_VOTE;
            XOPENRETURN(BSAHandle, "BSAEndTxn",bsaRC,__FILE__,__LINE__);
         }


      sprintf(traceStr2, "BSAEndTxn: issue AFSdsmEndTxn, vote=%s",
                  dsmVote == DSM_VOTE_COMMIT ? "COMMIT" : "ABORT");
      ourTrace(BSAHandle,TrFL, traceStr2);
      /*===========================================================
        check if this EndTxn call was proceeded by a Send call which
        returned WILL_ABORT. If so, make sure the vote is COMMIT
        so we will get the server's reason code.
        ===========================================================*/
      if (xopenGbl.sessFlags & FL_RC_WILL_ABORT)
      {
         dsmVote = DSM_VOTE_COMMIT;

         sprintf(traceStr2, "BSAEndTxn: sproceeded by RC_WILL_ABORT, so use vote=COMMIT");
         ourTrace(BSAHandle,TrFL, traceStr2);
         xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_RC_WILL_ABORT); /* set off*/
      }

      rc = AFSdsmEndTxn(dsmHandle, dsmVote, &reason);
      xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_DSM_TXN); /* set off */

      /*====================================================
       if the caller wanted to Abort, then return rc=OK, not
       our usual 2302 and reason 3
       =====================================================*/
      if ((dsmVote ==  DSM_VOTE_ABORT)     &&
          (rc == DSM_RC_CHECK_REASON_CODE) &&
          (reason == DSM_RC_ABORT_BY_CLIENT))

         rc = DSM_RC_OK;

      if (rc)
      {
         sprintf(traceStr2, "BSAEndTxn(AFSdsmEndTxn) error rc = %d", rc);
         ourTrace(BSAHandle,TrFL, traceStr2);

         AFSdsmRCMsg(BSAHandle, reason, rsMsg);
         sprintf(traceStr2, "BSAEndTxn: reason code = %d, Message='%s'", reason, rsMsg);
         ourTrace(BSAHandle,TrFL, traceStr2);

         strcpy(ourMessage, ourRCMsg(reason, errPrefix));
         ourLogEvent_Ex(BSAHandle,logBoth, ourMessage, errPrefix, logSevError);
      }
      xlateRC(BSAHandle, rc, &bsaRC);

      if (rc == DSM_RC_CHECK_REASON_CODE) /* return reason code instead */
      {
         xlateRC(BSAHandle, reason, &bsaRC);
      }
   }

   /*=== check if Query was terminated ===*/
   if (xopenGbl.sessFlags & FL_IN_BSA_QRY)
   {
      AFSdsmEndQuery(dsmHandle);
      xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_BSA_QRY); /* set off */
   }

   xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_BSA_TXN); /* set off */
   XOPENRETURN(BSAHandle, "BSAEndTxn",
               bsaRC,__FILE__,__LINE__);
}

void  BSAQueryApiVersion(
ApiVersion       *BSAapiVerP
)

{
   /*===============================================================
    Don't actually call the base QueryApiVersion call.
    Use the defines from custom.h since this is what the application
    sees in the header file. The constants should be the same,
    but may get out of sync occasionally.
    ===============================================================*/
   BSAapiVerP->version = BSA_API_VERSION;
   BSAapiVerP->release = BSA_API_RELEASE;
   BSAapiVerP->level   = BSA_API_LEVEL;
   return;
}

BSA_Int16 BSAQueryObject(
long              BSAHandle,
QueryDescriptor  *BSAqryDescP,
ObjectDescriptor *BSAobjDescP
)
{
   dsInt16_t      rc = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;
   dsmObjName     objName;
   dsUint8_t      dsmObjType;
   dsmQueryType   query_type;
   dsmQueryBuff   *query_buff;
   DataBlk        qData;
   qryArchiveData archiveData;
   qryRespArchiveData respArchive;
   qryBackupData  backupData;
   qryRespBackupData  respBackup;
   BSAObjectOwner upperNode;  /* upper cased node name */

   char           errPrefix[DSM_MAX_RC_MSG_LENGTH + 1];
   char           ourMessage[DSM_MAX_RC_MSG_LENGTH + 1];

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   memset(errPrefix,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(ourMessage,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);

   dsmHandle = BSAHandle;

   memset(&backupData, 0x00, sizeof(qryBackupData));

   sprintf(traceStr2, "BSAQueryObject ENTRY: BSAHandle:%d ObjOwner(qryDesc):'%s' appOwner(qryDesc):'%s' \n ObjName(qryDesc):'%.*s%.*s' \n copyType:%d ObjectType:%d status:%d ",
          BSAHandle,
          BSAqryDescP->owner.bsaObjectOwner,
          BSAqryDescP->owner.appObjectOwner,
          100,BSAqryDescP->objName.objectSpaceName,
          100,BSAqryDescP->objName.pathName,
          BSAqryDescP->copyType,
          BSAqryDescP->objectType,
          BSAqryDescP->status);
   ourTrace(BSAHandle, TrFL, traceStr2);

   /*=====================================================
    init objDesc area to 0's before we start to erase any
    garbage that might be there.
    =====================================================*/
   memset(BSAobjDescP, 0x00, sizeof(ObjectDescriptor));

   /*=== init global look ahead pointer to NULL ===*/
   if (xopenGbl.nextQryP != NULL)
   {
      dsFree(xopenGbl.nextQryP);
      xopenGbl.nextQryP = NULL;
   }

   /*=========================================================
    if the node name is different - we won't query it compare
    both the value passed on the BSAInit and the final value
    used for the session (which may be different for Generate).
    =========================================================*/
   strcpy(upperNode, BSAqryDescP->owner.bsaObjectOwner);
   StrUpper(upperNode);
   if ((strcmp(upperNode, xopenGbl.dsmSessInfo.id)) &&
      (strcmp(BSAqryDescP->owner.bsaObjectOwner, xopenGbl.bsaObjectOwner)))
   {
      sprintf(traceStr2,
      "BSAQueryObject: BSAObjectOwner(%s) doesn't match value for session(%s).", upperNode, xopenGbl.dsmSessInfo.id);
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = ADSM_RC_INVALID_NODE;
      strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
      ourLogEvent_Ex(BSAHandle, logLocal, ourMessage, errPrefix, logSevError);
      XOPENRETURN(BSAHandle, "BSAQueryObject",
                  bsaRC,__FILE__,__LINE__);
   }

   /*=====================================================
     currently BSA_MAX_OSNAME = DSM_MAX_FSNAME_LENGTH
     if this changes, use strncpy
     ====================================================*/
   if (strlen(BSAqryDescP->objName.objectSpaceName) > BSA_MAX_OSNAME)
   {
      sprintf(traceStr2, "BSAQueryObject: objectSpaceName too long (%d).",
              strlen(BSAqryDescP->objName.objectSpaceName));
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = BSA_RC_OBJNAME_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSAQueryObject",
                  bsaRC,__FILE__,__LINE__);
   }
   /*=================================================================
     the entire pathname gets copied into hl during parsing, so check
     for that max len as well. For now these are the same value.
     ================================================================*/
   if (strlen(BSAqryDescP->objName.pathName) >
       min(DSM_MAX_HL_LENGTH, BSA_MAX_PATHNAME))
   {
      sprintf(traceStr2, "BSAQueryObject: pathName too long (%d)",
              strlen(BSAqryDescP->objName.pathName));
      ourTrace(BSAHandle, TrFL, traceStr2);
      bsaRC = BSA_RC_OBJNAME_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSAQueryObject",
                  bsaRC,__FILE__,__LINE__);
   }
   if ((BSAqryDescP->objectType == BSAObjectType_FILE) ||
       (BSAqryDescP->objectType == BSAObjectType_DATABASE))
      dsmObjType = DSM_OBJ_FILE;
   else
      if (BSAqryDescP->objectType == BSAObjectType_DIRECTORY)
         dsmObjType = DSM_OBJ_DIRECTORY;
      else
         if (BSAqryDescP->objectType == BSAObjectType_ANY)
            dsmObjType = DSM_OBJ_ANY_TYPE;
         else
         {
             sprintf(traceStr2,
                     "BSAQueryObject: invalid objectType (%d)", BSAqryDescP->objectType);
             ourTrace(BSAHandle,TrFL, traceStr2);
             bsaRC = ADSM_RC_INVALID_OBJTYPE;
             strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
             ourLogEvent_Ex(BSAHandle, logLocal, ourMessage, errPrefix, logSevError);
             XOPENRETURN(BSAHandle, "BSAQueryObject",
                         bsaRC,__FILE__,__LINE__);
         }

   if (!(xopenGbl.sessFlags & FL_IN_BSA_TXN))
   {
      sprintf(traceStr2, "BSAQueryObject: issued without BSABeginTxn");
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = BSA_RC_BAD_CALL_SEQUENCE;
      XOPENRETURN(BSAHandle, "BSAQueryObject",
                  bsaRC,__FILE__,__LINE__);
   }

   /* store object name in ADSM structure */
   strcpy(objName.fs,BSAqryDescP->objName.objectSpaceName) ;

   if (!strcmp(BSAqryDescP->objName.pathName, "/*")) /* if they use a wildcard */
   {  /* we need to use a wild card on both hl and ll since the path gets split */
      strcpy(objName.hl, "*");  /* don't include delimiter in case there is
                             no hl */
      strcpy(objName.ll, "/*");
   }
   else
      xparsePath(BSAHandle, BSAqryDescP->objName.pathName, objName.hl, objName.ll);

      objName.objType = dsmObjType ;

      /*=== save copy type for GetNextQuery call later ===*/
      xopenGbl.copyType = BSAqryDescP->copyType;

      if (BSAqryDescP->copyType == BSACopyType_ARCHIVE)
      {
         if (strlen(BSAqryDescP->desc) > ADSM_MAX_DESC)
         {

            sprintf(traceStr2, "BSAQueryObject: description longer than ADSM max (%d). ", strlen(BSAqryDescP->desc));
            ourTrace(BSAHandle,TrFL, traceStr2);
            bsaRC = BSA_RC_DESC_TOO_LONG;
            XOPENRETURN(BSAHandle, "BSAQueryObject",
                        bsaRC,__FILE__,__LINE__);
         }

         archiveData.stVersion = qryArchiveDataVersion;
         archiveData.objName   = &objName;
         archiveData.owner     = (char *)&BSAqryDescP->owner.appObjectOwner;

         /*=================================================================
           in the tm structure the month count starts with 0 and year is
           the value since 1900.
           Adjust the year value only if it isn't one of the boundary values
           =================================================================*/

         archiveData.insDateLowerBound.year   = BSAqryDescP->createTimeLB.tm_year;
         if (archiveData.insDateLowerBound.year != ADSM_LOWEST_BOUND)
            archiveData.insDateLowerBound.year  += 1900;
         archiveData.insDateLowerBound.month  = BSAqryDescP->createTimeLB.tm_mon+1;
         archiveData.insDateLowerBound.day    = BSAqryDescP->createTimeLB.tm_mday;
         archiveData.insDateLowerBound.hour   = BSAqryDescP->createTimeLB.tm_hour;
         archiveData.insDateLowerBound.minute = BSAqryDescP->createTimeLB.tm_min;
         archiveData.insDateLowerBound.second = BSAqryDescP->createTimeLB.tm_sec;

         archiveData.insDateUpperBound.year   = BSAqryDescP->createTimeUB.tm_year;
         if (archiveData.insDateUpperBound.year != ADSM_HIGHEST_BOUND)
             archiveData.insDateUpperBound.year  += 1900;
         archiveData.insDateUpperBound.month  = BSAqryDescP->createTimeUB.tm_mon+1;
         archiveData.insDateUpperBound.day    = BSAqryDescP->createTimeUB.tm_mday;
         archiveData.insDateUpperBound.hour   = BSAqryDescP->createTimeUB.tm_hour;
         archiveData.insDateUpperBound.minute = BSAqryDescP->createTimeUB.tm_min;
         archiveData.insDateUpperBound.second = BSAqryDescP->createTimeUB.tm_sec;

         archiveData.expDateLowerBound.year   = BSAqryDescP->expireTimeLB.tm_year;
         if (archiveData.expDateLowerBound.year != ADSM_LOWEST_BOUND)
             archiveData.expDateLowerBound.year  += 1900;
         archiveData.expDateLowerBound.month  = BSAqryDescP->expireTimeLB.tm_mon+1;
         archiveData.expDateLowerBound.day    = BSAqryDescP->expireTimeLB.tm_mday;
         archiveData.expDateLowerBound.hour   = BSAqryDescP->expireTimeLB.tm_hour;
         archiveData.expDateLowerBound.minute = BSAqryDescP->expireTimeLB.tm_min;
         archiveData.expDateLowerBound.second = BSAqryDescP->expireTimeLB.tm_sec;

         archiveData.expDateUpperBound.year   = BSAqryDescP->expireTimeUB.tm_year;
         if (archiveData.expDateUpperBound.year != ADSM_HIGHEST_BOUND)
            archiveData.expDateUpperBound.year  += 1900;
         archiveData.expDateUpperBound.month  = BSAqryDescP->expireTimeUB.tm_mon+1;
         archiveData.expDateUpperBound.day    = BSAqryDescP->expireTimeUB.tm_mday;
         archiveData.expDateUpperBound.hour   = BSAqryDescP->expireTimeUB.tm_hour;
         archiveData.expDateUpperBound.minute = BSAqryDescP->expireTimeUB.tm_min;
         archiveData.expDateUpperBound.second = BSAqryDescP->expireTimeUB.tm_sec;

         archiveData.descr = (char *)&BSAqryDescP->desc;
         query_type = qtArchive;
         query_buff = (dsmQueryBuff *)&archiveData;

         qData.stVersion = DataBlkVersion;
         qData.bufferLen = sizeof(qryRespArchiveData);
         qData.bufferPtr = (char *)&respArchive;
         respArchive.stVersion = qryRespArchiveDataVersion;
   }
   else
      if (BSAqryDescP->copyType == BSACopyType_BACKUP)
      {
         if (BSAqryDescP->status == BSAObjectStatus_ACTIVE)
            backupData.objState  = DSM_ACTIVE;
         else
            if (BSAqryDescP->status == BSAObjectStatus_INACTIVE)
               backupData.objState  = DSM_INACTIVE;
            else
               if (BSAqryDescP->status == BSAObjectStatus_ANY)
                  backupData.objState  = DSM_ANY_MATCH;
               else
               {
                   sprintf(traceStr2,
                           "BSAQueryObject: invalid status (%d)",
                            BSAqryDescP->status);
                   ourTrace(BSAHandle,TrFL, traceStr2);
                   bsaRC = ADSM_RC_INVALID_STATUS;
                   strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
                   ourLogEvent_Ex(BSAHandle, logLocal, ourMessage, errPrefix, logSevError);
                   XOPENRETURN(BSAHandle, "BSAQueryObject",
                               bsaRC,__FILE__,__LINE__);
               }

            backupData.stVersion = qryBackupDataVersion;
            backupData.objName   = &objName;
            backupData.owner     = (char *)&BSAqryDescP->owner.appObjectOwner;

            query_type = qtBackup;
            query_buff = (dsmQueryBuff *)&backupData;

            qData.stVersion = DataBlkVersion;
            qData.bufferLen = sizeof(qryRespBackupData);
            qData.bufferPtr = (char *)&respBackup;
            respBackup.stVersion = qryRespBackupDataVersion;;
      }
      else
      {
         sprintf(traceStr2,
                           "BSAQueryObject: invalid copyType (%d)",
                            BSAqryDescP->copyType);
         ourTrace(BSAHandle,TrFL, traceStr2);
         bsaRC = ADSM_RC_INVALID_COPYTYPE;
         strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
         ourLogEvent_Ex(BSAHandle, logLocal, ourMessage, errPrefix, logSevError);
         XOPENRETURN(BSAHandle, "BSAQueryObject",
                     bsaRC,__FILE__,__LINE__);
      }

      /*=== now set flag for query ===*/
      xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_IN_BSA_QRY); /* set on */

      if ((rc = AFSdsmBeginQuery(dsmHandle,query_type,query_buff)))
      {

         sprintf(traceStr2, "BSAQueryObject(AFSdsmBeginQuery) error rc = %d", rc);
         ourTrace(BSAHandle,TrFL, traceStr2);
         xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_BSA_QRY); /* set off */
         xlateRC(BSAHandle, rc, &bsaRC);
         XOPENRETURN(BSAHandle, "BSAQueryObject(AFSdsmBeginQuery)",
                                 bsaRC,__FILE__,__LINE__);
      }

      rc = AFSdsmGetNextQObj(dsmHandle,&qData);
      if (((rc == DSM_RC_OK) || (rc == DSM_RC_MORE_DATA)) && qData.numBytes)
      {
         BSAobjDescP->version = ObjectDescriptorVersion;
         strcpy(BSAobjDescP->Owner.bsaObjectOwner, xopenGbl.dsmSessInfo.id);
         BSAobjDescP->copyType = BSAqryDescP->copyType;
         strcpy(BSAobjDescP->cGName, "\0");   /* no copy group name returned */

         if (BSAqryDescP->copyType == BSACopyType_ARCHIVE)
         {
            fillArchiveResp(dsmHandle, BSAobjDescP, &respArchive);
         }
         else /* backup */
         {
            fillBackupResp(dsmHandle, BSAobjDescP, &respBackup);
         }
         /*=== look ahead and see if there is more or if this is really done ===*/
         rc = AFSdsmGetNextQObj(dsmHandle,&qData);
         if (( (rc == DSM_RC_OK) || (rc == DSM_RC_MORE_DATA)) && qData.numBytes)
         { /*=== save the response we just got for later call ===*/
            if (BSAqryDescP->copyType == BSACopyType_ARCHIVE)
            {
               if (!(xopenGbl.nextQryP = (char *)dsMalloc(sizeof(qryRespArchiveData))))
               {

                  sprintf(traceStr2, "BSAQueryObject: no memory for look ahead buffer");
                  ourTrace(BSAHandle,TrFL, traceStr2);
                  xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_BSA_QRY); /* set off */
                  bsaRC = BSA_RC_NO_RESOURCES;
                  XOPENRETURN(BSAHandle, "BSAQueryObject(AFSdsmGetNextQObj)",
                              bsaRC,__FILE__,__LINE__);
               }
               memcpy(xopenGbl.nextQryP, &respArchive, sizeof(qryRespArchiveData));
            }
            else
            {
               if (!(xopenGbl.nextQryP = (char *)dsMalloc(sizeof(qryRespBackupData))))
               {

                  sprintf(traceStr2, "BSAQueryObject: no memory for look ahead buffer");
                  ourTrace(BSAHandle,TrFL, traceStr2);
                  xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_BSA_QRY); /* set off */
                  bsaRC = BSA_RC_NO_RESOURCES;
                  XOPENRETURN(BSAHandle, "BSAQueryObject(AFSdsmGetNextQObj)",
                              bsaRC,__FILE__,__LINE__);

               }
               memcpy(xopenGbl.nextQryP, &respBackup, sizeof(qryRespBackupData));
            }
         }
      else
      {

         sprintf(traceStr2, "BSAQueryObject(AFSdsmGetNextQObj) rc = %d", rc);
         ourTrace(BSAHandle,TrFL, traceStr2);
         AFSdsmEndQuery(dsmHandle);
         xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_BSA_QRY); /* set off */
      }

   }
   else /* could be FINISHED or an error */
   {

      sprintf(traceStr2, "BSAQueryObject(AFSdsmGetNextQObj) rc = %d", rc);
      ourTrace(BSAHandle,TrFL, traceStr2);
      AFSdsmEndQuery(dsmHandle);
      xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_BSA_QRY); /* set off */
   }

   if (rc == DSM_RC_OK)
   {
      sprintf(traceStr2, "BSAQueryObject(AFSdsmGetNextQObj) rc = %d, ObjOwner(objDesc):'%s' appOwner(objDesc):'%s' \n ObjName(objDesc):'%.*s%.*s' \n copyType:%d copyId:'%d %d' cGName:'%s'",
       rc,
       BSAobjDescP->Owner.bsaObjectOwner,
       BSAobjDescP->Owner.appObjectOwner,
       100,BSAobjDescP->objName.objectSpaceName,
       100,BSAobjDescP->objName.pathName,
       BSAobjDescP->copyType,
       BSAobjDescP->copyId.left,
       BSAobjDescP->copyId.right,
       BSAobjDescP->cGName == NULL ? "" : BSAobjDescP->cGName);
       ourTrace(BSAHandle,TrFL, traceStr2);
   }

   xlateRC(BSAHandle, rc, &bsaRC);
   XOPENRETURN(BSAHandle, "BSAQueryObject",
               bsaRC, __FILE__,__LINE__);
}

BSA_Int16 BSAGetObject(
    long              BSAHandle,
    ObjectDescriptor *BSAobjDescP,
    DataBlock        *BSAdataBlockP
)
{
   dsInt16_t      rc = 0;
   dsInt16_t      rc1 = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;
   DataBlk        getBlk;
   dsmGetType     getType;
   dsmGetList     dsmObjList ;
   char           rcMsg[DSM_MAX_RC_MSG_LENGTH + 1];
   char           errPrefix[DSM_MAX_RC_MSG_LENGTH + 1];
   char           ourMessage[DSM_MAX_RC_MSG_LENGTH + 1];

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   dsmHandle = BSAHandle;

   memset(rcMsg,         '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(errPrefix,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(ourMessage,    '\0', DSM_MAX_RC_MSG_LENGTH + 1);

   sprintf(traceStr2, "BSAGetObject ENTRY: BSAHandle:%d version:%d copyType:%d copyId:'%d %d' \n bufferLen:%d numBytes:%d ",
           BSAHandle,
           BSAobjDescP->version,
           BSAobjDescP->copyType,
           BSAobjDescP->copyId.left,
           BSAobjDescP->copyId.right,
           BSAdataBlockP->bufferLen,
           BSAdataBlockP->numBytes);
   ourTrace(BSAHandle, TrFL, traceStr2);

   xopenGbl.oper = OPER_RECV_START;    /* save state for EndData later */

   if (BSAobjDescP->version != ObjectDescriptorVersion)
   {
      sprintf(traceStr2,"Warning: BSAGetObject: objectDesc.version unexpected %d.", BSAobjDescP->version);
      ourTrace(BSAHandle,TrFL, traceStr2);
      /*==================================================================
       don't treat this as an error now since it isn't defined in the spec
       bsaRC = ADSM_RC_INVALID_ST_VER;
       return(bsaRC);
       =================================================================*/
   }

   if (!(xopenGbl.sessFlags & FL_IN_BSA_TXN))
   {
      sprintf(traceStr2, "BSAGetObject: issued without BSABeginTxn");
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = BSA_RC_BAD_CALL_SEQUENCE;
      XOPENRETURN(BSAHandle, "BSAGetObject",
                  bsaRC,__FILE__,__LINE__);
   }

   /*=== Get the data ===*/
   if (BSAobjDescP->copyType == BSACopyType_ARCHIVE)
      getType = gtArchive;
   else
      if (BSAobjDescP->copyType == BSACopyType_BACKUP)
         getType = gtBackup;
      else
      {
         sprintf(traceStr2,
                           "BSAGetObject: invalid copyType (%d)",
                            BSAobjDescP->copyType);
         ourTrace(BSAHandle,TrFL, traceStr2);
         bsaRC = ADSM_RC_INVALID_COPYTYPE;
         strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
         ourLogEvent_Ex(BSAHandle, logLocal, ourMessage, errPrefix, logSevError);
         XOPENRETURN(BSAHandle, "BSAGetObject",
                     bsaRC,__FILE__,__LINE__);
      }

   /*=== now update state since we'll issue a base API call ===*/
   xopenGbl.oper = OPER_RECV_ISSUED;    /* save state for EndData later */
   /*=== setup for a single object get ===*/

   dsmObjList.stVersion = dsmGetListVersion ;
   dsmObjList.numObjId = 1 ;
   dsmObjList.objId = (ObjID *)dsMalloc(sizeof(ObjID) * dsmObjList.numObjId) ;

   dsmObjList.objId[0].hi = BSAobjDescP->copyId.left ;
   dsmObjList.objId[0].lo = BSAobjDescP->copyId.right ;

   if ((rc = AFSdsmBeginGetData(dsmHandle,bTrue,   /* always say MountWait */
                             getType,
                             &dsmObjList)) != DSM_RC_OK)
   {
      sprintf(traceStr2, "BSAGetObject: Call to AFSdsmBeginGetData error rc = %d", rc);
      ourTrace(BSAHandle,TrFL, traceStr2);
      xlateRC(BSAHandle, rc, &bsaRC);
      XOPENRETURN(BSAHandle, "BSAGetObject(AFSdsmBeginGetData)",
                  bsaRC,__FILE__,__LINE__);
   }

   /*********************************************************************/
   getBlk.stVersion = DataBlkVersion ;
   getBlk.bufferPtr = BSAdataBlockP->bufferPtr ;
   getBlk.bufferLen = BSAdataBlockP->bufferLen;
   getBlk.numBytes  = 0;

   rc = AFSdsmGetObj(dsmHandle, &(dsmObjList.objId[0]), &getBlk) ;

   if ((rc == DSM_RC_FINISHED) &&
       (getBlk.numBytes != 0))    /* actually is data received */
      rc = DSM_RC_MORE_DATA;      /* use different rc for consistency */
                                  /* with Query. No data returned */
                                  /* if rc = FINISHED.     */
   if ((rc == DSM_RC_FINISHED) ||
       (rc == DSM_RC_MORE_DATA))
   {
      /*=== data is already in the buffer ===*/
      BSAdataBlockP->numBytes = (BSA_UInt16)getBlk.numBytes;
   }
   else
   {
      /*=== appl may call BSAEndData to clean up trxn but don't count on it... ===*/
      rc1 = AFSdsmEndGetObj(dsmHandle);
      rc1 = AFSdsmEndGetData(dsmHandle);
      xopenGbl.sessFlags =
               (xopenGbl.sessFlags | FL_END_DATA_DONE);  /* turn flag on */
    }

   xlateRC(BSAHandle, rc, &bsaRC);
   XOPENRETURN(BSAHandle, "BSAGetObject",
               bsaRC,__FILE__,__LINE__);
}

BSA_Int16 BSAGetData(
    long              BSAHandle,
    DataBlock        *BSAdataBlockP
)
{
   dsInt16_t      rc = 0;
   dsInt16_t      rc1 = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;
   DataBlk        getBlk;

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   dsmHandle = BSAHandle;


   sprintf(traceStr2, "BSAGetData ENTRY: BSAHandle:%d, bufferLen:%d, numBytes:%d",
           BSAHandle,
           BSAdataBlockP->bufferLen,
           BSAdataBlockP->numBytes);
   ourTrace(BSAHandle,TrFL, traceStr2);

   getBlk.stVersion = DataBlkVersion ;
   getBlk.bufferPtr = BSAdataBlockP->bufferPtr ;
   getBlk.bufferLen = BSAdataBlockP->bufferLen;
   getBlk.numBytes  = 0;

   rc = AFSdsmGetData(dsmHandle, &getBlk) ;

   if ((rc == DSM_RC_FINISHED) &&
       (getBlk.numBytes != 0))      /* actually is data received */
      rc = DSM_RC_MORE_DATA;        /* use different rc for consistency */
                                    /* with Query. No data returned */
                                    /* if rc = FINISHED.     */

   if ((rc == DSM_RC_FINISHED) ||
       (rc == DSM_RC_MORE_DATA))
   {
      /*=== data is already in the buffer ===*/
      BSAdataBlockP->numBytes = (BSA_UInt16)getBlk.numBytes;
   }
   else
   {
      sprintf(traceStr2, "BSAGetData: Call to AFSdsmGetData error rc = %d", rc);
      ourTrace(BSAHandle, TrFL,traceStr2);

      /*=== appl may call BSAEndData to clean up trxn but don't count on it... ===*/
      rc1 = AFSdsmEndGetObj(dsmHandle);
      rc1 = AFSdsmEndGetData(dsmHandle);
      xopenGbl.sessFlags =
               (xopenGbl.sessFlags | FL_END_DATA_DONE);  /* turn flag on */
   }
   xlateRC(BSAHandle, rc, &bsaRC);
   XOPENRETURN(BSAHandle, "BSAGetData(AFSdsmGetData)",
               bsaRC,__FILE__,__LINE__);
}

BSA_Int16 BSASendData(
    long           BSAHandle,
    DataBlock     *BSAdataBlockP
)
{
   dsInt16_t      rc = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;
   DataBlk        dataBlkArea;

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   dsmHandle = BSAHandle;


   sprintf(traceStr2, "BSASendData ENTRY: BSAHandle:%d bufferLen: %d numBytes: %d ",
           BSAHandle,
           BSAdataBlockP->bufferLen,
           BSAdataBlockP->numBytes);
   ourTrace(BSAHandle, TrFL, traceStr2);

   /*=== check for NULL dataBlock pointer ===*/
   if (BSAdataBlockP == NULL)
      XOPENRETURN(BSAHandle, "BSASendData",
                  BSA_RC_NULL_POINTER,__FILE__,__LINE__);
      dataBlkArea.stVersion = DataBlkVersion ;
      dataBlkArea.bufferPtr = BSAdataBlockP->bufferPtr ;
      dataBlkArea.bufferLen = BSAdataBlockP->bufferLen;

      rc = AFSdsmSendData(dsmHandle, &dataBlkArea);

      if (rc)
      {

         sprintf(traceStr2, "BSASendData(AFSdsmSendData) error rc = %d", rc);
         ourTrace(BSAHandle,TrFL, traceStr2);

         if (rc == DSM_RC_WILL_ABORT)  /* save flag */
            xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_RC_WILL_ABORT);
      }
      BSAdataBlockP->numBytes = (BSA_UInt16)dataBlkArea.numBytes;
      sprintf(traceStr2, "BSASendData(AFSdsmSendData): BSAHandle:%d bufferLen: %d numBytes sent: %d ",
           BSAHandle,
           BSAdataBlockP->bufferLen,
           BSAdataBlockP->numBytes);
      ourTrace(BSAHandle, TrFL, traceStr2);

      xlateRC(BSAHandle, rc, &bsaRC);
      XOPENRETURN(BSAHandle, "BSASendData",
                  BSA_RC_SUCCESS,__FILE__,__LINE__);
}

BSA_Int16 BSAEndData(
    long          BSAHandle
)
{
   dsInt16_t      rc = 0;
   dsInt16_t      rc1 = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   dsmHandle = BSAHandle;


   sprintf(traceStr2, "BSAEndData ENTRY: BSAHandle:%d", BSAHandle);
   ourTrace(BSAHandle,TrFL, traceStr2);

   /*=======================================================
     check the state - don't issue base API call unless one
     was called previously, or else we get a sequence error.
     ======================================================*/

   if (xopenGbl.oper == OPER_SEND_ISSUED)
   {
      rc = AFSdsmEndSendObj(dsmHandle);
      if (rc)
      {

         sprintf(traceStr2, "BSAEndData(AFSdsmEndSendObj) error rc = %d", rc);
         ourTrace(BSAHandle,TrFL, traceStr2);
         xlateRC(BSAHandle, rc, &bsaRC);
      }
   }
   else
      if (xopenGbl.oper == OPER_RECV_ISSUED)
      {
          if (xopenGbl.sessFlags & FL_END_DATA_DONE)   /* EndData processing */
             xopenGbl.sessFlags =                     /*  already done */
                   (xopenGbl.sessFlags ^ FL_END_DATA_DONE);  /* turn flag off */
         else
         {
            rc = AFSdsmEndGetObj(dsmHandle);
            if (rc)
            {
               sprintf(traceStr2, "BSAEndData(AFSdsmEndGetObj) error rc = %d", rc);
               ourTrace(BSAHandle, TrFL, traceStr2);

               /*==============================================================
                this may get 'Out of sequence error' if previous GetObject
                had rc=2 (no match). Don't return this to caller - too confusing
                ===============================================================*/
               if (rc != DSM_RC_BAD_CALL_SEQUENCE)
               xlateRC(BSAHandle, rc, &bsaRC);
            }

            rc = AFSdsmEndGetData(dsmHandle);
            if (rc)
            {
               sprintf(traceStr2, "BSAEndData(AFSdsmEndGetData) error rc = %d", rc);
               ourTrace(BSAHandle, TrFL, traceStr2);
               xlateRC(BSAHandle, rc, &bsaRC);
            }
         }
      }
   else  /* invalid state */
      if ((xopenGbl.oper != OPER_RECV_START) &&
          (xopenGbl.oper != OPER_SEND_START))
      {
         sprintf(traceStr2, "BSAEndData: BSAEndData but not Send or Recv");
         ourTrace(BSAHandle,TrFL, traceStr2);
         bsaRC = BSA_RC_BAD_CALL_SEQUENCE;
      }

   xopenGbl.oper = OPER_NONE;
   XOPENRETURN(BSAHandle, "BSAEndData",bsaRC,__FILE__,__LINE__);
}

BSA_Int16 BSACreateObject(
    long              BSAHandle,
    ObjectDescriptor *BSAobjDescP,
    DataBlock        *BSAdataBlockP
)
{
   dsInt16_t          rc = 0;
   BSA_Int16          bsaRC = 0;
   dsUint32_t         dsmHandle;
   regFSData          regFilespace ;
   dsmObjName         objName ;
   ObjAttr            objAttrArea;
   sndArchiveData     archData;
   DataBlk            dataBlkArea;
   dsmSendType        sendType;
   dsUint8_t          dsmObjType;
   XAPIObjInfo        xapiObjInfo;
   mcBindKey          mcBindKey;
   BSAObjectOwner     upperNode;  /* upper cased node name */
   char               errPrefix[DSM_MAX_RC_MSG_LENGTH + 1];
   char               ourMessage[DSM_MAX_RC_MSG_LENGTH + 1];

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   memset(errPrefix,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(ourMessage,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);

   dsmHandle = BSAHandle;

   if (BSAobjDescP != NULL && BSAdataBlockP != NULL)
   {
   sprintf(traceStr2, "BSACreateObject ENTRY: BSAHandle:%d ObjOwner:'%s' appOwner:'%s' \n ObjName:'%.*s%.*s' \n objType:%d  size:'%d %d' resourceType:'%s'  \n bufferLen:%d numBytes:%d ",

           BSAHandle,
           BSAobjDescP->Owner.bsaObjectOwner[0] != '\0' ? BSAobjDescP->Owner.bsaObjectOwner : "",
           BSAobjDescP->Owner.appObjectOwner[0] != '\0' ?  BSAobjDescP->Owner.appObjectOwner : "",
           100,BSAobjDescP->objName.objectSpaceName[0] != '\0' ? BSAobjDescP->objName.objectSpaceName
           : "",
           100,BSAobjDescP->objName.pathName[0] != '\0' ? BSAobjDescP->objName.pathName : "",
           BSAobjDescP->objectType,
           BSAobjDescP->size.left,
           BSAobjDescP->size.right,
           BSAobjDescP->resourceType[0] != '\0' ?  BSAobjDescP->resourceType : "" ,
           BSAdataBlockP->bufferLen,
           BSAdataBlockP->numBytes);
  }
  else
  {
    if (BSAobjDescP == NULL)
      {
         bsaRC = BSA_RC_NULL_POINTER;
         XOPENRETURN(BSAHandle, "BSACreateObject",
                  bsaRC,__FILE__,__LINE__);
      }
    sprintf(traceStr2, "BSACreateObject ENTRY: BSAHandle:%d", BSAHandle);
  }

  ourTrace(BSAHandle, TrFL, traceStr2);

   xopenGbl.oper = OPER_SEND_START;    /* save state for EndData later */
   /*=================================================================
   if (BSAobjDescP->version != ObjectDescriptorVersion)
       BSA spec doesn't require this currently..
   =================================================================*/

   /*===============================================================
    if the node name is different - we won't create it compare both
    the value passed on the BSAInit and the final value used for the
    session (which may be different for Generate)
    ===============================================================*/
   strcpy(upperNode, BSAobjDescP->Owner.bsaObjectOwner);
   StrUpper(upperNode);
   if ((strcmp(upperNode, xopenGbl.dsmSessInfo.id)) &&
      (strcmp(BSAobjDescP->Owner.bsaObjectOwner, xopenGbl.bsaObjectOwner)))
   {
      sprintf(traceStr2,
             "BSACreateObject: BSAObjectOwner(%s) doesn't match value for session(%s).",
              upperNode, xopenGbl.dsmSessInfo.id);
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = ADSM_RC_INVALID_NODE;
      strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
      ourLogEvent_Ex(BSAHandle,logLocal, ourMessage, errPrefix, logSevError);
      XOPENRETURN(BSAHandle, "BSACreateObject",
                  bsaRC,__FILE__,__LINE__);
   }

   /*=== check string lengths - if this too long, it won't fit in our objInfo ===*/
   if (strlen(BSAobjDescP->desc) > ADSM_MAX_DESC)
   {
      sprintf(traceStr2,"BSACreateObject: description longer than TSM max (%d). ",
              strlen(BSAobjDescP->desc));
      ourTrace(BSAHandle, TrFL, traceStr2);
      bsaRC = BSA_RC_DESC_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSACreateObject",
                  bsaRC,__FILE__,__LINE__);
   }
   if (strlen(BSAobjDescP->objectInfo) > ADSM_MAX_OBJINFO)
   {
      sprintf(traceStr2,"BSACreateObject: objInfo longer than TSM max (%d).",
      strlen(BSAobjDescP->objectInfo));
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = BSA_RC_OBJINFO_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSACreateObject",
                  bsaRC,__FILE__,__LINE__);
   }

   if (!(xopenGbl.sessFlags & FL_IN_BSA_TXN))
   {
      sprintf(traceStr2, "BSACreateObject issued without BSABeginTxn");
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = BSA_RC_BAD_CALL_SEQUENCE;
      XOPENRETURN(BSAHandle, "BSACreateObject",
                  bsaRC,__FILE__,__LINE__);
   }

   if (strlen(BSAobjDescP->objName.objectSpaceName) > BSA_MAX_OSNAME)
   {
      sprintf(traceStr2, "BSACreateObject: objectSpaceName too long (%d)",
                       strlen(BSAobjDescP->objName.objectSpaceName));
      ourTrace(BSAHandle, TrFL, traceStr2);
      bsaRC = BSA_RC_OBJNAME_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSACreateObject:",
                  bsaRC,__FILE__,__LINE__);
   }

   if (!(xopenGbl.sessFlags & FL_IN_DSM_TXN))        /* first CreateObj */
   { /* can't issue RegisterFS if already in Txn */
      /*=== Register the file space ===*/
      regFilespace.stVersion = regFSDataVersion ;
      regFilespace.fsName = BSAobjDescP->objName.objectSpaceName ;

      /*===  use resource type for fsType (as it was intended)  ===*/

      regFilespace.fsType = BSAobjDescP->resourceType ;
      regFilespace.capacity.lo = 0;
      regFilespace.capacity.hi = 0;
      regFilespace.occupancy.lo = 0;
      regFilespace.occupancy.hi = 0;
      #if _OPSYS_TYPE == DS_AIX
         regFilespace.fsAttr.unixFSAttr.fsInfoLength = strlen(XAPI_FSINFO) ;
         strcpy(regFilespace.fsAttr.unixFSAttr.fsInfo, XAPI_FSINFO);
      #else
         regFilespace.fsAttr.dosFSAttr.fsInfoLength = strlen(XAPI_FSINFO) ;
         strcpy(regFilespace.fsAttr.dosFSAttr.fsInfo, XAPI_FSINFO);
         regFilespace.fsAttr.dosFSAttr.driveLetter = 'X';
      #endif
      rc = AFSdsmRegisterFS(dsmHandle, &regFilespace) ;
      if ((rc != 0) && (rc != DSM_RC_FS_ALREADY_REGED))
      {
         sprintf(traceStr2, "BSACreateObject(AFSdsmRegisterFS) error rc = %d",rc);
         ourTrace(BSAHandle,TrFL, traceStr2);

         xlateRC(BSAHandle, rc, &bsaRC);
         XOPENRETURN(BSAHandle, "BSACreateObject(AFSdsmRegisterFS)",
                     bsaRC,__FILE__,__LINE__);
      }
   }

   /*========================================================
    Check for invalid copyType before sending data. Log error
    to dsirror.log file.
    ========================================================*/
    if (BSAobjDescP->copyType == BSACopyType_ARCHIVE)
       sendType = stArchiveMountWait;
    else
       if (BSAobjDescP->copyType == BSACopyType_BACKUP)
          sendType = stBackupMountWait;
       else
       {
           sprintf(traceStr2,
                  "BSACreateObject: invalid copyType (%d)",
                   BSAobjDescP->copyType);
           ourTrace(BSAHandle,TrFL, traceStr2);
           bsaRC = ADSM_RC_INVALID_COPYTYPE;
           strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
           ourLogEvent_Ex(BSAHandle,logLocal, ourMessage, errPrefix, logSevError);
           XOPENRETURN(BSAHandle, "BSACreateObject",
                       bsaRC,__FILE__,__LINE__);
       }

   if ((BSAobjDescP->objectType == BSAObjectType_FILE) ||
      (BSAobjDescP->objectType == BSAObjectType_DATABASE) ||
      (BSAobjDescP->objectType == BSAObjectType_ANY))

         dsmObjType = DSM_OBJ_FILE;
   else
      if (BSAobjDescP->objectType == BSAObjectType_DIRECTORY)
         dsmObjType = DSM_OBJ_DIRECTORY;
      else
      {
         sprintf(traceStr2,
               "BSACreateObject: invalid objectType (%d)",
                BSAobjDescP->objectType);
         ourTrace(BSAHandle,TrFL, traceStr2);
         bsaRC = ADSM_RC_INVALID_OBJTYPE;
         strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
         ourLogEvent_Ex(BSAHandle,logLocal, ourMessage, errPrefix, logSevError);
         XOPENRETURN(BSAHandle, "BSACreateObject",
                    bsaRC,__FILE__,__LINE__);
      }

   /*==================================================================
    put in a check here - count the number of objects per txn
    and compare with xopenGbl.sessInfo.maxObjPerTxn
    If reach the limit, EndTxn and start a new one
    OK to do this without telling the BSA caller?
    Or should we exit with an error to tell them the limit is reached ?
    ==================================================================*/

    if (!(xopenGbl.sessFlags & FL_IN_DSM_TXN))
    {
      rc = AFSdsmBeginTxn(dsmHandle);

      if (rc)
      {
         sprintf(traceStr2, "BSACreateObject(AFSdsmBeginTxn) error rc = %d", rc);
         ourTrace(BSAHandle,TrFL, traceStr2);
         xlateRC(BSAHandle, rc, &bsaRC);
         XOPENRETURN(BSAHandle, "BSACreateObject(AFSdsmBeginTxn)",
                     bsaRC,__FILE__,__LINE__);
      }
      xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_IN_DSM_TXN); /* set on */
    }

    /*=== Backup the data  ===*/

   /*================================================================
     the entire pathname gets copied into hl during parsing, so
     check for that max len as well. For now these are the same value.
     =================================================================*/
   if (strlen(BSAobjDescP->objName.pathName) >
            min(DSM_MAX_HL_LENGTH, BSA_MAX_PATHNAME))
   {
      sprintf(traceStr2, "BSACreateObject: pathName too long (%d)",
              strlen(BSAobjDescP->objName.pathName));
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = BSA_RC_OBJNAME_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSACreateObject",
                  bsaRC,__FILE__,__LINE__);
   }

   strcpy(objName.fs,BSAobjDescP->objName.objectSpaceName) ;
   /*=== previous code to use only ll field ===*/
   /*objName.hl[0] = '\0';
     strcpy(objName.ll,BSAobjDescP->objName.pathName) ;
   */
   xparsePath(BSAHandle, BSAobjDescP->objName.pathName, objName.hl, objName.ll);
   objName.objType = dsmObjType ;

   objAttrArea.stVersion = ObjAttrVersion ;
   strcpy(objAttrArea.owner,BSAobjDescP->Owner.appObjectOwner);
   objAttrArea.sizeEstimate.hi = BSAobjDescP->size.left;
   objAttrArea.sizeEstimate.lo = BSAobjDescP->size.right;
   objAttrArea.objCompressed = bFalse ;  /* let COMPRESSION option decide */
   /*=== whether there's actually compression ===*/
   objAttrArea.objInfoLength = sizeof(XAPIObjInfo);
   objAttrArea.objInfo = (char *)&xapiObjInfo ;

   memset(&xapiObjInfo,0x00,sizeof(XAPIObjInfo));
   strcpy(xapiObjInfo.resourceType, BSAobjDescP->resourceType);
   xapiObjInfo.size.left = BSAobjDescP->size.left;
   xapiObjInfo.size.right = BSAobjDescP->size.right;
   strcpy(xapiObjInfo.partDesc, BSAobjDescP->desc);
   strcpy(xapiObjInfo.partObjInfo, BSAobjDescP->objectInfo);

   /*=== check if a lifecycle group name was passed to us ===*/
   if (strlen(BSAobjDescP->lGName))
      objAttrArea.mcNameP = (char *)BSAobjDescP->lGName ;
   else
      objAttrArea.mcNameP = NULL;

      dataBlkArea.stVersion = DataBlkVersion ;
      if (BSAdataBlockP == NULL)
      {
         dataBlkArea.bufferPtr = NULL;
         dataBlkArea.bufferLen = 0;
      }
      else
      {
         dataBlkArea.bufferPtr = BSAdataBlockP->bufferPtr ;
         dataBlkArea.bufferLen = BSAdataBlockP->bufferLen;
      }

      /*=======================================================
       always issue BindMC because we don't expect applications
       to call ResolveLifecycleGroup since it isn't in the
       Data Movement subset
       =======================================================*/
       mcBindKey.stVersion = mcBindKeyVersion ;
       rc = AFSdsmBindMC(dsmHandle, &objName, sendType, &mcBindKey);
       if (rc)
       {
          sprintf(traceStr2, "BSACreateObject(AFSdsmBindMC): error rc = %d", rc);
          ourTrace(BSAHandle, TrFL, traceStr2);
          xlateRC(BSAHandle, rc, &bsaRC);
          XOPENRETURN(BSAHandle, "BSACreateObject(dsnBindMC)",
                      bsaRC,__FILE__,__LINE__);
       }

       /*=== now update state since we'll issue the base Send call ===*/

       xopenGbl.oper = OPER_SEND_ISSUED;    /* save state for EndData later */

       switch (sendType)
       {
          case (stBackupMountWait) :
          rc = AFSdsmSendObj(dsmHandle,
                          sendType,
                          NULL,
                          &objName,
                          &objAttrArea,
                          &dataBlkArea);
          break;

          case (stArchiveMountWait) :
          archData.stVersion = sndArchiveDataVersion;
          archData.descr = (char *)(BSAobjDescP->desc);
          rc = AFSdsmSendObj(dsmHandle,
                          sendType,
                          &archData,
                          &objName,
                          &objAttrArea,
                          &dataBlkArea);
          break;
          default : ;
       }

       if (rc != DSM_RC_OK)
       {
          sprintf(traceStr2, "BSACreateObject(AFSdsmSendObj) error rc = %d", rc);
          ourTrace(BSAHandle,TrFL, traceStr2);

          if (rc == DSM_RC_WILL_ABORT)  /* save flag */
             xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_RC_WILL_ABORT);

          xlateRC(BSAHandle, rc, &bsaRC);
          XOPENRETURN(BSAHandle, "BSACreateObject(AFSdsmSendObj)",
                      bsaRC,__FILE__,__LINE__);
       }
   XOPENRETURN(BSAHandle, "BSACreateObject",
               BSA_RC_SUCCESS, __FILE__,__LINE__);
}


BSA_Int16 BSADeleteObject(
    long           BSAHandle,
    CopyType       copyType,
    ObjectName     *BSAobjNameP,
    CopyId         *copyidP
)
{
   dsInt16_t      rc = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;
   dsUint16_t     reason ;                   /* for AFSdsmEndTxn */
   dsmObjName     dsmobjName;

   dsmDelType delType;
   dsmDelInfo delInfo;

   delList *llHeadP = NULL;
   delList *llTailP = NULL;
   delList *ll = NULL;

   char errPrefix[DSM_MAX_RC_MSG_LENGTH + 1];
   char ourMessage[DSM_MAX_RC_MSG_LENGTH + 1];

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   memset(errPrefix,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);
   memset(ourMessage,     '\0', DSM_MAX_RC_MSG_LENGTH + 1);

   dsmHandle = BSAHandle;

   sprintf(traceStr2, "BSADeleteObject ENTRY: BSAHandle:%d CopyType:%d \n ObjName:'%.*s%.*s' copyidP:'%d %d'.",
           BSAHandle,
           copyType,
           100,BSAobjNameP->objectSpaceName,
           100,BSAobjNameP->pathName,
           copyidP->left,
           copyidP->right);
   ourTrace(BSAHandle, TrFL, traceStr2);
   if (copyType != BSACopyType_ARCHIVE)
   {
      sprintf(traceStr2,
              "BSADeleteObject: invalid copyType %d",
               copyType);
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = ADSM_RC_INVALID_COPYTYPE;
      strcpy(ourMessage, ourRCMsg(bsaRC, errPrefix));
      ourLogEvent_Ex(BSAHandle, logLocal, ourMessage, errPrefix, logSevError);
      XOPENRETURN(BSAHandle, "BSADeleteObject",
                  bsaRC,__FILE__,__LINE__);
   }

   if (!(xopenGbl.sessFlags & FL_IN_BSA_TXN))
   {
      sprintf(traceStr2, "BSADeleteObject issued without BSABeginTxn");
      ourTrace(BSAHandle, TrFL, traceStr2);

      bsaRC = BSA_RC_BAD_CALL_SEQUENCE;
      XOPENRETURN(BSAHandle, "BSADeleteObject:",
                  bsaRC,__FILE__,__LINE__);
   }

   strcpy(dsmobjName.fs, BSAobjNameP->objectSpaceName);
   xparsePath(BSAHandle, BSAobjNameP->pathName, dsmobjName.hl, dsmobjName.ll);
   dsmobjName.objType = DSM_OBJ_FILE;

   if (!copyidP)    /* NULL, so query and delete all with same name */
   {
      if (xopenGbl.sessFlags & FL_IN_DSM_TXN)
      /*=== if a trxn had been started, end it before doing Query ===*/
      {
         rc = AFSdsmEndTxn(dsmHandle, DSM_VOTE_COMMIT, &reason);
         xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_DSM_TXN); /* set off */
      }
      rc = buildList(dsmHandle, &dsmobjName, &llHeadP, &llTailP);
      if (rc)
      {
         bsaRC = BSA_RC_OBJECT_NOT_FOUND;
         XOPENRETURN(BSAHandle, "BSADeleteObject(buildList)",
                     bsaRC,__FILE__,__LINE__);
      }
   }

   if (!(xopenGbl.sessFlags & FL_IN_DSM_TXN))        /* first call */
   {
      rc = AFSdsmBeginTxn(dsmHandle);
      if (rc)
      {
         xlateRC(BSAHandle, rc, &bsaRC);
         XOPENRETURN(dsmHandle,"BSADeleteObject(AFSdsmBeginTxn)",
                     bsaRC,__FILE__,__LINE__);
      }
      xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_IN_DSM_TXN); /* set on */
   }

   delType = dtArchive;
   delInfo.archInfo.stVersion = delArchVersion;

   if (copyidP)   /* single ID to delete */
   {
      delInfo.archInfo.objId.hi  = copyidP->left;
      delInfo.archInfo.objId.lo  = copyidP->right;

      if ((rc = AFSdsmDeleteObj(dsmHandle,delType,delInfo)) != DSM_RC_OK)
      {
         sprintf(traceStr2, "BSADeleteObject: AFSdsmDeleteObj error rc = %d", rc);
         ourTrace(dsmHandle,TrFL, traceStr2);
      }
   }
   else   /* multiple IDs to delete */
   {
      ll = llHeadP;
      while (ll)
      {
         delInfo.archInfo.objId.hi  = ll->objId.hi;
         delInfo.archInfo.objId.lo  = ll->objId.lo;
         if ((rc = AFSdsmDeleteObj(dsmHandle, delType, delInfo)) != DSM_RC_OK)
         {
            sprintf(traceStr2, "BSADeleteObject: AFSdsmDeleteObj error rc = %d", rc);
            ourTrace(dsmHandle, TrFL, traceStr2);
            /*=== break and get out of loop, or keep going? ===*/
         }
         /*=== incr to next list entry ===*/
         ll = ll->next;
      }
      /*=== free list now that done ===*/
      rc =  freeList(&llHeadP, &llTailP);
   }

   xlateRC(BSAHandle, rc, &bsaRC);
   XOPENRETURN(BSAHandle,"BSADeleteObject",
               bsaRC,__FILE__,__LINE__)
}

BSA_Int16 BSAMarkObjectInactive(
    long            BSAHandle,
    ObjectName     *BSAobjNameP
)
{
   dsInt16_t      rc = 0;
   BSA_Int16      bsaRC = 0;
   dsUint32_t     dsmHandle;

   dsmObjName           dsmobjName;

   qryBackupData        queryBuffer;       /* for query Backup */
   qryRespBackupData    qbDataArea;
   DataBlk              qDataBlkArea;

   dsmDelType           delType;
   dsmDelInfo           delInfo;

   dsUint16_t           reason ;                   /* for AFSdsmEndTxn */
   /*=== build list of all objTypes we find for this name ===*/
   dsInt16_t            i;
   dsInt16_t            numTypes;
   dsUint8_t            listObjType[5];    /* only 2 objTypes defined today */
   dsUint32_t           listCopyGroup[5];

   if(!dsm_init)
   {
#ifdef DEBUG_BUTC
        printf("TSM library not mounted. \n");
#endif
        if (dsm_MountLibrary())
        {
                printf("TSM Library initialisation failed. \n");
                return 1 ;
        }
#ifdef DEBUG_BUTC
        printf("TSM Library initialisation SUCCESS. \n");
#endif
   }

   dsmHandle = BSAHandle;
   memset(&delInfo, 0x00, sizeof(dsmDelInfo));
   memset(&queryBuffer, 0x00, sizeof(qryBackupData));

   sprintf(traceStr2, "BSAMarkObjectInactive ENTRY: BSAHandle:%d \n ObjName:'%.*s%.*s'.",
           BSAHandle,
           100, BSAobjNameP->objectSpaceName,
           100, BSAobjNameP->pathName);
   ourTrace(dsmHandle, TrFL, traceStr2);

   if (!(xopenGbl.sessFlags & FL_IN_BSA_TXN))
   {
      sprintf(traceStr2, "BSAMarkObjectInactive: issued without BSABeginTxn.");
      ourTrace(BSAHandle, TrFL, traceStr2);
      bsaRC = BSA_RC_BAD_CALL_SEQUENCE;
      XOPENRETURN(BSAHandle, "BSAMarkObjectInactive",
                  bsaRC,__FILE__,__LINE__);
   }

   if (strlen(BSAobjNameP->objectSpaceName) > DSM_MAX_FSNAME_LENGTH)
   {
      sprintf(traceStr2, "BSAMarkObjectInactive: objectSpaceName too long (%d)", strlen(BSAobjNameP->objectSpaceName));
      ourTrace(BSAHandle,TrFL, traceStr2);

      bsaRC = BSA_RC_OBJNAME_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSAMarkObjectInactive",
                  bsaRC,__FILE__,__LINE__);
   }
   /*===============================================================
    The entire pathname gets copied into hl during parsing, so
    check for that max len as well. For now these are the same value.
    =============================================================== */
   if (strlen(BSAobjNameP->pathName) >
       min(DSM_MAX_HL_LENGTH, BSA_MAX_PATHNAME))
   {
      sprintf(traceStr2, "BSAMarkObjectInactive: pathName too long (%d)",
                         strlen(BSAobjNameP->pathName));
      ourTrace(BSAHandle,TrFL, traceStr2);
      bsaRC = BSA_RC_OBJNAME_TOO_LONG;
      XOPENRETURN(BSAHandle, "BSAMarkObjectInactive",
                  bsaRC,__FILE__,__LINE__);
   }

   /*==============================================================
    we don't allow any wildcard in name because that could retrieve
    LOTS of objects and make list processing more complicated.
    XBSA spec implies a single object.
    ==============================================================*/
   if ( strchr(BSAobjNameP->objectSpaceName, '*') ||
        strchr(BSAobjNameP->objectSpaceName, '?') ||
        strchr(BSAobjNameP->pathName,        '*') ||
        strchr(BSAobjNameP->pathName,        '?'))
   {

      sprintf(traceStr2, "BSAMarkObjectInactive: objName contains a wildcard.")
;
      ourTrace(BSAHandle, TrFL, traceStr2);
      /*=== could have a more specific rc, use this for now ===*/
      bsaRC = BSA_RC_ABORT_ACTIVE_NOT_FOUND;
      XOPENRETURN(BSAHandle, "BSAMarkObjectInactive",
                  bsaRC,__FILE__,__LINE__);
   }

   strcpy(dsmobjName.fs, BSAobjNameP->objectSpaceName);
   xparsePath(BSAHandle, BSAobjNameP->pathName, dsmobjName.hl, dsmobjName.ll);
   dsmobjName.objType = DSM_OBJ_ANY_TYPE;

   /*============================================================
    A Backup Delete must include the copy Group and objType this
    wasn't passed in, so we have to do a query.
    ============================================================*/

   queryBuffer.stVersion = qryBackupDataVersion ;
   queryBuffer.objName = &dsmobjName;
   queryBuffer.owner   = xopenGbl.dsmSessInfo.owner;
   queryBuffer.objState = DSM_ACTIVE;              /* only get active one */

   if ((rc=AFSdsmBeginQuery(dsmHandle, qtBackup,
                        (void *)&queryBuffer )) != DSM_RC_OK)
   {
      sprintf(traceStr2, "BSAMarkObjectInactive: Call to AFSdsmBeginQuery for Backup error rc = %d", rc);
      ourTrace(BSAHandle,TrFL, traceStr2);
      xlateRC(BSAHandle, rc, &bsaRC);
      if ((rc == DSM_RC_ABORT_NO_MATCH) ||    /* special rc for MarkInact */
          (rc == DSM_RC_FILE_SPACE_NOT_FOUND))
         bsaRC = BSA_RC_ABORT_ACTIVE_NOT_FOUND;

      rc = AFSdsmEndQuery(dsmHandle);
      XOPENRETURN(BSAHandle, "BSAMarkObjectInactive: AFSdsmBeginQuery",
                  bsaRC,__FILE__,__LINE__);
   }

   qbDataArea.stVersion   = qryRespBackupDataVersion;
   qDataBlkArea.stVersion = DataBlkVersion ;
   qDataBlkArea.bufferPtr = (char *)&qbDataArea;
   qDataBlkArea.bufferLen = sizeof(qryRespBackupData);

   numTypes = 0;
   rc = AFSdsmGetNextQObj(dsmHandle, &qDataBlkArea);
   while (((rc == DSM_RC_MORE_DATA) ||
         (rc == DSM_RC_FINISHED)) &&
          qDataBlkArea.numBytes)
   {  /* save copy Group we got */
      listCopyGroup[numTypes] = qbDataArea.copyGroup;
      listObjType[numTypes]   = qbDataArea.objName.objType;
      numTypes++;
      rc = AFSdsmGetNextQObj(dsmHandle, &qDataBlkArea);
   }

   if (rc != DSM_RC_FINISHED)
   {
      xlateRC(BSAHandle, rc, &bsaRC);
      if ((rc == DSM_RC_ABORT_NO_MATCH) || /* special rc for MarkInact */
          (rc == DSM_RC_FILE_SPACE_NOT_FOUND))
         bsaRC = BSA_RC_ABORT_ACTIVE_NOT_FOUND;
      rc = AFSdsmEndQuery(dsmHandle);
      XOPENRETURN(BSAHandle,"BSAMarkObjectInactive: AFSdsmGetNextQObj",
                  bsaRC,__FILE__,__LINE__);
   }
   rc = AFSdsmEndQuery(dsmHandle);

   /*=== now we can do the delete ===*/
   rc = AFSdsmBeginTxn(dsmHandle);

   if (rc)
   {
     xlateRC(BSAHandle, rc, &bsaRC);
     XOPENRETURN(BSAHandle,"BSAMarkObjectInactive: AFSdsmBeginTxn",
                 bsaRC,__FILE__,__LINE__);
   }
   xopenGbl.sessFlags = (xopenGbl.sessFlags | FL_IN_DSM_TXN); /* set on */

   delType = dtBackup;     /* this only applies to Backup */

   delInfo.backInfo.stVersion = delBackVersion;
   delInfo.backInfo.objNameP = &dsmobjName;

   for (i=0; i<numTypes; i++)
   {
      delInfo.backInfo.copyGroup = listCopyGroup[i];
      dsmobjName.objType         = listObjType[i];

      if ((rc = AFSdsmDeleteObj(dsmHandle, delType, delInfo)) != DSM_RC_OK)
      {

         sprintf(traceStr2, "BSAMarkObjectInactive: call to AFSdsmDeleteObj error rc = %d", rc);
         ourTrace(BSAHandle, TrFL, traceStr2);
      }
   }

   /*=== issue EndTxn since these can't be nested because of Query sequence ===*/
   AFSdsmEndTxn(dsmHandle, DSM_VOTE_COMMIT, &reason);
   xopenGbl.sessFlags = (xopenGbl.sessFlags ^ FL_IN_DSM_TXN); /* set off */

   xlateRC(BSAHandle, rc, &bsaRC);
   XOPENRETURN(BSAHandle,"BSAMarkObjectInactive",
               bsaRC,__FILE__,__LINE__);
}

#endif /*xbsa*/
