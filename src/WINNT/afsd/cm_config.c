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

#ifndef DJGPP
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif /* !DJGPP */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cm_config.h"

char AFSConfigKeyName[] =
	"SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters";

#define AFS_THISCELL "ThisCell"
#define AFS_CELLSERVDB_UNIX "CellServDB"
#define AFS_CELLSERVDB_NT "afsdcell.ini"
#define AFSDIR_CLIENT_ETC_DIRPATH "c:/afs"
#if defined(DJGPP) || defined(AFS_WIN95_ENV)
#define AFS_CELLSERVDB AFS_CELLSERVDB_UNIX
#ifdef DJGPP
extern char cm_confDir[];
extern int errno;
#endif /* DJGPP */
#else
#define AFS_CELLSERVDB AFS_CELLSERVDB_NT
#endif /* DJGPP || WIN95 */


static long cm_ParsePair(char *lineBufferp, char *leftp, char *rightp)
{
	char *tp;
        char tc;
        int sawEquals;
	int sawBracket;
        
        sawEquals = 0;
	sawBracket = 0;
        for(tp = lineBufferp; *tp; tp++) {
		tc = *tp;

		if (sawBracket) {
			if (tc == ']')
				sawBracket = 0;
			continue;
		}

		/* comment or line end */
                if (tc == '#' || tc == '\r' || tc == '\n') break;

		/* square bracket comment -- look for closing delim
		if (tc == '[') {sawBracket = 1; continue;}

		/* space or tab */
                if (tc == ' ' || tc == '\t') continue;

                if (tc == '=') {
                	sawEquals = 1;
                        continue;
		}
                
                /* now we have a real character, put it in the appropriate bucket */
                if (sawEquals == 0) {
			*leftp++ = tc;
                }
                else {
			*rightp++ = tc;
                }
        }

	/* null terminate the strings */
	*leftp = 0;
        *rightp = 0;

        return 0;	/* and return success */
}

/* search for a cell, and either return an error code if we don't find it,
 * or return 0 if we do, in which case we also fill in the addresses in
 * the cellp field.
 *
 * new feature:  we can handle abbreviations and are insensitive to case.
 * If the caller wants the "real" cell name, it puts a non-null pointer in
 * newCellNamep.  Anomaly:  if cellNamep is ambiguous, we may modify
 * newCellNamep but return an error code.
 */
long cm_SearchCellFile(char *cellNamep, char *newCellNamep,
	cm_configProc_t *procp, void *rockp)
{
	char wdir[256];
        int tlen;
        FILE *tfilep, *bestp, *tempp;
        char *tp;
        char lineBuffer[256];
        struct hostent *thp;
        char *valuep;
        struct sockaddr_in vlSockAddr;
        int inRightCell;
        int foundCell;
        long code;
	int tracking = 1, partial = 0;
        long ip_addr;
#if defined(DJGPP) || defined(AFS_WIN95_ENV)
        int c1, c2, c3, c4;
        char aname[256];
#endif
        char *afsconf_path;

	foundCell = 0;

#if !defined(DJGPP)
	code = GetWindowsDirectory(wdir, sizeof(wdir));
        if (code == 0 || code > sizeof(wdir)) return -1;

	/* add trailing backslash, if required */
        tlen = strlen(wdir);
        if (wdir[tlen-1] != '\\') strcat(wdir, "\\");
#else
        strcpy(wdir, cm_confDir);
        strcat(wdir,"/");
#endif /* !DJGPP */
        
        strcat(wdir, AFS_CELLSERVDB);

        tfilep = fopen(wdir, "r");

        if (!tfilep) {
          /* If we are using DJGPP client, cellservdb will be in afsconf dir. */
          /* If we are in Win95 here, we are linking with klog etc. and are
             using DJGPP client even though DJGPP is not defined.  So we still
             need to check AFSCONF for location. */
            afsconf_path = getenv("AFSCONF");
            if (!afsconf_path)
               strcpy(wdir, AFSDIR_CLIENT_ETC_DIRPATH);
            else
               strcpy(wdir, afsconf_path);
            strcat(wdir, "/");
            strcat(wdir, AFS_CELLSERVDB_UNIX);
            /*fprintf(stderr, "opening cellservdb file %s\n", wdir);*/
            tfilep = fopen(wdir, "r");
            if (!tfilep) return -2;
        }

	bestp = fopen(wdir, "r");
        
	/* have we seen the cell line for the guy we're looking for? */
	inRightCell = 0;
	while (1) {
	        tp = fgets(lineBuffer, sizeof(lineBuffer), tfilep);
	        if (tracking)
			(void) fgets(lineBuffer, sizeof(lineBuffer), bestp);
                if (tp == NULL) {
			if (feof(tfilep)) {
				/* hit EOF */
				if (partial) {
					/*
					 * found partial match earlier;
					 * now go back to it
					 */
					tempp = bestp;
					bestp = tfilep;
					tfilep = tempp;
					inRightCell = 1;
					partial = 0;
					continue;
				}
				else {
					fclose(tfilep);
					fclose(bestp);
					return (foundCell? 0 : -3);
				}
			}
                }
                
                /* turn trailing cr or lf into null */
                tp = strchr(lineBuffer, '\r');
                if (tp) *tp = 0;
                tp = strchr(lineBuffer, '\n');
                if (tp) *tp = 0;
                
		/* skip blank lines */
                if (lineBuffer[0] == 0) continue;

                if (lineBuffer[0] == '>') {
			/* trim off at white space or '#' chars */
                        tp = strchr(lineBuffer, ' ');
                        if (tp) *tp = 0;
                        tp = strchr(lineBuffer, '\t');
                        if (tp) *tp = 0;
                        tp = strchr(lineBuffer, '#');
                        if (tp) *tp = 0;

			/* now see if this is the right cell */
                	if (stricmp(lineBuffer+1, cellNamep) == 0) {
				/* found the cell we're looking for */
				if (newCellNamep)
					strcpy(newCellNamep, lineBuffer+1);
	                        inRightCell = 1;
				tracking = 0;
			}
			else if (strnicmp(lineBuffer+1, cellNamep,
					  strlen(cellNamep)) == 0) {
				/* partial match */
				if (partial) {	/* ambiguous */
					fclose(tfilep);
					fclose(bestp);
					return -5;
				}
				if (newCellNamep)
					strcpy(newCellNamep, lineBuffer+1);
				inRightCell = 0;
				tracking = 0;
				partial = 1;
			}
                        else inRightCell = 0;
                }
                else {
#if !defined(DJGPP) && !defined(AFS_WIN95_ENV)
                	valuep = strchr(lineBuffer, '#');
			if (valuep == NULL) {
				fclose(tfilep);
				fclose(bestp);
				return -4;
			}
                        valuep++;	/* skip the "#" */

                        valuep += strspn(valuep, " 	"); /* skip SP & TAB */
#endif /* !DJGPP */
			if (inRightCell) {
#if !defined(DJGPP) && !defined(AFS_WIN95_ENV)
				/* add the server to the VLDB list */
                                thp = gethostbyname(valuep);
                                if (thp) {
					memcpy(&vlSockAddr.sin_addr.s_addr, thp->h_addr,
                                        	sizeof(long));
                                        vlSockAddr.sin_family = AF_INET;
                                        /* sin_port supplied by connection code */
					if (procp)
						(*procp)(rockp, &vlSockAddr, valuep);
	                                foundCell = 1;
				}
#else
                                /* For DJGPP, we will read IP address instead
                                   of name/comment field */
                                code = sscanf(lineBuffer, "%d.%d.%d.%d #%s",
                                              &c1, &c2, &c3, &c4, aname);
                                tp = (char *) &ip_addr;
                                *tp++ = c1;
                                *tp++ = c2;
                                *tp++ = c3;
                                *tp++ = c4;
                                memcpy(&vlSockAddr.sin_addr.s_addr, &ip_addr,
                                        	sizeof(long));
                                vlSockAddr.sin_family = AF_INET;
                                /* sin_port supplied by connection code */
                                if (procp)
                                  (*procp)(rockp, &vlSockAddr, valuep);
                                foundCell = 1;
#endif /* !DJGPP */
                        }
                }	/* a vldb line */
        }		/* while loop processing all lines */
}

#if !defined(DJGPP) && !defined(AFS_WIN95_ENV)
/* look up the root cell's name in the Registry */
long cm_GetRootCellName(char *cellNamep)
{
	DWORD code, dummyLen;
	HKEY parmKey;

	code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSConfigKeyName,
				0, KEY_QUERY_VALUE, &parmKey);
	if (code != ERROR_SUCCESS)
		return -1;

	dummyLen = 256;
	code = RegQueryValueEx(parmKey, "Cell", NULL, NULL,
				cellNamep, &dummyLen);
	RegCloseKey (parmKey);
	if (code != ERROR_SUCCESS)
		return -1;

	return 0;
}
#else
/* look up the root cell's name in the THISCELL file */
long cm_GetRootCellName(char *cellNamep)
{
        FILE *thisCell;
        char thisCellPath[256];
        char *afsconf_path;
        char *newline;

#ifdef DJGPP
        strcpy(thisCellPath, cm_confDir);
#else
        /* Win 95 */
        afsconf_path = getenv("AFSCONF");
        if (!afsconf_path)
          strcpy(thisCellPath, AFSDIR_CLIENT_ETC_DIRPATH);
        else
          strcpy(thisCellPath, afsconf_path);
#endif
        strcat(thisCellPath,"/");

        strcat(thisCellPath, AFS_THISCELL);
        thisCell = fopen(thisCellPath, "r");
        if (thisCell == NULL)
          return -1;

        fgets(cellNamep, 256, thisCell);
        fclose(thisCell);

        newline = strrchr(cellNamep,'\n');
        if (newline) *newline = '\0';
        newline = strrchr(cellNamep,'\r');
        if (newline) *newline = '\0';

        return 0;
}
#endif /* !DJGPP */

cm_configFile_t *cm_CommonOpen(char *namep, char *rwp)
{
	char wdir[256];
        long code;
        long tlen;
        FILE *tfilep;
        char *afsconf_path;

#if !defined(DJGPP) && !defined(AFS_WIN95_ENV)
	code = GetWindowsDirectory(wdir, sizeof(wdir));
        if (code == 0 || code > sizeof(wdir)) return NULL;
        
	/* add trailing backslash, if required */
        tlen = strlen(wdir);
        if (wdir[tlen-1] != '\\') strcat(wdir, "\\");
#else
#ifdef DJGPP
        strcpy(wdir,cm_confDir);
#else
        afsconf_path = getenv("AFSCONF");
        if (!afsconf_path)
          strcpy(wdir, AFSDIR_CLIENT_ETC_DIRPATH);
        else
          strcpy(wdir, afsconf_path);
#endif /* !DJGPP */
        strcat(wdir,"/");
#endif /* DJGPP || WIN95 */

        strcat(wdir, namep);
        
        tfilep = fopen(wdir, rwp);

	return ((cm_configFile_t *) tfilep);        
}

#ifndef DJGPP
long cm_WriteConfigString(char *labelp, char *valuep)
{
	DWORD code, dummyDisp;
	HKEY parmKey;

	code = RegCreateKeyEx(HKEY_LOCAL_MACHINE, AFSConfigKeyName,
				0, "container", 0, KEY_SET_VALUE, NULL,
				&parmKey, &dummyDisp);
	if (code != ERROR_SUCCESS)
		return -1;

	code = RegSetValueEx(parmKey, labelp, 0, REG_SZ,
			     valuep, strlen(valuep) + 1);
	RegCloseKey (parmKey);
	if (code != ERROR_SUCCESS)
		return -1;

        return 0;
}
#endif /* !DJGPP */

#ifndef DJGPP
long cm_WriteConfigInt(char *labelp, long value)
{
	DWORD code, dummyDisp;
	HKEY parmKey;

	code = RegCreateKeyEx(HKEY_LOCAL_MACHINE, AFSConfigKeyName,
				0, "container", 0, KEY_SET_VALUE, NULL,
				&parmKey, &dummyDisp);
	if (code != ERROR_SUCCESS)
		return -1;

	code = RegSetValueEx(parmKey, labelp, 0, REG_DWORD,
			     &value, sizeof(value));
	RegCloseKey (parmKey);
	if (code != ERROR_SUCCESS)
		return -1;

	return 0;
}
#endif /* !DJGPP */

cm_configFile_t *cm_OpenCellFile(void)
{
        cm_configFile_t *cfp;

	cfp = cm_CommonOpen("afsdcel2.ini", "w");
	return cfp;
}

long cm_AppendPrunedCellList(cm_configFile_t *ofp, char *cellNamep)
{
	cm_configFile_t *tfilep;	/* input file */
        char *tp;
        char lineBuffer[256];
        char *valuep;
        int inRightCell;
        int foundCell;

        tfilep = cm_CommonOpen(AFS_CELLSERVDB, "r");
        if (!tfilep) return -1;

	foundCell = 0;

	/* have we seen the cell line for the guy we're looking for? */
	inRightCell = 0;
	while (1) {
	        tp = fgets(lineBuffer, sizeof(lineBuffer), (FILE *)tfilep);
                if (tp == NULL) {
			if (feof((FILE *)tfilep)) {
				/* hit EOF */
				fclose((FILE *)tfilep);
				return 0;
			}
                }
                
                /* turn trailing cr or lf into null */
                tp = strchr(lineBuffer, '\r');
                if (tp) *tp = 0;
                tp = strchr(lineBuffer, '\n');
                if (tp) *tp = 0;
                
		/* skip blank lines */
                if (lineBuffer[0] == 0) {
			fprintf((FILE *)ofp, "%s\n", lineBuffer);
                	continue;
		}

                if (lineBuffer[0] == '>') {
			/* trim off at white space or '#' chars */
                        tp = strchr(lineBuffer, ' ');
                        if (tp) *tp = 0;
                        tp = strchr(lineBuffer, '\t');
                        if (tp) *tp = 0;
                        tp = strchr(lineBuffer, '#');
                        if (tp) *tp = 0;

			/* now see if this is the right cell */
                	if (strcmp(lineBuffer+1, cellNamep) == 0) {
				/* found the cell we're looking for */
	                        inRightCell = 1;
			}
                        else {
                        	inRightCell = 0;
                                fprintf((FILE *)ofp, "%s\n", lineBuffer);
			}
                }
                else {
                	valuep = strchr(lineBuffer, '#');
			if (valuep == NULL) return -2;
                        valuep++;	/* skip the "#" */
			if (!inRightCell) {
                                fprintf((FILE *)ofp, "%s\n", lineBuffer);
                        }
                }	/* a vldb line */
        }		/* while loop processing all lines */
}

long cm_AppendNewCell(cm_configFile_t *filep, char *cellNamep)
{
	fprintf((FILE *)filep, ">%s\n", cellNamep);
        return 0;
}

long cm_AppendNewCellLine(cm_configFile_t *filep, char *linep)
{
	fprintf((FILE *)filep, "%s\n", linep);
        return 0;
}

extern long cm_CloseCellFile(cm_configFile_t *filep)
{
	char wdir[256];
        char sdir[256];
        long code;
        long closeCode;
        int tlen;
        char *afsconf_path;

	closeCode = fclose((FILE *)filep);

#if !defined(DJGPP) && !defined(AFS_WIN95_ENV)
	code = GetWindowsDirectory(wdir, sizeof(wdir));
        if (code == 0 || code > sizeof(wdir)) return NULL;
        
	/* add trailing backslash, if required */
        tlen = strlen(wdir);
        if (wdir[tlen-1] != '\\') strcat(wdir, "\\");
#else
#ifdef DJGPP
        strcpy(wdir,cm_confDir);
#else
        afsconf_path = getenv("AFSCONF");
        if (!afsconf_path)
          strcpy(wdir, AFSDIR_CLIENT_ETC_DIRPATH);
        else
          strcpy(wdir, afsconf_path);
#endif /* !DJGPP */
        strcat(wdir,"/");
#endif /* DJGPP || WIN95 */

        strcpy(sdir, wdir);

	if (closeCode != 0) {
		/* something went wrong, preserve original database */
                strcat(wdir, "afsdcel2.ini");
                unlink(wdir);
                return closeCode;
        }

        strcat(wdir, AFS_CELLSERVDB);
        strcat(sdir, "afsdcel2.ini");	/* new file */
        
        unlink(wdir);			/* delete old file */
        
        code = rename(sdir, wdir);	/* do the rename */
        
        if (code) code = errno;
        
        return code;
}
