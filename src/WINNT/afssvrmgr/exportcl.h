/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef EXPORT_CELL_H
#define EXPORT_CELL_H
#ifdef DEBUG

#define eckCELL        TEXT("cell")
#define eckSERVER      TEXT("server")
#define eckSERVICE     TEXT("service")
#define eckAGGREGATE   TEXT("partition")
#define eckFILESET     TEXT("volume")
#define eckPRINCIPAL   TEXT("principal")

      // For a server:
      //
#define eckADDRESS     TEXT("address")      // if server is DB and Vol server
#define eckDBADDRESS   TEXT("dbaddress")    // if server is just DB server
#define eckVOLADDRESS  TEXT("voladdress")   // if server is just Vol server
#define eckHOST        TEXT("hostname")
#define eckADMIN       TEXT("admin")
//#def  eckKEYVERSION  TEXT("key_version")  // from principal codes
//#def  eckKEYDATA     TEXT("key_data")     // from principal codes

      // For a service:
      //
#define eckRUNNING     TEXT("running")
#define eckSIMPLE      TEXT("simple")
#define eckCRON        TEXT("cron")
#define eckTYPE        TEXT("type")
#define eckFS          TEXT("fs")
#define eckSTATE       TEXT("state")
#define eckNSTARTS     TEXT("nstarts")
#define eckERRLAST     TEXT("lasterr")
#define eckSIGLAST     TEXT("lastsig")
#define eckPARAMS      TEXT("parameters")
#define eckNOTIFIER    TEXT("notifier")
#define eckSTARTTIME   TEXT("starttime")
#define eckSTOPTIME    TEXT("stoptime")
#define eckERRORTIME   TEXT("errortime")

      // For an partition:
      //
#define eckDEVICE      TEXT("device")
#define eckTOTAL       TEXT("total")
#define eckFREECUR     TEXT("curfree")

      // For a volume:
      //
#define eckID          TEXT("identifier")
#define eckID_RW       TEXT("id_rwlink")
#define eckID_BK       TEXT("id_bklink")
#define eckUSED        TEXT("used")
#define eckQUOTA       TEXT("quota")
#define eckCREATETIME  TEXT("createtime")
#define eckUPDATETIME  TEXT("updatetime")
#define eckACCESSTIME  TEXT("accesstime")
#define eckBACKUPTIME  TEXT("backuptime")

      // For a replicated volume:
      //
#define eckRP_MAXSITEAGE  TEXT("maxsiteage")
#define eckRP_MAXAGE      TEXT("maxage")
#define eckRP_FAILAGE     TEXT("failage")
#define eckRP_MINREP      TEXT("minrepdelay")
#define eckRP_DEFSITEAGE  TEXT("defsiteage")
#define eckRP_RECWAIT     TEXT("reclaimwait")

      // For a principal:
      //
#define eckEXPIRES        TEXT("expires")
#define eckLASTMOD        TEXT("modified")
#define eckLASTPWMOD      TEXT("pw_modified")
#define eckLIFETIME       TEXT("lifetime")
#define eckLOCKTIME       TEXT("locktime")
#define eckPWEXPIRES      TEXT("pw_expires")
#define eckFAILLOGIN      TEXT("fail_login")
#define eckKEYVERSION     TEXT("key_version")
#define eckKEYDATA        TEXT("key_data")
#define eckKEYCSUM        TEXT("key_csum")
#define eckPWREUSE        TEXT("pw_reuse")
#define eckCREATOR        TEXT("creator")
#define eckGRP_QUOTA      TEXT("grp_quota")

      // For a group:
      //
#define eckGROUP          TEXT("group")
#define eckMEMBER         TEXT("member")
#define eckOWNER          TEXT("owner")

#endif
#endif

