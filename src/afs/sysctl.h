/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef AFS_SYSCTL_H
#define AFS_SYSCTL_H

/* top level AFS names */
#define AFS_SC_ALL				0
#define AFS_SC_DARWIN				1

/* AFS_SC_ALL: platform-independent sysctls */
	/* nothing defined */

/* AFS_SC_DARWIN: darwin-specific sysctls */
#define AFS_SC_DARWIN_ALL			0
#define AFS_SC_DARWIN_12			1
#define AFS_SC_DARWIN_13			2
#define AFS_SC_DARWIN_14			3
#define AFS_SC_DARWIN_60			4
#define AFS_SC_DARWIN_70			5
#define AFS_SC_DARWIN_80			6
#define AFS_SC_DARWIN_90			7

/* AFS_SC_DARWIN_ALL: darwin version-independent sysctls */
#define AFS_SC_DARWIN_ALL_REALMODES		1

/* AFS_SC_DARWIN_12: darwin 1.2 sysctls */
	/* nothing defined */

/* AFS_SC_DARWIN_13: darwin 1.3 sysctls */
	/* nothing defined */

/* AFS_SC_DARWIN_14: darwin 1.4 sysctls */
	/* nothing defined */

/* AFS_SC_DARWIN_60: darwin 6.x sysctls */
	/* nothing defined */

/* AFS_SC_DARWIN_70: darwin 7.x sysctls */
	/* nothing defined */

/* AFS_SC_DARWIN_80: darwin 8.x sysctls */
	/* nothing defined */

/* AFS_SC_DARWIN_90: darwin 9.x sysctls */
	/* nothing defined */

#endif /* AFS_SYSCTL_H */
