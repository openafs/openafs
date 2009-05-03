/*
 * Copyright (c) 2007
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/*
 * types and constants for compact standalone description of darwin ccache IPC 
 * beware; internal (unstable) interface of Kerberos 65-10; this is no doubt
 * subject to change without notice.
 */

#define kCCacheServerBundleID	"edu.mit.Kerberos.CCacheServer"
#define kCCacheServerPath	"/System/Library/CoreServices/CCacheServer.app/Contents/MacOS/CCacheServer"
#define kInitialDefaultCCacheName	"Initial default ccache"

typedef int		CCIInt32, CCIResult;
typedef unsigned int	CCIUInt32;
typedef char		*CCacheOutName, *CCacheOutPrincipal, *FlattenedOutCredentials;
typedef const char	*CCacheInName, *CCacheInPrincipal, *FlattenedInCredentials;
typedef CCIInt32	CCIPID;
typedef CCIUInt32	CCIObjectID, CCITime;
typedef CCIObjectID	ContextID, CCacheID, CredentialsID;
typedef CredentialsID	*CredentialsIDArray;
typedef CCacheID	*CCacheIDArray;
