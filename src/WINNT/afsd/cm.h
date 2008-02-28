/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CM_H_ENV__
#define __CM_H_ENV__ 1

#ifndef AFS_PTHREAD_ENV
#define AFS_PTHREAD_ENV 1
#endif
#include <rx/rx.h>
#include <afs/vldbint.h>
#include <afs/afsint.h>

#define CM_DEFAULT_CALLBACKPORT         7001

/* common flags to many procedures */
#define CM_FLAG_CREATE		1		/* create entry */
#define CM_FLAG_CASEFOLD	2		/* fold case in namei, lookup, etc. */
#define CM_FLAG_EXCLUSIVE	4		/* create exclusive */
#define CM_FLAG_FOLLOW		8		/* follow symlinks, even at the end (namei) */
#define CM_FLAG_8DOT3		0x10		/* restrict to 8.3 name */
#define CM_FLAG_NOMOUNTCHASE	0x20		/* don't follow mount points */
#define CM_FLAG_DIRSEARCH	0x40		/* for directory search */
#define CM_FLAG_CHECKPATH	0x80		/* Path instead of File */
#define CM_FLAG_NOPROBE         0x100           /* For use with cm_GetCellxxx - do not probe server status */
#define CM_FLAG_DFS_REFERRAL    0x200           /* The request is a DFS Referral - the last char of the lookup name may be missing */

/* error codes */
#define CM_ERROR_BASE			0x66543200
#define CM_ERROR_NOSUCHCELL		(CM_ERROR_BASE+0)
#define CM_ERROR_NOSUCHVOLUME		(CM_ERROR_BASE+1)
#define CM_ERROR_TIMEDOUT		(CM_ERROR_BASE+2)
#define CM_ERROR_RETRY			(CM_ERROR_BASE+3)
#define CM_ERROR_NOACCESS		(CM_ERROR_BASE+4)
#define CM_ERROR_NOSUCHFILE		(CM_ERROR_BASE+5)
#define CM_ERROR_STOPNOW		(CM_ERROR_BASE+6)
#define CM_ERROR_TOOBIG			(CM_ERROR_BASE+7)
#define CM_ERROR_INVAL			(CM_ERROR_BASE+8)
#define CM_ERROR_BADFD			(CM_ERROR_BASE+9)
#define CM_ERROR_BADFDOP		(CM_ERROR_BASE+10)
#define CM_ERROR_EXISTS			(CM_ERROR_BASE+11)
#define CM_ERROR_CROSSDEVLINK		(CM_ERROR_BASE+12)
#define CM_ERROR_BADOP			(CM_ERROR_BASE+13)
#define CM_ERROR_BADPASSWORD            (CM_ERROR_BASE+14)
#define CM_ERROR_NOTDIR			(CM_ERROR_BASE+15)
#define CM_ERROR_ISDIR			(CM_ERROR_BASE+16)
#define CM_ERROR_READONLY		(CM_ERROR_BASE+17)
#define CM_ERROR_WOULDBLOCK		(CM_ERROR_BASE+18)
#define CM_ERROR_QUOTA			(CM_ERROR_BASE+19)
#define CM_ERROR_SPACE			(CM_ERROR_BASE+20)
#define CM_ERROR_BADSHARENAME		(CM_ERROR_BASE+21)
#define CM_ERROR_BADTID			(CM_ERROR_BASE+22)
#define CM_ERROR_UNKNOWN		(CM_ERROR_BASE+23)
#define CM_ERROR_NOMORETOKENS		(CM_ERROR_BASE+24)
#define CM_ERROR_NOTEMPTY		(CM_ERROR_BASE+25)
#define CM_ERROR_USESTD			(CM_ERROR_BASE+26)
#define CM_ERROR_REMOTECONN		(CM_ERROR_BASE+27)
#define CM_ERROR_ATSYS			(CM_ERROR_BASE+28)
#define CM_ERROR_NOSUCHPATH		(CM_ERROR_BASE+29)
#define CM_ERROR_CLOCKSKEW		(CM_ERROR_BASE+31)
#define CM_ERROR_BADSMB			(CM_ERROR_BASE+32)
#define CM_ERROR_ALLBUSY		(CM_ERROR_BASE+33)
#define CM_ERROR_NOFILES		(CM_ERROR_BASE+34)
#define CM_ERROR_PARTIALWRITE		(CM_ERROR_BASE+35)
#define CM_ERROR_NOIPC			(CM_ERROR_BASE+36)
#define CM_ERROR_BADNTFILENAME		(CM_ERROR_BASE+37)
#define CM_ERROR_BUFFERTOOSMALL		(CM_ERROR_BASE+38)
#define CM_ERROR_RENAME_IDENTICAL	(CM_ERROR_BASE+39)
#define CM_ERROR_ALLOFFLINE             (CM_ERROR_BASE+40)
#define CM_ERROR_AMBIGUOUS_FILENAME     (CM_ERROR_BASE+41)
#define CM_ERROR_BADLOGONTYPE	        (CM_ERROR_BASE+42)
#define CM_ERROR_GSSCONTINUE            (CM_ERROR_BASE+43)
#define CM_ERROR_TIDIPC                 (CM_ERROR_BASE+44)
#define CM_ERROR_TOO_MANY_SYMLINKS      (CM_ERROR_BASE+45)
#define CM_ERROR_PATH_NOT_COVERED       (CM_ERROR_BASE+46)
#define CM_ERROR_LOCK_CONFLICT          (CM_ERROR_BASE+47)
#define CM_ERROR_SHARING_VIOLATION      (CM_ERROR_BASE+48)
#define CM_ERROR_ALLDOWN                (CM_ERROR_BASE+49)
#define CM_ERROR_TOOFEWBUFS		(CM_ERROR_BASE+50)
#define CM_ERROR_TOOMANYBUFS		(CM_ERROR_BASE+51)
#define CM_ERROR_BAD_LEVEL	        (CM_ERROR_BASE+52)
#define CM_ERROR_NOT_A_DFSLINK          (CM_ERROR_BASE+53)
#define CM_ERROR_INEXACT_MATCH          (CM_ERROR_BASE+54)
#define CM_ERROR_BPLUS_NOMATCH          (CM_ERROR_BASE+55)
#define CM_ERROR_EAS_NOT_SUPPORTED      (CM_ERROR_BASE+56)
#define CM_ERROR_RANGE_NOT_LOCKED       (CM_ERROR_BASE+57)
#define CM_ERROR_NOSUCHDEVICE           (CM_ERROR_BASE+58)
#define CM_ERROR_LOCK_NOT_GRANTED       (CM_ERROR_BASE+59)

/* Used by cm_FollowMountPoint and cm_GetVolumeByName */
#define RWVOL	0
#define ROVOL	1
#define BACKVOL	2
#endif /*  __CM_H_ENV__ */
