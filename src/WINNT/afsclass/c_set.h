/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_FILESET_H
#define AFSCLASS_FILESET_H


/*
 * FILESET CLASS ______________________________________________________________
 *
 */

#define fsNORMAL        0x00000000
#define fsSALVAGE       0x00000001
#define fsNO_VNODE      0x00000002
#define fsNO_VOL        0x00000004
#define fsNO_SERVICE    0x00000008
#define fsOFFLINE       0x00000010
#define fsDISK_FULL     0x00000020
#define fsOVER_QUOTA    0x00000040
#define fsBUSY          0x00000080
#define fsMOVED         0x00000100
#define fsLOCKED        0x00010000
#define fsMASK_VLDB     0xFFFF0000
typedef DWORD FILESETSTATE;

typedef enum
   {
   ftREADWRITE,
   ftREPLICA,
   ftCLONE
   } FILESETTYPE;

typedef struct
   {
   VOLUMEID id;
   VOLUMEID idReadWrite;
   VOLUMEID idReplica;
   VOLUMEID idClone;
   SYSTEMTIME timeCreation;
   SYSTEMTIME timeLastUpdate;
   SYSTEMTIME timeLastAccess;
   SYSTEMTIME timeLastBackup;
   SYSTEMTIME timeCopyCreation;
   ULONG nFiles;
   size_t ckQuota;
   size_t ckUsed;
   FILESETTYPE Type;
   FILESETSTATE State;
   } FILESETSTATUS, *LPFILESETSTATUS;

class FILESET
   {
   friend class CELL;
   friend class SERVER;
   friend class AGGREGATE;
   friend class IDENT;

   public:
      void Close (void);
      void Invalidate (void);
      BOOL RefreshStatus (BOOL fNotify = TRUE, ULONG *pStatus = NULL);	// does nothing if not invalidated
      BOOL RefreshStatus_VLDB (BOOL fNotify = TRUE, ULONG *pStatus = NULL);	// does nothing if not invalidated

      LPCELL OpenCell (ULONG *pStatus = NULL);
      LPSERVER OpenServer (ULONG *pStatus = NULL);
      LPAGGREGATE OpenAggregate (ULONG *pStatus = NULL);

      // Fileset properties
      //
      LPIDENT GetIdentifier (void);
      LPIDENT GetReadWriteIdentifier (ULONG *pStatus = NULL);
      LPIDENT GetReadOnlyIdentifier (LPIDENT lpiParent, ULONG *pStatus = NULL);
      LPIDENT GetCloneIdentifier (ULONG *pStatus = NULL);
      void GetName (LPTSTR pszName);
      void GetID (LPVOLUMEID pvid);

      BOOL GetStatus (LPFILESETSTATUS lpfs, BOOL fNotify = TRUE, ULONG *pStatus = NULL);
      short GetGhostStatus (void);	// returns GHOST_*

      PVOID GetUserParam (void);
      void SetUserParam (PVOID pUserParam);

   private:
      FILESET (LPAGGREGATE lpAggregateParent, LPVOLUMEID pvid, LPTSTR pszName);
      ~FILESET (void);
      void SendDeleteNotifications (void);

      BOOL ProbablyReplica (void);
      void SetStatusFromVOS (PVOID /* vos_volumeEntry_p */ pEntry);

   private:
      LPIDENT m_lpiCell;
      LPIDENT m_lpiServer;
      LPIDENT m_lpiAggregate;
      LPIDENT m_lpiThis;
      LPIDENT m_lpiThisRW;
      LPIDENT m_lpiThisBK;
      VOLUMEID m_idVolume;

      TCHAR m_szName[ cchNAME ];
      short m_wGhost;

      BOOL m_fStatusOutOfDate;
      FILESETSTATUS m_fs;
   };


#endif // AFSCLASS_FILESET_H

