/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
void afsi_start();

int afsd_InitCM(char **reasonP);
int afsd_InitDaemons(char **reasonP);
int afsd_InitSMB(char **reasonP, void *aMBfunc);

void afsd_ForceTrace(BOOL flush);

extern char cm_HostName[];

