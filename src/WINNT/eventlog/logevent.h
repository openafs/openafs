/* Copyright (C) 1998, 1999  Transarc Corporation.  All rights reserved.
 *
 */

#ifndef TRANSARC_LOGEVENT_H
#define TRANSARC_LOGEVENT_H

#define AFSEVT_MAXARGS  16   /* max number of text insertion strings */

extern int
ReportErrorEventAlt(unsigned int eventId, int status, char *str, ...);

extern int
ReportWarningEventAlt(unsigned int eventId, int status, char *str, ...);

extern int
ReportInformationEventAlt(unsigned int eventId, char *str, ...);

#endif /* TRANSARC_LOGEVENT_H */
