/* Copyright (C) 1998  Transarc Corporation.  All rights reserved.
 *
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
afssw_GetClientInstallDir(char **bufPP);

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
