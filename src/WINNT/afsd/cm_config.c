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

#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cm_config.h"

char AFSConfigKeyName[] =
	"SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters";

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

	foundCell = 0;

	code = GetWindowsDirectory(wdir, sizeof(wdir));
        if (code == 0 || code > sizeof(wdir)) return -1;
        
	/* add trailing backslash, if required */
        tlen = strlen(wdir);
        if (wdir[tlen-1] != '\\') strcat(wdir, "\\");

        strcat(wdir, "afsdcell.ini");
        
        tfilep = fopen(wdir, "r");
        if (!tfilep) return -2;
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
                	valuep = strchr(lineBuffer, '#');
			if (valuep == NULL) {
				fclose(tfilep);
				fclose(bestp);
				return -4;
			}
                        valuep++;	/* skip the "#" */
                        valuep += strspn(valuep, " 	"); /* skip SP & TAB */
			if (inRightCell) {
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
                        }
                }	/* a vldb line */
        }		/* while loop processing all lines */
}

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

cm_configFile_t *cm_CommonOpen(char *namep, char *rwp)
{
	char wdir[256];
        long code;
        long tlen;
        FILE *tfilep;

	code = GetWindowsDirectory(wdir, sizeof(wdir));
        if (code == 0 || code > sizeof(wdir)) return NULL;
        
	/* add trailing backslash, if required */
        tlen = strlen(wdir);
        if (wdir[tlen-1] != '\\') strcat(wdir, "\\");

        strcat(wdir, namep);
        
        tfilep = fopen(wdir, rwp);

	return ((cm_configFile_t *) tfilep);        
}

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

        tfilep = cm_CommonOpen("afsdcell.ini", "r");
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

	closeCode = fclose((FILE *)filep);

	code = GetWindowsDirectory(wdir, sizeof(wdir));
        if (code == 0 || code > sizeof(wdir)) return -1;
        
	/* add trailing backslash, if required */
        tlen = strlen(wdir);
        if (wdir[tlen-1] != '\\') strcat(wdir, "\\");
        strcpy(sdir, wdir);

	if (closeCode != 0) {
		/* something went wrong, preserve original database */
                strcat(wdir, "afsdcel2.ini");
                unlink(wdir);
                return closeCode;
        }

        strcat(wdir, "afsdcell.ini");
        strcat(sdir, "afsdcel2.ini");	/* new file */
        
        unlink(wdir);			/* delete old file */
        
        code = rename(sdir, wdir);	/* do the rename */
        
        if (code) code = errno;
        
        return code;
}
