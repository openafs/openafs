#ifndef CELLSERVDB_H
#define CELLSERVDB_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX_CSDB_PATH
#define MAX_CSDB_PATH 2048
#endif


#define cchCELLDBLINE  512

typedef struct CELLDBLINE CELLDBLINE, *PCELLDBLINE;
struct CELLDBLINE
   {
   char szLine[ cchCELLDBLINE ];
   PCELLDBLINE pPrev;
   PCELLDBLINE pNext;
   };

typedef struct
   {
   char szFilename[ MAX_CSDB_PATH ];
   BOOL fChanged;
   PCELLDBLINE pFirst;
   PCELLDBLINE pLast;
   } CELLSERVDB, *PCELLSERVDB;

typedef struct
   {
   char szCell[ cchCELLDBLINE ];
   char szLinkedCell[ cchCELLDBLINE ];
   int ipServer;
   char szComment[ cchCELLDBLINE ];
   } CELLDBLINEINFO, *PCELLDBLINEINFO;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void CSDB_GetFileName (char *pszFilename);

BOOL CSDB_ReadFile (PCELLSERVDB pCellServDB, const char *pszFilename);
BOOL CSDB_WriteFile (PCELLSERVDB pCellServDB);
void CSDB_FreeFile (PCELLSERVDB pCellServDB);

BOOL CSDB_CrackLine (PCELLDBLINEINFO pInfo, const char *pszLine);
BOOL CSDB_FormatLine (char *pszLine, const char *pszCell, const char *pszLinkedCell, const char *pszComment, BOOL fIsCell);

PCELLDBLINE CSDB_FindCell (PCELLSERVDB pCellServDB, const char *pszCell);

BOOL CSDB_RemoveCell (PCELLSERVDB pCellServDB, PCELLDBLINE pCellLine);
BOOL CSDB_RemoveCellServers (PCELLSERVDB pCellServDB, PCELLDBLINE pCellLine);

PCELLDBLINE CSDB_AddCell (PCELLSERVDB pCellServDB, const char *pszCell, const char *pszLinkedCell, const char *pszComment);
PCELLDBLINE CSDB_AddCellServer (PCELLSERVDB pCellServDB, PCELLDBLINE pAddAfter, const char *pszAddress, const char *pszComment);

PCELLDBLINE CSDB_AddLine (PCELLSERVDB pCellServDB, PCELLDBLINE pAddAfter, const char *pszLine);
PCELLDBLINE CSDB_RemoveLine (PCELLSERVDB pCellServDB, PCELLDBLINE pRemove);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif

