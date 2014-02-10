/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BOSPROTOTYPES_H_
#define _BOSPROTOTYPES_H_

#ifdef AFS_NT40_ENV
typedef int pid_t;
#endif

#include <rx/rxkad.h>

/* bnode.c */
int bnode_CoreName(struct bnode *abnode, char *acoreName, char *abuffer);
int bnode_GetString(struct bnode *abnode, char *abuffer, afs_int32 alen);
int bnode_GetParm(struct bnode *abnode, afs_int32 aindex, char *abuffer,
		  afs_int32 alen);
int bnode_GetStat(struct bnode *abnode, afs_int32 * astatus);
int bnode_RestartP(struct bnode *abnode);
int bnode_HasCore(struct bnode *abnode);
int bnode_WaitAll(void);
int bnode_SetGoal(struct bnode *abnode, int agoal);
int bnode_SetFileGoal(struct bnode *abnode, int agoal);
int bnode_ApplyInstance(int (*aproc)(struct bnode *, void *), void *arock);
int bnode_Register(char *, struct bnode_ops *, int);
int bnode_DeleteName(char *);
int bnode_Hold(struct bnode *);
int bnode_Release(struct bnode *);
int bnode_Delete(struct bnode *);
int bnode_PendingTimeout(struct bnode *abnode);
void bnode_Int(int asignal);
int bnode_Init(void);
int bnode_FreeTokens(struct bnode_token *alist);
int bnode_ParseLine(char *aline, struct bnode_token **alist);
int bnode_NewProc(struct bnode *abnode, char *aexecString, char *coreName,
		  struct bnode_proc **aproc);
int bnode_StopProc(struct bnode_proc *aproc, int asignal);

/* bosserver.c */
void bozo_Log(const char *format, ... );
int bozo_ReBozo(void);
int WriteBozoFile(char *aname);
int bozo_CreatePidFile(char *ainst, char *aname, pid_t apid);
int bozo_DeletePidFile(char *ainst, char *aname);

/* bosoprocs.c */
int GetRequiredDirPerm(const char *path);
void *bozo_ShutdownAndExit(void *arock /* really int asignal */);
int initBosEntryStats(void);
int DirAccessOK(void);

/* fsbnodeops.c */
char *copystr(char *a);

/* inline functions */
static_inline struct bozo_key *
ktc_to_bozoptr(struct ktc_encryptionKey *key) {
    return (struct bozo_key *)key;
}

#endif
