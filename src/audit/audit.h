/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define AUD_END  0		/* End           of veriable list */
#define AUD_STR  1		/* String        in variable list */
#define AUD_INT  2		/* int           in variable list */
#define AUD_LST  3		/* Variable list in a variable list */
#define AUD_HOST 4		/* A host # to be changed to string */
#define AUD_LONG 5		/* long          in variable list */
#define AUD_DATE 6		/* date (unsigned long)           */
#define AUD_FID  7		/* File ID                        */
#define AUD_FIDS 8		/* array of Fids                  */
/* next 3 lines on behalf of MR-AFS */
#define AUD_RESID 20		/* resid         in variable list */
#define AUD_RSSIZERANGE 21	/* rssizerange   in variable list */
#define AUD_LOOKUPINFO 22	/* LookupInfo    in variable list */

/*
 * Note: the master definitions of these error codes come from *.et
 * files in other parts of the tree.  They are hardcoded here as a
 * simple way to avoid circular dependence problems in the build.
 */

#define KANOAUTH                                 (180488L)	/* kauth/kaerrors.et */
#define RXKADNOAUTH                              (19270405L)	/* rxkad/rxkad_errs.et */
#define PRPERM                                   (267269L)	/* ptserver/pterror.et */
#define VL_PERM                                  (363546L)	/* vlserver/vl_errors.et */
#define BUDB_NOTPERMITTED                        (156303880L)	/* budb/budb_errs.et */
#define BZACCESS                                 (39430L)	/* bozo/boserr.et */
#define VOLSERBAD_ACCESS                         (1492325127L)	/* volser/volerr.et */


#define VS_StartEvent	   "AFS_VS_Start"
#define VS_FinishEvent	   "AFS_VS_Finish"
#define VS_ExitEvent	   "AFS_VS_Exit"
#define VS_ParInfEvent     "AFS_VS_ParInf"
#define VS_NukVolEvent     "AFS_VS_NukVol"
#define VS_CrVolEvent      "AFS_VS_CrVol"
#define VS_DelVolEvent     "AFS_VS_DelVol"
#define VS_CloneEvent      "AFS_VS_Clone"
#define VS_ReCloneEvent    "AFS_VS_ReClone"
#define VS_TransCrEvent    "AFS_VS_TransCr"
#define VS_GetNVolEvent    "AFS_VS_GetNVol"
#define VS_GetFlgsEvent    "AFS_VS_GetFlgs"
#define VS_SetFlgsEvent    "AFS_VS_SetFlgs"
#define VS_ForwardEvent    "AFS_VS_Forward"
#define VS_DumpEvent       "AFS_VS_Dump"
#define VS_RestoreEvent    "AFS_VS_Restore"
#define VS_EndTrnEvent     "AFS_VS_EndTrn"
#define VS_SetForwEvent    "AFS_VS_SetForw"
#define VS_GetStatEvent    "AFS_VS_GetStat"
#define VS_SetInfoEvent    "AFS_VS_SetInfo"
#define VS_GetNameEvent    "AFS_VS_GetName"
#define VS_SigRstEvent     "AFS_VS_SigRst"
#define VS_ListParEvent    "AFS_VS_ListPar"
#define VS_Lst1VolEvent    "AFS_VS_Lst1Vol"
#define VS_XLst1VlEvent    "AFS_VS_XLst1Vl"
#define VS_ListVolEvent    "AFS_VS_ListVol"
#define VS_XLstVolEvent    "AFS_VS_XLstVol"
#define VS_MonitorEvent    "AFS_VS_Monitor"
#define VS_SetIdTyEvent    "AFS_VS_SetIdTy"
#define VS_SetDateEvent    "AFS_VS_SetDate"
/* Next 2 lines on behalf of MR-AFS */
#define VS_SelectiveDumpEvent "AFS_VS_DmpSelct"
#define VS_ListVolumeResidencyInfoEvent "AFS_VS_LstVInfo"

#define PTS_StartEvent     "AFS_PTS_Start"
#define PTS_FinishEvent    "AFS_PTS_Finish"
#define PTS_ExitEvent      "AFS_PTS_Exit"
#define PTS_INewEntEvent   "AFS_PTS_INewEnt"
#define PTS_NewEntEvent    "AFS_PTS_NewEnt"
#define PTS_WheIsItEvent   "AFS_PTS_WheIsIt"
#define PTS_DmpEntEvent    "AFS_PTS_DmpEnt"
#define PTS_AdToGrpEvent   "AFS_PTS_AdToGrp"
#define PTS_NmToIdEvent    "AFS_PTS_NmToId"
#define PTS_IdToNmEvent    "AFS_PTS_IdToNm"
#define PTS_DelEvent       "AFS_PTS_Del"
#define PTS_RmFmGrpEvent   "AFS_PTS_RmFmGrp"
#define PTS_GetCPSEvent    "AFS_PTS_GetCPS"
#define PTS_GetCPS2Event   "AFS_PTS_GetCPS2"
#define PTS_GetHCPSEvent   "AFS_PTS_GetHCPS"
#define PTS_LstMaxEvent    "AFS_PTS_LstMax"
#define PTS_SetMaxEvent    "AFS_PTS_SetMax"
#define PTS_LstEntEvent    "AFS_PTS_LstEnt"
#define PTS_LstEntsEvent   "AFS_PTS_LstEnts"
#define PTS_ChgEntEvent    "AFS_PTS_ChgEnt"
#define PTS_SetFldEntEvent "AFS_PTS_SetFEnt"
#define PTS_LstEleEvent    "AFS_PTS_LstEle"
#define PTS_LstOwnEvent    "AFS_PTS_LstOwn"
#define PTS_IsMemOfEvent   "AFS_PTS_IsMemOf"
#define PTS_UpdEntEvent    "AFS_PTS_UpdEnt"

#define BUDB_StartEvent    "AFS_BUDB_Start"
#define BUDB_FinishEvent   "AFS_BUDB_Finish"
#define BUDB_ExitEvent     "AFS_BUDB_Exit"
#define BUDB_AddVolEvent   "AFS_BUDB_AddVol"
#define BUDB_CrDmpEvent    "AFS_BUDB_CrDmp"
#define BUDB_DelDmpEvent   "AFS_BUDB_DelDmp"
#define BUDB_LstDmpEvent   "AFS_BUDB_LstDmp"
#define BUDB_DelTpeEvent   "AFS_BUDB_DelTpe"
#define BUDB_DelVDPEvent   "AFS_BUDB_DelVDP"
#define BUDB_FndClnEvent   "AFS_BUDB_FndCln"
#define BUDB_FndDmpEvent   "AFS_BUDB_FndDmp"
#define BUDB_FndLaDEvent   "AFS_BUDB_FndLaD"
#define BUDB_FinDmpEvent   "AFS_BUDB_FinDmp"
#define BUDB_FinTpeEvent   "AFS_BUDB_FinTpe"
#define BUDB_GetDmpEvent   "AFS_BUDB_GetDmp"
#define BUDB_GetTpeEvent   "AFS_BUDB_GetTpe"
#define BUDB_GetVolEvent   "AFS_BUDB_GetVol"
#define BUDB_UseTpeEvent   "AFS_BUDB_UseTpe"
#define BUDB_TDmpHaEvent   "AFS_BUDB_TDmpHa"
#define BUDB_TGetVrEvent   "AFS_BUDB_TGetVr"
#define BUDB_TDmpDBEvent   "AFS_BUDB_TDmpDB"
#define BUDB_DBVfyEvent    "AFS_BUDB_DBVfy"
#define BUDB_FrALckEvent   "AFS_BUDB_FrALck"
#define BUDB_FreLckEvent   "AFS_BUDB_FreLck"
#define BUDB_GetIIdEvent   "AFS_BUDB_GetIId"
#define BUDB_GetLckEvent   "AFS_BUDB_GetLck"
#define BUDB_GetTxtEvent   "AFS_BUDB_GetTxt"
#define BUDB_GetTxVEvent   "AFS_BUDB_GetTxV"
#define BUDB_SavTxtEvent   "AFS_BUDB_SavTxt"
#define BUDB_DmpDBEvent    "AFS_BUDB_DmpDB"
#define BUDB_RstDBHEvent   "AFS_BUDB_RstDBH"
#define BUDB_AppDmpEvent   "AFS_BUDB_AppDmp"
#define BUDB_FndLTpeEvent  "AFS_BUDB_FnLTpe"

#define AFS_KAA_ChPswdEvent     "AFS_KAA_ChPswd"
#define AFS_KAA_AuthOEvent	"AFS_KAA_AuthO"
#define AFS_KAA_AuthEvent	"AFS_KAA_Auth"

#define AFS_KAT_GetTicketOEvent	"AFS_KAT_GetTktO"
#define AFS_KAT_GetTicketEvent	"AFS_KAT_GetTkt"

#define AFS_KAM_CrUserEvent     "AFS_KAM_CrUser"
#define AFS_KAM_DelUserEvent    "AFS_KAM_DelUser"
#define AFS_KAM_SetPswdEvent    "AFS_KAM_SetPswd"
#define AFS_KAM_LstEntEvent     "AFS_KAM_LstEnt"
#define AFS_KAM_GetPswdEvent    "AFS_KAM_GetPswd"
#define AFS_KAM_DbgEvent        "AFS_KAM_Dbg"
#define AFS_KAM_SetFldEvent     "AFS_KAM_SetFld"
#define AFS_KAM_GetStatEvent    "AFS_KAM_GetStat"
#define AFS_KAM_GetEntEvent     "AFS_KAM_GetEnt"
#define AFS_KAM_GetRndKeyEvent  "AFS_KAM_GRnKey"
#define UnlockEvent             "AFS_UnlockUser"
#define LockStatusEvent         "AFS_LockStatus"

#define UnAuthEvent	        "AFS_UnAth"
#define UseOfPrivilegeEvent     "AFS_UseOfPriv"
#define UDPAuthenticateEvent	"AFS_UDPAuth"
#define UDPGetTicketEvent	"AFS_UDPGetTckt"

#define FetchDataEvent		"AFS_SRX_FchData"
#define FetchACLEvent		"AFS_SRX_FchACL"
#define BulkFetchStatusEvent	"AFS_SRX_BFchSta"
#define FetchStatusEvent	"AFS_SRX_FchStat"
#define StoreDataEvent		"AFS_SRX_StData"
#define StoreACLEvent		"AFS_SRX_StACL"
#define StoreStatusEvent	"AFS_SRX_StStat"
#define RemoveFileEvent		"AFS_SRX_RmFile"
#define CreateFileEvent		"AFS_SRX_CrFile"
#define RenameFileEvent		"AFS_SRX_RNmFile"
#define SymlinkEvent		"AFS_SRX_SymLink"
#define LinkEvent		"AFS_SRX_Link"
#define MakeDirEvent		"AFS_SRX_MakeDir"
#define RemoveDirEvent		"AFS_SRX_RmDir"
#define SetLockEvent		"AFS_SRX_SetLock"
#define ExtendLockEvent		"AFS_SRX_ExtLock"
#define ReleaseLockEvent	"AFS_SRX_RelLock"
#define SetVolumeStatusEvent    "AFS_SRX_SetVolS"
#define FlushCPSEvent           "AFS_SRX_FlusCPS"
#define InlineBulkFetchStatusEvent     "AFS_SRX_BIFchSt"
#define PrivilegeEvent		"AFS_Priv"
#define PrivSetID		"AFS_PrivSet"
/* Next 5 lines on behalf of MR-AFS */
#define GetResidenciesEvent     "AFS_SRX_GetR"
#define ChangeResidenciesEvent  "AFS_SRX_ChgR"
#define ImportResidenciesEvent  "AFS_SRX_Import"
#define GetExtendedVolumeAttributesEvent "AFS_SRX_GetVA"
#define SetExtendedVolumeAttributesEvent "AFS_SRX_SetVA"

#define NoAuthEvent             "AFS_RunNoAuth"
#define NoAuthDisableEvent      "AFS_NoAuthDsbl"
#define NoAuthEnableEvent       "AFS_NoAuthEnbl"

#define BOS_SetRestartEvent     "AFS_BOS_SetReSt"
#define BOS_ExecEvent 		"AFS_BOS_Exec"
#define BOS_UnInstallEvent	"AFS_BOS_UnInst"
#define BOS_InstallEvent	"AFS_BOS_Inst"
#define BOS_SetCellEvent	"AFS_BOS_SetCell"
#define BOS_AddHostEvent	"AFS_BOS_AddHst"
#define BOS_DeleteHostEvent	"AFS_BOS_DelHst"
#define BOS_AddKeyEvent         "AFS_BOS_AddKey"
#define BOS_ListKeysEvent       "AFS_BOS_LstKey"
#define BOS_UnAuthListKeysEvent "AFS_BOS_LstKeyU"
#define BOS_DeleteKeyEvent      "AFS_BOS_DelKey"
#define BOS_SetNoAuthEvent	"AFS_BOS_SetNoAu"
#define BOS_AddSUserEvent       "AFS_BOS_AddSUsr"
#define BOS_ListSUserEvent      "AFS_BOS_LstSUsr"
#define BOS_DeleteSUserEvent    "AFS_BOS_DelSUsr"
#define BOS_CreateBnodeEvent    "AFS_BOS_CreBnod"
#define BOS_DeleteBnodeEvent	"AFS_BOS_DelBnod"
#define BOS_StartupAllEvent     "AFS_BOS_StartAl"
#define BOS_RestartAllEvent     "AFS_BOS_ReStAll"
#define BOS_ShutdownAllEvent    "AFS_BOS_ShtdAll"
#define BOS_WaitAllEvent        "AFS_BOS_WaitAll"
#define BOS_RestartEvent	"AFS_BOS_ReStart"
#define BOS_RebozoEvent         "AFS_BOS_ReBos"
#define BOS_RebozoIntEvent      "AFS_BOS_ReBosIn"
#define BOS_SetTempStatusEvent	"AFS_BOS_SetTSta"
#define BOS_SetStatusEvent      "AFS_BOS_SetStat"
#define BOS_PruneLogs           "AFS_BOS_PrnLog"
#define BOS_GetLogsEvent        "AFS_BOS_GetLog"
#define BOSDoExecEvent          "AFS_BOS_DoExec"
#define BOSStopProcEvent        "AFS_BOS_StpProc"
#define BOSSpawnProcEvent       "AFS_BOS_SpawnProc"

#define	VLCreateEntryEvent	"AFS_VL_CreEnt"
#define VLDeleteEntryEvent	"AFS_VL_DelEnt"
#define VLGetNewVolumeIdEvent	"AFS_VL_GetNVlID"
#define VLReplaceVLEntryEvent	"AFS_VL_RepEnt"
#define VLUpdateEntryEvent	"AFS_VL_UpdEnt"
#define VLSetLockEvent		"AFS_VL_SetLck"
#define VLReleaseLockEvent	"AFS_VL_RelLck"
#define VLChangeAddrEvent	"AFS_VL_ChgAdd"

/* Next 2 block of lines on behalf of MR-AFS */
#define RSStartEvent            "AFS_RS_StartEnt"
#define RSFinishEvent           "AFS_RS_FnshEnt"
#define RSExitEvent             "AFS_RS_ExitEnt"
#define RSChangeAddr            "AFS_RS_ChgAddr"
#define RSChangeName            "AFS_RS_ChgName"
#define RSNewEntry              "AFS_RS_NewEntry"
#define RSAddResidencyToServer  "AFS_RS_AddRToS"
#define RSRemoveResidencyFromServer "AFS_RS_RemRFS"
#define RSNameToResId           "AFS_RS_NameToId"
#define RSResIdToName           "AFS_RS_IdToName"
#define RSDelete                "AFS_RS_Delete"
#define RSListMax               "AFS_RS_ListMax"
#define RSSetMax                "AFS_RS_SetMax"
#define RSListEntry             "AFS_RS_ListEnt"
#define RSSetFieldsEntry        "AFS_RS_SetEnt"
#define RSListElements          "AFS_RS_ListElm"
#define RSIsAHolderOf           "AFS_RS_IsAHold"
#define RSChooseResidency       "AFS_RS_ChooseR"
#define RSSwapAllocatedSpace    "AFS_RS_SwapSpc"
#define RSQuickCheck            "AFS_RS_QuickChk"
#define RSResidencyWipeableInfo "AFS_RS_WipeInfo"
#define RSGetResidencySummary   "AFS_RS_GetRSum"
#define RSGetWiperFreeSpaceFraction "AFS_RS_GetFSpc"
#define RSRegisterAddrs         "AFS_RS_Regist"
#define RSGetAddrsU             "AFS_RS_GetAddrU"
#define RSSetWeights            "AFS_RS_SetWght"
#define RSGetWeights            "AFS_RS_GetWght"
#define RSSetThresholds         "AFS_RS_SetThrsh"
#define RSGetThresholds         "AFS_RS_GetThrsh"
#define RSListResidencies       "AFS_RS_ListRes"
#define RSListServers           "AFS_RS_ListServ"
#define RSGetRandom             "AFS_RS_GetRandm"

#define REMIOExitEvent          "AFS_RE_ExitEnt"
#define SREMIOGetConnection     "AFS_RE_GetConn"
#define SREMIORemoteIncDec      "AFS_RE_IncDec"
#define SREMIOBulkIncDec        "AFS_RE_BlkIDec"
#define SREMIORemoteStat        "AFS_RE_Stat"
#define SREMIORemoteCloseIfOpen "AFS_RE_Close"
#define SREMIORemoteOpen        "AFS_RE_Open"
#define SREMIORemoteSeek        "AFS_RE_Seek"
#define SREMIORemoteRead        "AFS_RE_Read"
#define SREMIORemoteWrite       "AFS_RE_Write"
#define SREMIORemoteListFiles   "AFS_RE_LstFiles"
#define SREMIORemoteTruncate    "AFS_RE_Truncate"
#define SREMIORemoteFsyncFile   "AFS_RE_Fsync"
#define SREMIORemoteImportFile  "AFS_RE_Import"
#define SREMIORemoteGetHSMdata  "AFS_RE_HSMdata"
#define SREMIOPrefetch          "AFS_RE_Prefetch"


/* prototypes for audit functions */
int osi_audit(char *audEvent, afs_int32 errCode, ...);
int osi_auditU(struct rx_call *call, char *audEvent, int errCode, ...);
