/*
 * Copyright (c) 2005, 2006, 2007
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/* cache manager property list */

#ifndef AFS_CM_PROPERTIES_H
#define AFS_CM_PROPERTIES_H

/* Initialize properties string table */
int afs_InitProperties();

/* Add a property--called by subsystems during initialization */
int afs_AddProperty(const char* key, const char* value);

/* Lookup property value by key */
const char* afs_GetProperty(const char* key);

/* Format a buffer with output of matching properties.  
 *  On return, qLen is the length of this buffer, which must be freed 
 * by the caller */
char* afs_GetProperties(const char* qStr, int qStrlen, /* out */ afs_int32 *qLen);

#endif /* AFS_CM_PROPERTIES_H */
