/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

void afsi_start();

#ifndef DJGPP
int afsd_InitCM(char **reasonP);
int afsd_InitSMB(char **reasonP, void *aMBfunc);

void GenerateMiniDump(PEXCEPTION_POINTERS ep);
#else /* DJGPP */
int afsd_InitCM(char **reasonP, struct cmd_syndesc *as, char *arock);
int afsd_InitSMB(char **reasonP);
#endif /* !DJGPP */
int afsd_InitDaemons(char **reasonP);
int afsd_ShutdownCM(void);
void afsd_ForceTrace(BOOL flush);
void afsd_SetUnhandledExceptionFilter();

extern char cm_HostName[];
extern char cm_NetbiosName[];


