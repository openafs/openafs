/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_LOGEVENT_H
#define OPENAFS_LOGEVENT_H

#define AFSEVT_MAXARGS  16   /* max number of text insertion strings */

extern int
ReportErrorEventAlt(unsigned int eventId, int status, char *str, ...);

extern int
ReportWarningEventAlt(unsigned int eventId, int status, char *str, ...);

extern int
ReportInformationEventAlt(unsigned int eventId, char *str, ...);

#endif /* OPENAFS_LOGEVENT_H */
