/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include "cellservdb.h"

#ifdef AFS_NT40_ENV
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <WINNT\afsreg.h>

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

#define new(_t)     (_t*)malloc(sizeof(_t))
#define delete(_p)  free((void*)(_p))

#ifndef iswhite
#define iswhite(_ch) (((_ch)==' ') || ((_ch)=='\t'))
#endif
#ifndef iseol
#define iseol(_ch) (((_ch)=='\r') || ((_ch)=='\n'))
#endif
#ifndef iswhiteeol
#define iswhiteeol(_ch) (iswhite(_ch) || iseol(_ch))
#endif
#ifndef min
#define min(_a,_b) ((_a) < (_b) ? (_a) : (_b))
#endif


/*
 * STATICS ____________________________________________________________________
 *
 */

static void strzcpy (char *pszTarget, const char *pszSource, size_t cch)
{
   cch = min(cch, (size_t)(1+strlen(pszSource)));
   strncpy (pszTarget, pszSource, cch-1);
   pszTarget[ cch-1 ] = '\0';
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void CSDB_GetFileName (char *pszFilename)
{
#ifdef AFS_NT40_ENV
   /* Find the appropriate CellServDB */
    char * clientdir = 0;
	DWORD code, dummyLen;
	HKEY parmKey;
    int tlen;

	code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
				0, KEY_QUERY_VALUE, &parmKey);
	if (code != ERROR_SUCCESS)
        goto dirpath;

	dummyLen = MAX_CSDB_PATH;
	code = RegQueryValueEx(parmKey, "CellServDBDir", NULL, NULL,
				pszFilename, &dummyLen);
	RegCloseKey (parmKey);

  dirpath:
	if (code != ERROR_SUCCESS || pszFilename[0] == 0) {
        afssw_GetClientInstallDir(&clientdir);
        if (clientdir) {
            strncpy(pszFilename, clientdir, MAX_CSDB_PATH);
            pszFilename[MAX_CSDB_PATH - 1] = '\0';
        }
    }
    if (pszFilename[ strlen(pszFilename)-1 ] != '\\')
      strcat (pszFilename, "\\");

   strcat (pszFilename, "CellServDB");
#else
   strcpy (pszFilename, "/usr/vice/etc/CellServDB");
#endif
}


BOOL CSDB_ReadFile (PCELLSERVDB pCellServDB, const char *pszFilename)
{
   BOOL rc = FALSE;
   FILE *pFile;
   size_t cbLength;
   size_t cbRead;
   char *pszBuffer;
   char *pszStart;
   char *pszEnd;
   PCELLDBLINE pLine;

   memset (pCellServDB, 0x00, sizeof(CELLSERVDB));

   /* Open AFSDCELL.INI and read it into memory. */

   if (pszFilename)
      strcpy (pCellServDB->szFilename, pszFilename);
   else
      CSDB_GetFileName (pCellServDB->szFilename);

   if ((pFile = fopen (pCellServDB->szFilename, "r")) != NULL)
      {
      fseek (pFile, 0, 2);
      cbLength = ftell (pFile);
      fseek (pFile, 0, 0);

      pszBuffer = (char*)malloc (sizeof(char) * (cbLength +2));

      if ((cbRead = fread (pszBuffer, 1, cbLength, pFile)) != 0)
         {
         pszBuffer[ cbRead ] = '\0';
         pszBuffer[ cbRead+1 ] = '\0';

         /* Scan the file line-by-line... */

         for (pszStart = pszBuffer; pszStart && *pszStart; )
            {
            while (iswhiteeol(*pszStart))
               ++pszStart;
            if (!*pszStart)
               break;

            for (pszEnd = pszStart; *pszEnd && !iseol(*pszEnd); ++pszEnd)
               ;
            *pszEnd++ = '\0';

            /* Add this line to our chain */

            pLine = new(CELLDBLINE);
            memset (pLine, 0x00, sizeof(CELLDBLINE));
            strzcpy (pLine->szLine, pszStart, cchCELLDBLINE-1);
            pLine->szLine[ cchCELLDBLINE-1 ] = '\0';
            if ((pLine->pPrev = pCellServDB->pLast) != NULL)
               pLine->pPrev->pNext = pLine;
            if ((pCellServDB->pLast = pLine)->pPrev == NULL)
               pCellServDB->pFirst = pLine;

            /* Process the next line in the file */

            pszStart = pszEnd;
            }

         rc = TRUE;
         }

      free (pszBuffer);
      fclose (pFile);
      }

   return rc;
}


BOOL CSDB_WriteFile (PCELLSERVDB pCellServDB)
{
   BOOL rc = TRUE;
   FILE *pFile;
   char szLine[ cchCELLDBLINE ];
   PCELLDBLINE pLine;

   if (pCellServDB->fChanged)
      {
      if ((pFile = fopen (pCellServDB->szFilename, "w")) == NULL)
         {
         rc = FALSE;
         }
      else
         {
         for (pLine = pCellServDB->pFirst; pLine; pLine = pLine->pNext)
            {
#ifdef AFS_NT40_ENV
            sprintf (szLine, "%s\r\n", pLine->szLine);
#else
            sprintf (szLine, "%s\n", pLine->szLine);
#endif
            fwrite (szLine, 1, strlen(szLine), pFile);
            }

         fclose (pFile);
         }

      pCellServDB->fChanged = FALSE;
      }

   return rc;
}


void CSDB_FreeFile (PCELLSERVDB pCellServDB)
{
   PCELLDBLINE pNext;
   PCELLDBLINE pLine;
   for (pLine = pCellServDB->pFirst; pLine; pLine = pNext)
      {
      pNext = pLine->pNext;
      delete(pLine);
      }
   memset (pCellServDB, 0x00, sizeof(CELLSERVDB));
}


BOOL CSDB_CrackLine (PCELLDBLINEINFO pInfo, const char *pszLine)
{
   char *pszOut;
   BOOL fIsCell = TRUE;
   BOOL fSawHash = FALSE;

   memset (pInfo, 0x00, sizeof(CELLDBLINEINFO));

   if (!pszLine || !*pszLine)
      return FALSE;

   while (iswhite(*pszLine))
      ++pszLine;

   if (*pszLine == '>')
      ++pszLine;
   else if (!isdigit (*pszLine))
      return FALSE;
   else /* (isdigit (*pszLine)) */
      fIsCell = FALSE;

   for (pszOut = pInfo->szCell; *pszLine && (!iswhite(*pszLine)) && (*pszLine != '#'); )
      *pszOut++ = *pszLine++;
   *pszOut = '\0';

   while (iswhite(*pszLine) || (*pszLine == '#'))
      {
      fSawHash = fSawHash || (*pszLine == '#');
      ++pszLine;
      }

   if (fIsCell && *pszLine && !fSawHash)
      {
      for (pszOut = pInfo->szLinkedCell; *pszLine && (!iswhite(*pszLine)) && (*pszLine != '#'); )
         *pszOut++ = *pszLine++;
      *pszOut = '\0';

      while (iswhite(*pszLine) || (*pszLine == '#'))
         ++pszLine;
      }

   for (pszOut = pInfo->szComment; *pszLine; )
      *pszOut++ = *pszLine++;
   *pszOut = '\0';

   if (!pInfo->szCell[0])
      return FALSE;

   if (!fIsCell)
      {
      if ((pInfo->ipServer = inet_addr (pInfo->szCell)) == 0xffffffff)
         return FALSE;
      pInfo->szCell[0] = '\0';
      }

   return TRUE;
}


BOOL CSDB_FormatLine (char *pszLine, const char *pszCell, const char *pszLinkedCell, const char *pszComment, BOOL fIsCell)
{
   if (fIsCell)
      sprintf (pszLine, ">%s", pszCell);
   else
      strcpy (pszLine, pszCell);

   if (fIsCell && pszLinkedCell && *pszLinkedCell)
      sprintf (&pszLine[ strlen(pszLine) ], " %s", pszLinkedCell);

   if (pszComment)
      {
      size_t cchSpacing = (fIsCell) ? 28 : 33;
      strcat (pszLine, " ");
      if ((size_t)strlen(pszLine) < cchSpacing)
         {
         strcat (pszLine, "                                 ");
         pszLine[cchSpacing] = '\0';
         }

      sprintf (&pszLine[ strlen(pszLine) ], ((fIsCell) ? "# %s" : "#%s"), pszComment);
      }

   return TRUE;
}


PCELLDBLINE CSDB_FindCell (PCELLSERVDB pCellServDB, const char *pszCell)
{
   PCELLDBLINE pLine;
   for (pLine = pCellServDB->pFirst; pLine; pLine = pLine->pNext)
      {
      CELLDBLINEINFO Info;
      if (!CSDB_CrackLine (&Info, pLine->szLine))
         continue;
      if (!strcmpi (Info.szCell, pszCell))
         return pLine;
      }
   return NULL;
}


BOOL CSDB_OnRemove (PCELLSERVDB pCellServDB, PCELLDBLINE pCellLine, BOOL fRemoveCellLineToo)
{
   CELLDBLINEINFO Info;
   PCELLDBLINE pNext;
   PCELLDBLINE pLine;

   /* Quick validation: make sure the caller specified a Cell line */

   if (!pCellLine)
      return FALSE;
   if (!CSDB_CrackLine (&Info, pCellLine->szLine))
      return FALSE;
   if (!Info.szCell[0])
      return FALSE;

   /* Remove everything about this cell (except maybe the cell line) */

   pLine = (fRemoveCellLineToo) ? pCellLine : pCellLine->pNext;
   for ( ; pLine; pLine = pNext)
      {
      if ((pNext = CSDB_RemoveLine (pCellServDB, pLine)) != NULL)
         {
         if (!CSDB_CrackLine (&Info, pNext->szLine))
            break;
         if (Info.szCell[0]) /* Hit the next cell? We're done! */
            break;
         }
      }

   pCellServDB->fChanged = TRUE;
   return TRUE;
}

BOOL CSDB_RemoveCell (PCELLSERVDB pCellServDB, PCELLDBLINE pCellLine)
{
   return CSDB_OnRemove (pCellServDB, pCellLine, TRUE);
}
   
BOOL CSDB_RemoveCellServers (PCELLSERVDB pCellServDB, PCELLDBLINE pCellLine)
{
   return CSDB_OnRemove (pCellServDB, pCellLine, FALSE);
}


PCELLDBLINE CSDB_AddCell (PCELLSERVDB pCellServDB, const char *pszCell, const char *pszLinkedCell, const char *pszComment)
{
   PCELLDBLINE pCellLine;

   /* Find out if there's already an entry in CellServDB for this cell; */
   /* add one if necessary. */

   if ((pCellLine = CSDB_FindCell (pCellServDB, pszCell)) == NULL)
      {
      pCellLine = new(CELLDBLINE);
      memset (pCellLine, 0x00, sizeof(CELLDBLINE));
      if ((pCellLine->pPrev = pCellServDB->pLast) != NULL)
         pCellLine->pPrev->pNext = pCellLine;
      if ((pCellServDB->pLast = pCellLine)->pPrev == NULL)
         pCellServDB->pFirst = pCellLine;
      }

   CSDB_FormatLine (pCellLine->szLine, pszCell, pszLinkedCell, pszComment, TRUE);
   pCellServDB->fChanged = TRUE;
   return pCellLine;
}


PCELLDBLINE CSDB_AddCellServer (PCELLSERVDB pCellServDB, PCELLDBLINE pAddAfter, const char *pszAddress, const char *pszComment)
{
   char szLine[ cchCELLDBLINE ];
   CSDB_FormatLine (szLine, pszAddress, NULL, pszComment, FALSE);
   return CSDB_AddLine (pCellServDB, pAddAfter, szLine);
}


PCELLDBLINE CSDB_AddLine (PCELLSERVDB pCellServDB, PCELLDBLINE pAddAfter, const char *pszLine)
{
   PCELLDBLINE pNew = new(CELLDBLINE);
   memset (pNew, 0x00, sizeof(CELLDBLINE));
   strcpy (pNew->szLine, pszLine);

   if (pAddAfter == NULL)
      {
      if ((pNew->pNext = pCellServDB->pFirst) != NULL)
         pNew->pNext->pPrev = pNew;
      pNew->pPrev = NULL;
      pCellServDB->pFirst = pNew;
      if (pCellServDB->pLast == NULL)
         pCellServDB->pLast = pNew;
      }
   else /* (pAddAfter != NULL) */
      {
      if ((pNew->pNext = pAddAfter->pNext) != NULL)
         pNew->pNext->pPrev = pNew->pPrev;
      pNew->pPrev = pAddAfter;
      pAddAfter->pNext = pNew;
      if (pCellServDB->pLast == pAddAfter)
         pCellServDB->pLast = pNew;
      }

   pCellServDB->fChanged = TRUE;
   return pNew;
}


PCELLDBLINE CSDB_RemoveLine (PCELLSERVDB pCellServDB, PCELLDBLINE pRemove)
{
   PCELLDBLINE pNext;

   if (!pRemove)
      return NULL;

   pNext = pRemove->pNext;

   if (pRemove->pPrev)
      pRemove->pPrev->pNext = pRemove->pNext;
   if (pRemove->pNext)
      pRemove->pNext->pPrev = pRemove->pPrev;
   if (pCellServDB->pFirst == pRemove)
      pCellServDB->pFirst = pRemove->pNext;
   if (pCellServDB->pLast == pRemove)
      pCellServDB->pLast = pRemove->pPrev;

   delete(pRemove);

   pCellServDB->fChanged = TRUE;
   return pNext;
}

