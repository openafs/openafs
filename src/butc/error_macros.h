/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define ERROR_EXIT(evalue)                                      \
	{                                                       \
            code = evalue;                                      \
            goto error_exit;                                    \
        }

#define ERROR_EXIT2(evalue)                                     \
	{                                                       \
            code = evalue;                                      \
            goto error_exit2;                                   \
        }

#define ABORT_EXIT(evalue)                                      \
	{                                                       \
            code = evalue;                                      \
            goto abort_exit;                                    \
        }

/*need conversion to varargs*//*extern void ELog(afs_int32 task, char *str, char *a, char *b, char *c, char *d, char *e, char *f, char *g, char *h, char *i, char *j);
extern void ErrorLog(int debug, afs_int32 task, afs_int32 error1, afs_int32 error2, char *str, char *a, char *b, char *c, char *d, char *e, char *f, char *g, char *h, char *i, char *j);
extern void TLog(afs_int32 task, char *str, char *a, char *b, char *c, char *d, char *e, char *f, char *g, char *h, char *i, char *j);
extern void TapeLog(int debug, afs_int32 task, afs_int32 error1, afs_int32 error2, char *str, char *a, char *b, char *c, char *d, char *e, char *f, char *g, char *h, char *i, char *j);*/

extern void FreeNode(afs_int32 taskID);
extern void CreateNode(struct dumpNode **newNode);
extern void LeaveDeviceQueue(struct deviceSyncNode *devLatch);
extern void EnterDeviceQueue(struct deviceSyncNode *devLatch);
extern Date ExpirationDate(afs_int32 dumpid);
extern void InitNodeList(afs_int32 portOffset);

/* bucoord/status.c */
extern void clearStatus(afs_uint32 taskId, afs_uint32 flags);
extern void setStatus(afs_uint32 taskId, afs_uint32 flags);
