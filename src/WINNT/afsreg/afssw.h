/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSSW_H_
#define AFSSW_H_

/* Functions for accessing AFS software configuration information. */

#ifdef __cplusplus
extern "C" {
#endif

extern int
afssw_GetServerInstallDir(char **bufPP);

extern int
afssw_GetClientCellServDBDir(char **bufPP);

extern int
afssw_GetClientCellDir(char **bufPP);

extern int
afssw_GetClientCellName(char **bufPP);

extern int
afssw_SetClientCellName(const char *cellName);

extern int
afssw_GetServerVersion(unsigned *major,
		       unsigned *minor,
		       unsigned *patch);

extern int
afssw_GetClientVersion(unsigned *major,
		       unsigned *minor,
		       unsigned *patch);

#ifdef __cplusplus
};
#endif

#endif /* AFSSW_H_ */
